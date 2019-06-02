/*
 * Copyright (c) 2018 naehrwert
 *
 * Copyright (c) 2018-2019 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "config/config.h"
#include "gfx/di.h"
#include "gfx/gfx.h"
#include "gfx/tui.h"
#include "hos/hos.h"
#include "hos/secmon_exo.h"
#include "hos/sept.h"
#include "ianos/ianos.h"
#include "libs/compr/blz.h"
#include "libs/fatfs/ff.h"
#include "mem/heap.h"
#include "mem/sdram.h"
#include "power/max77620.h"
#include "rtc/max77620-rtc.h"
#include "soc/fuse.h"
#include "soc/hw_init.h"
#include "soc/i2c.h"
#include "soc/t210.h"
#include "soc/uart.h"
#include "storage/nx_emmc.h"
#include "storage/sdmmc.h"
#include "utils/btn.h"
#include "utils/dirlist.h"
#include "utils/list.h"
#include "utils/util.h"
#include "keys/keys.h"

#include "frontend/fe_emmc_tools.h"
#include "frontend/fe_tools.h"
#include "frontend/fe_info.h"

//TODO: ugly.
sdmmc_t sd_sdmmc;
sdmmc_storage_t sd_storage;
FATFS sd_fs;
static bool sd_mounted;
u8 folder;
extern char *file_browser();

#ifdef MENU_LOGO_ENABLE
u8 *Kc_MENU_LOGO;
#endif //MENU_LOGO_ENABLE

hekate_config h_cfg;
boot_cfg_t __attribute__((section ("._boot_cfg"))) b_cfg;
const volatile ipl_ver_meta_t __attribute__((section ("._ipl_version"))) ipl_ver = {
	.magic = BL_MAGIC,
	.version = (BL_VER_MJ + '0') | ((BL_VER_MN + '0') << 8) | ((BL_VER_HF + '0') << 16),
	.rsvd0 = 0,
	.rsvd1 = 0
};

bool sd_mount()
{
	if (sd_mounted)
		return true;

	if (!sdmmc_storage_init_sd(&sd_storage, &sd_sdmmc, SDMMC_1, SDMMC_BUS_WIDTH_4, 11))
	{
		EPRINTF("Init SD card fail.\nCard missing or reader error.");
	}
	else
	{
		int res = 0;
		res = f_mount(&sd_fs, "", 1);
		if (res == FR_OK)
		{
			sd_mounted = 1;
			return true;
		}
		else
		{
			EPRINTFARGS("Mount fail (FatFS Error %d).\n", res);
		}
	}

	return false;
}

void sd_unmount()
{
	if (sd_mounted)
	{
		f_mount(NULL, "", 1);
		sdmmc_storage_end(&sd_storage);
		sd_mounted = false;
	}
}

void *sd_file_read(const char *path, u32 *fsize)
{
	FIL fp;
	if (f_open(&fp, path, FA_READ) != FR_OK)
		return NULL;

	u32 size = f_size(&fp);
	if (fsize)
		*fsize = size;

	void *buf = malloc(size);

	if (f_read(&fp, buf, size, NULL) != FR_OK)
	{
		free(buf);
		f_close(&fp);

		return NULL;
	}

	f_close(&fp);

	return buf;
}

int sd_save_to_file(void *buf, u32 size, const char *filename)
{
	FIL fp;
	u32 res = 0;
	res = f_open(&fp, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res)
	{
		EPRINTFARGS("Error (%d) creating file\n%s.\n", res, filename);
		return 1;
	}

	f_write(&fp, buf, size, NULL);
	f_close(&fp);

	return 0;
}

void emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage)
{
	sdmmc_storage_t storage2;
	bool init_done = false;
	
	u32 sub_dir_len = strlen(sub_dir);   // Can be a null-terminator.
	u32 filename_len = strlen(filename); // Can be a null-terminator.
	
	if (folder == 0){
	memcpy(path, "safe", 5);
	goto start;
	} else if (folder != 0 && folder <= 10) { 
	memcpy(path, "backup", 7);
	f_mkdir(path);
	memcpy(path + strlen(path), "/", 2);
	memcpy(path + strlen(path), "BACKUP_", 8);
	}
		if (folder == 1){
			memcpy(path + strlen(path), "1", 2);
		} else if (folder == 2){
			memcpy(path + strlen(path), "2", 2);
		} else if (folder == 3){
			memcpy(path + strlen(path), "3", 2);
		} else if (folder == 4){
			memcpy(path + strlen(path), "4", 2);
		} else if (folder == 5){
			memcpy(path + strlen(path), "5", 2);
		}/* else if (folder == 6){
			memcpy(path + strlen(path), "6", 2);
		} else if (folder == 7){
			memcpy(path + strlen(path), "7", 2);
		} else if (folder == 8){
			memcpy(path + strlen(path), "8", 2);
	}*/ else if (folder == 11){
	memcpy(path, "screenshot", 11);
	} else if (folder == 12) {memcpy(path, "", 1);}
start:	
	f_mkdir(path);
	
	memcpy(path + strlen(path), sub_dir, sub_dir_len + 1);
	if (sub_dir_len)
		f_mkdir(path);
	memcpy(path + strlen(path), "/", 2);
	memcpy(path + strlen(path), filename, filename_len + 1);
	
	if (init_done)
		sdmmc_storage_end(&storage2);
	
}

void check_power_off_from_hos()
{
	// Power off on AutoRCM wakeup from HOS shutdown. For modchips/dongles.
	u8 hosWakeup = i2c_recv_byte(I2C_5, MAX77620_I2C_ADDR, MAX77620_REG_IRQTOP);
	if (hosWakeup & MAX77620_IRQ_TOP_RTC_MASK)
	{
		sd_unmount();

		// Stop the alarm, in case we injected too fast.
		max77620_rtc_stop_alarm();

		
		power_off();
	}
}

// This is a safe and unused DRAM region for our payloads.
#define RELOC_META_OFF      0x7C
#define PATCHED_RELOC_SZ    0x94
#define PATCHED_RELOC_STACK 0x40007000
#define PATCHED_RELOC_ENTRY 0x40010000
#define EXT_PAYLOAD_ADDR    0xC03C0000
#define RCM_PAYLOAD_ADDR    (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))
#define COREBOOT_ADDR       (0xD0000000 - 0x100000)
#define CBFS_DRAM_EN_ADDR   0x4003e000
#define  CBFS_DRAM_MAGIC    0x4452414D // "DRAM"

void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size)
{
	memcpy((u8 *)payload_src, (u8 *)IPL_LOAD_ADDR, PATCHED_RELOC_SZ);

	volatile reloc_meta_t *relocator = (reloc_meta_t *)(payload_src + RELOC_META_OFF);

	relocator->start = payload_dst - ALIGN(PATCHED_RELOC_SZ, 0x10);
	relocator->stack = PATCHED_RELOC_STACK;
	relocator->end   = payload_dst + payload_size;
	relocator->ep    = payload_dst;

	if (payload_size == 0x7000)
	{
		memcpy((u8 *)(payload_src + ALIGN(PATCHED_RELOC_SZ, 0x10)), (u8 *)COREBOOT_ADDR, 0x7000); //Bootblock
		*(vu32 *)CBFS_DRAM_EN_ADDR = CBFS_DRAM_MAGIC;
	}
}

bool is_ipl_updated(void *buf)
{
	ipl_ver_meta_t *update_ft = (ipl_ver_meta_t *)(buf + PATCHED_RELOC_SZ + sizeof(boot_cfg_t));	

	if (update_ft->magic == ipl_ver.magic)
	{
		if (byte_swap_32(update_ft->version) <= byte_swap_32(ipl_ver.version))
			return true;
		return false;
		
	}
	else
		return true;
}

int launch_payload(char *path, bool update)
{
	if (!update)
		gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);
	if (!path)
		return 1;

	if (sd_mount())
	{
		FIL fp;
		if (f_open(&fp, path, FA_READ))
		{
			EPRINTFARGS("Payload missing!\n(%s)", path);
			sd_unmount();

			return 1;
		}

		// Read and copy the payload to our chosen address
		void *buf;
		u32 size = f_size(&fp);

		if (size < 0x30000)
			buf = (void *)RCM_PAYLOAD_ADDR;
		else
			buf = (void *)COREBOOT_ADDR;

		if (f_read(&fp, buf, size, NULL))
		{
			f_close(&fp);
			sd_unmount();

			return 1;
		}

		f_close(&fp);
		free(path);

		if (update && is_ipl_updated(buf))
			return 1;

		sd_unmount();

		if (size < 0x30000)
		{
			if (update)
				memcpy((u8 *)(RCM_PAYLOAD_ADDR + PATCHED_RELOC_SZ), &b_cfg, sizeof(boot_cfg_t)); // Transfer boot cfg.
			else
				reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));

			reconfig_hw_workaround(false, byte_swap_32(*(u32 *)(buf + size - sizeof(u32))));
		}
		else
		{
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);
			reconfig_hw_workaround(true, 0);
		}

		void (*ext_payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;
		void (*update_ptr)() = (void *)RCM_PAYLOAD_ADDR;

		// Launch our payload.
		if (!update)
			(*ext_payload_ptr)();
		else
		{
			EMC(EMC_SCRATCH0) |= EMC_HEKA_UPD;
			(*update_ptr)();
		}
	}

	return 1;
}

void auto_launch_argon()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "argon/argon.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("argon/argon.bin", false);
			}

		}
	}

void auto_launch_payload_bin()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "payload.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("payload.bin", false);
			}

		}
	}

void auto_launch_numbered_payload()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "payload1.bin", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("payload1.bin", false);
			}

	}
}

void auto_launch_dummy_payload()
{
	FIL fp;
	if (sd_mount())
		{
			if (f_open(&fp, "bootloader/payloads/dummy", FA_READ))
				return;
			else
			{
				f_close(&fp);
				launch_payload("bootloader/payloads/dummy", false);
			}

		}
	}
	
	int read_strap_info()
	{
	FIL fp;
	bool read_info = false;
	
	char path[35] = "bootloader/fusee/straps_info.txt";
	if (sd_mount())
	{
		f_unlink("intentionally_left_blank_for_UF2");
		
		if (f_open(&fp, path, FA_READ))
		{
			sd_unmount();
			read_info = false;
			goto out;
		}

		u32 size = f_size(&fp);
		char* strap_buffer = malloc(size + 1);
		memset(strap_buffer, 0, size + 1);
		if (size != 339) {
			EPRINTF ("Strap info incorrect\n\n");
			read_info = false;
			goto out;
		}

		if (f_read(&fp, strap_buffer, size, NULL))
		{
			f_close(&fp);
			sd_unmount();
			EPRINTF ("Strap info read fail.\n\n");
			read_info = false;
			goto out;
		}
		const char* strap_info = strap_buffer;
		free(strap_buffer);
		f_close(&fp);

	gfx_printf("%k%s%k\n", 0xFF00FF00, strap_info, 0xFFFFFFFF);
	
	read_info = true;
	}
out:
sd_unmount();
if (read_info){	
	return 1;
	} else return 0;
	}

extern void ini_list_launcher();
extern void prev_menu();

void launch_tools(const u8 type)
{
	char *path = malloc(256);
	char *file_sec = malloc(256);

	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);
	
if (sd_mount()){	
	if (type == 1)
	{	
	char *file_sec = file_browser();
	memcpy (path + 0, file_sec, (strlen(file_sec) + 1));
	u8 str_compared_bin = strcmp (path + strlen (path) - 4, ".bin");
	u8 str_compared_ini = strcmp (path + strlen (path) - 4, ".ini");
	if (!str_compared_bin){
		if (launch_payload(path, false))
				{
					EPRINTF("Failed to launch payload.");
					free(file_sec);
					free(path);
				}
		sd_unmount();
		free(file_sec);
		free(path);
		msleep(3000);
		return;
	} else if (!str_compared_ini){
		ini_list_launcher(true, path);
		return;
	} else
		{
			gfx_printf("%k\n No valid files here! Press a key.%k", 0xFFFFFF00, 0xFFFFFFFF);
			btn_wait();
			free(file_sec);
			free(path);
			prev_menu();
		}
	}
	
	if (type == 2 || type == 3)
		
		{
			if (type == 2){
			file_sec = file_browser();
			}
			gfx_clear_black(0x00);
			gfx_con_setpos(0, 0);
			u8 res = create_config_entry();
			if (res){
				gfx_printf ("Failed creating config entry.\n\nPress any key.");
				return;
			}
			FIL fp;
		
			if (f_open(&fp, "bootloader/hekate_ipl.ini", FA_WRITE | FA_READ) != FR_OK){
				gfx_printf ("Failed opening hekate_ipl.ini\n\nPress any key.");
				return;
				}
			f_lseek(&fp, f_size(&fp));
			if (type == 2){
			f_puts("[", &fp);
			f_puts(file_sec, &fp);
			f_puts("]\n", &fp);
			f_puts("payload=", &fp);
			f_puts(file_sec, &fp);
			gfx_printf ("Added:\n\n%s\n\nto hekate_ipl.ini\n\nPress any key.", file_sec);
			} else if (type == 3){
			f_puts("[Stock]\n", &fp);
			f_puts("fss0=atmosphere/fusee-secondary.bin\n", &fp);
			f_puts("stock=1\n", &fp);
			gfx_printf ("Done. Press any key.", file_sec);
			}
			btn_wait();
			f_close(&fp);
			}
			else {
			char *file_sec = file_browser();
			ianos_loader(true, file_sec, DRAM_LIB, NULL);
			}
		}
	btn_wait();
	return;
}

void type1() {launch_tools(1); }
void type2() {launch_tools(2); }
void type3() {launch_tools(3); }

void ini_list_launcher(bool ini_selected, char* path)
{
	u8 max_entries = 61;
	char *payload_path = NULL;

	ini_sec_t *cfg_tmp = NULL;
	ini_sec_t *cfg_sec = NULL;
	LIST_INIT(ini_list_sections);

	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		if (!ini_selected){
		ini_parse(&ini_list_sections, "bootloader/ini", true);
		} else {
		ini_parse(&ini_list_sections, path, false);
		}
			// Build configuration menu.
			ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 3));
			ments[0].type = MENT_BACK;
			ments[0].caption = "Back";
			ments[1].type = MENT_CHGLINE;

			u32 i = 2;
			LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, &ini_list_sections, link)
			{
				if (!strcmp(ini_sec->name, "config") ||
					ini_sec->type == INI_COMMENT || ini_sec->type == INI_NEWLINE)
					continue;
				ments[i].type = ini_sec->type;
				ments[i].caption = ini_sec->name;
				ments[i].data = ini_sec;
				if (ini_sec->type == MENT_CAPTION)
					ments[i].color = ini_sec->color;
				i++;

				if ((i - 1) > max_entries)
					break;
			}
			if (i > 2)
			{
				memset(&ments[i], 0, sizeof(ment_t));
				menu_t menu = {
					ments, "Launch linux / ini entry", 0, 0
				};

				cfg_tmp = (ini_sec_t *)tui_do_menu(&menu);

				if (cfg_tmp)
				{
					u32 non_cfg = 1;
					for (int j = 2; j < i; j++)
					{
						if (ments[j].type != INI_CHOICE)
							non_cfg++;

						if (ments[j].data == cfg_tmp)
						{
							b_cfg.boot_cfg = BOOT_CFG_FROM_LAUNCH;
							b_cfg.autoboot = j - non_cfg;
							b_cfg.autoboot_list = 1;

							break;
						}
					}
				}

				payload_path = ini_check_payload_section(cfg_tmp);

				if (cfg_tmp && !payload_path)
					check_sept();

				cfg_sec = ini_clone_section(cfg_tmp);

				if (!cfg_sec)
				{
					free(ments);
					ini_free(&ini_list_sections);

					return;
				}
			}
			else 
				EPRINTF("No extra configs found.");
			free(ments);
			ini_free(&ini_list_sections);
			
	}	
		else
			EPRINTF("Could not find any ini\nin bootloader/ini!");
	
	

	if (!cfg_sec)
		goto out;

	if (payload_path)
	{
		ini_free_section(cfg_sec);
		if (launch_payload(payload_path, false))
		{
			EPRINTF("Failed to launch payload.");
			free(payload_path);
		}
	}
	else if (!hos_launch(cfg_sec))
	{
		EPRINTF("Failed to launch firmware.");
		btn_wait();
	}

out:
	ini_free_section(cfg_sec);

	btn_wait();
}

void ini_launch(){
	ini_list_launcher(false, NULL);
}

void launch_firmware()
{
	u8 max_entries = 61;
	char *payload_path = NULL;

	ini_sec_t *cfg_tmp = NULL;
	ini_sec_t *cfg_sec = NULL;
	LIST_INIT(ini_sections);

	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		if (ini_parse(&ini_sections, "bootloader/hekate_ipl.ini", false))
		{
			// Build configuration menu.
			ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 1));
			ments[0].type = MENT_BACK;
			ments[0].caption = "Back";
			u32 i = 1;
			LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, &ini_sections, link)
			{
				if (!strcmp(ini_sec->name, "config") ||
					ini_sec->type == INI_COMMENT || ini_sec->type == INI_NEWLINE)
					continue;
				ments[i].type = ini_sec->type;
				ments[i].caption = ini_sec->name;
				ments[i].data = ini_sec;
				if (ini_sec->type == MENT_CAPTION)
					ments[i].color = ini_sec->color;
				i++;

				if (i > max_entries)
					break;
			}
			if (i < 6)
			{
				ments[i].type = MENT_CAPTION;
				ments[i].caption = "No main configs found...";
				ments[i].color = 0xFFFFDD00;
				i++;
			}
			memset(&ments[i], 0, sizeof(ment_t));
			menu_t menu = {
				ments, "Launch configurations", 0, 0
			};

			cfg_tmp = (ini_sec_t *)tui_do_menu(&menu);

			if (cfg_tmp)
			{
				u8 non_cfg = 4;
				for (int j = 5; j < i; j++)
				{
					if (ments[j].type != INI_CHOICE)
						non_cfg++;
					if (ments[j].data == cfg_tmp)
					{
						b_cfg.boot_cfg = BOOT_CFG_FROM_LAUNCH;
						b_cfg.autoboot = j - non_cfg;
						b_cfg.autoboot_list = 0;

						break;
					}
				}
			}

			payload_path = ini_check_payload_section(cfg_tmp);

			if (cfg_tmp && !payload_path)
				check_sept();

			cfg_sec = ini_clone_section(cfg_tmp);
			if (!cfg_sec)
			{
				free(ments);
				ini_free(&ini_sections);
				sd_unmount();
				return;
			}

			free(ments);
			ini_free(&ini_sections);
		}
		else
			EPRINTF("Could not open hekate_ipl.ini.\n");
	}

	if (!cfg_sec)
	{
		gfx_puts("\n[POWER] = attempt boot.\n\n[VOL-] = main menu.\n\n[VOL+] = make new ini\n");
		gfx_printf("\nUsing default launch configuration...\n\n\n");

		u32 btn = btn_wait();
		if (btn & BTN_VOL_DOWN){
			goto out;
		}
		if (btn & BTN_VOL_UP) {
			gfx_printf("\nPlease wait... Making new INI\n\n\n");
			create_config_entry();
		}
		gfx_printf("\nUsing default launch configuration...\n\n\n");
	}

	if (payload_path && (!(strcmp(payload_path + strlen(payload_path) - 4, ".bin"))))
	{
		ini_free_section(cfg_sec);
		if (launch_payload(payload_path, false))
		{
			EPRINTF("Failed to launch payload.");
			free(payload_path);
		}
	} else if (payload_path && (!(strcmp(payload_path + strlen(payload_path) - 4, ".ini"))))
	{
		ini_list_launcher(true, payload_path);	
	}
	else if (!hos_launch(cfg_sec))
		EPRINTF("Failed to launch firmware.");

out:
	ini_free_section(cfg_sec);
	sd_unmount();

	btn_wait();
}

void auto_launch_firmware()
{

	u8 *BOOTLOGO = NULL;
	char *payload_path = NULL;
	u32 btn = 0;

	struct _bmp_data
	{
		u32 size;
		u32 size_x;
		u32 size_y;
		u32 offset;
		u32 pos_x;
		u32 pos_y;
	};

	struct _bmp_data bmpData;
	bool bootlogoFound = false;
	char *bootlogoCustomEntry = NULL;

	if (!(b_cfg.boot_cfg & BOOT_CFG_FROM_LAUNCH))
		gfx_con.mute = true;

	ini_sec_t *cfg_sec = NULL;
	LIST_INIT(ini_sections);
	LIST_INIT(ini_list_sections);

	if (sd_mount())
	{
		if (f_stat("bootloader/hekate_ipl.ini", NULL))
			create_config_entry();

		if (ini_parse(&ini_sections, "bootloader/hekate_ipl.ini", false))
		{
			u32 configEntry = 0;
			u32 boot_entry_id = 0;

			// Load configuration.
			LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, &ini_sections, link)
			{
				// Skip other ini entries for autoboot.
				if (ini_sec->type == INI_CHOICE)
				{
					if (!strcmp(ini_sec->name, "config"))
					{
						configEntry = 1;
						LIST_FOREACH_ENTRY(ini_kv_t, kv, &ini_sec->kvs, link)
						{
							if (!strcmp("autoboot", kv->key))
								h_cfg.autoboot = atoi(kv->val);
							else if (!strcmp("autoboot_list", kv->key))
								h_cfg.autoboot_list = atoi(kv->val);
							else if (!strcmp("bootwait", kv->key))
								h_cfg.bootwait = atoi(kv->val);
							else if (!strcmp("verification", kv->key))
								h_cfg.verification = atoi(kv->val);
							else if (!strcmp("backlight", kv->key))
								h_cfg.backlight = atoi(kv->val);
							else if (!strcmp("autohosoff", kv->key))
								h_cfg.autohosoff = atoi(kv->val);
							else if (!strcmp("autonogc", kv->key))
								h_cfg.autonogc = atoi(kv->val);
						}
						boot_entry_id++;

						// Override autoboot, otherwise save it for a possbile sept run.
						if (b_cfg.boot_cfg & BOOT_CFG_AUTOBOOT_EN)
						{
							h_cfg.autoboot = b_cfg.autoboot;
							h_cfg.autoboot_list = b_cfg.autoboot_list;
						}
						else
						{
							b_cfg.autoboot = h_cfg.autoboot;
							b_cfg.autoboot_list = h_cfg.autoboot_list;
						}

						continue;
					}

					if (h_cfg.autoboot == boot_entry_id && configEntry)
					{
						cfg_sec = ini_clone_section(ini_sec);
						LIST_FOREACH_ENTRY(ini_kv_t, kv, &cfg_sec->kvs, link)
						{
							if (!strcmp("logopath", kv->key))
								bootlogoCustomEntry = kv->val;
						}
						break;
					}
					boot_entry_id++;
				}
			}

			if (h_cfg.autohosoff && !(b_cfg.boot_cfg & BOOT_CFG_AUTOBOOT_EN))
				check_power_off_from_hos();

			if (h_cfg.autoboot_list)
			{
				ini_free_section(cfg_sec);
				boot_entry_id = 1;
				bootlogoCustomEntry = NULL;

				if (ini_parse(&ini_list_sections, "bootloader/ini", true))
				{
					LIST_FOREACH_ENTRY(ini_sec_t, ini_sec_list, &ini_list_sections, link)
					{
						if (ini_sec_list->type == INI_CHOICE)
						{
							if (!strcmp(ini_sec_list->name, "config"))
								continue;

							if (h_cfg.autoboot == boot_entry_id)
							{
								cfg_sec = ini_clone_section(ini_sec_list);
								LIST_FOREACH_ENTRY(ini_kv_t, kv, &cfg_sec->kvs, link)
								{
									if (!strcmp("logopath", kv->key))
										bootlogoCustomEntry = kv->val;
								}
								break;
							}
							boot_entry_id++;
						}
						
					}

				}

			}

			// Add missing configuration entry.
			if (!configEntry)
				create_config_entry();

			if (!h_cfg.autoboot)
				goto out; // Auto boot is disabled.

			if (!cfg_sec)
				goto out; // No configurations.
		}
		else
			goto out; // Can't load hekate_ipl.ini.
	}
	else
		goto out;

	u8 *bitmap = NULL;
	if (!(b_cfg.boot_cfg & BOOT_CFG_FROM_LAUNCH) && h_cfg.bootwait)
	{
		if (bootlogoCustomEntry) // Check if user set custom logo path at the boot entry.
			bitmap = (u8 *)sd_file_read(bootlogoCustomEntry, NULL);

		if (!bitmap) // Custom entry bootlogo not found, trying default custom one.
			bitmap = (u8 *)sd_file_read("bootloader/bootlogo.bmp", NULL);

		if (bitmap)
		{
			// Get values manually to avoid unaligned access.
			bmpData.size = bitmap[2] | bitmap[3] << 8 |
				bitmap[4] << 16 | bitmap[5] << 24;
			bmpData.offset = bitmap[10] | bitmap[11] << 8 |
				bitmap[12] << 16 | bitmap[13] << 24;
			bmpData.size_x = bitmap[18] | bitmap[19] << 8 |
				bitmap[20] << 16 | bitmap[21] << 24;
			bmpData.size_y = bitmap[22] | bitmap[23] << 8 |
				bitmap[24] << 16 | bitmap[25] << 24;
			// Sanity check.
			if (bitmap[0] == 'B' &&
				bitmap[1] == 'M' &&
				bitmap[28] == 32 && // Only 32 bit BMPs allowed.
				bmpData.size_x <= 720 &&
				bmpData.size_y <= 1280)
			{
				if ((bmpData.size - bmpData.offset) <= 0x400000)
				{
					// Avoid unaligned access from BM 2-byte MAGIC and remove header.
					BOOTLOGO = (u8 *)malloc(0x400000);
					memcpy(BOOTLOGO, bitmap + bmpData.offset, bmpData.size - bmpData.offset);
					free(bitmap);
					// Center logo if res < 720x1280.
					bmpData.pos_x = (720  - bmpData.size_x) >> 1;
					bmpData.pos_y = (1280 - bmpData.size_y) >> 1;
					// Get background color from 1st pixel.
					if (bmpData.size_x < 720 || bmpData.size_y < 1280)
						gfx_clear_color(*(u32 *)BOOTLOGO);

					bootlogoFound = true;
				}
			}
			else
				free(bitmap);
		}

		// Render boot logo.
		if (bootlogoFound)
		{
			gfx_render_bmp_argb((u32 *)BOOTLOGO, bmpData.size_x, bmpData.size_y,
				bmpData.pos_x, bmpData.pos_y);
		}
		free(BOOTLOGO);
	}

	ini_free(&ini_sections);
	if (h_cfg.autoboot_list)
		ini_free(&ini_list_sections);

	if (h_cfg.sept_run){
		if (h_cfg.bootwait){
			display_backlight_brightness(h_cfg.backlight, 1000);
		} else {
		display_backlight_brightness(h_cfg.backlight, 0);
		}
	} else if (h_cfg.bootwait){
		display_backlight_brightness(h_cfg.backlight, 1000);
	}
	// Wait before booting. If VOL- is pressed go into bootloader menu.

	btn = btn_wait_timeout(h_cfg.bootwait * 1000, BTN_VOL_DOWN);

	if (btn & BTN_VOL_DOWN)
			goto out;

	payload_path = ini_check_payload_section(cfg_sec);

	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);
	
	if (payload_path)
	{
		ini_free_section(cfg_sec);
		if (launch_payload(payload_path, false))
			free(payload_path);
	}
	else
	{
		check_sept();
		hos_launch(cfg_sec);
	}

out:
	ini_free(&ini_sections);
	if (h_cfg.autoboot_list)
		ini_free(&ini_list_sections);
	ini_free_section(cfg_sec);

	sd_unmount();
	gfx_con.mute = false;

	b_cfg.boot_cfg &= ~(BOOT_CFG_AUTOBOOT_EN | BOOT_CFG_FROM_LAUNCH);
}

void patched_rcm_protection()
{
	sdmmc_storage_t storage;
	sdmmc_t sdmmc;

	h_cfg.rcm_patched = fuse_check_patched_rcm();

	if (!h_cfg.rcm_patched)
		return;

	// Check if AutoRCM is enabled and protect from a permanent brick.
	if (!sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4))
		return;

	u8 *tempbuf = (u8 *)malloc(0x200);
	sdmmc_storage_set_mmc_partition(&storage, 1);

	u8 corr_mod_byte0;
	int i, sect = 0;
	if ((fuse_read_odm(4) & 3) != 3)
		corr_mod_byte0 = 0xF7;
	else
		corr_mod_byte0 = 0x37;

	for (i = 0; i < 4; i++)
	{
		sect = (0x200 + (0x4000 * i)) / NX_EMMC_BLOCKSIZE;
		sdmmc_storage_read(&storage, sect, 1, tempbuf);

		// If AutoRCM is enabled, disable it.
		if (tempbuf[0x10] != corr_mod_byte0)
		{
			tempbuf[0x10] = corr_mod_byte0;

			sdmmc_storage_write(&storage, sect, 1, tempbuf);
		}
	}

	free(tempbuf);
	sdmmc_storage_end(&storage);
}

void save_to_sdcard(){dump_to_sd(1);}
void nca_to_sdcard(){dump_to_sd(2);}

extern menu_t menu_top;

void home_dir(){
while (true)
				{
				tui_do_menu(&menu_top);
				}
}

extern menu_t menu_launch;

void prev_menu(){
while (true)
				{
				tui_do_menu(&menu_launch);
				}
}

char *file_browser(){
	
u8 max_entries = 61;
	char *filelist = malloc(256);
	char *file_sec = malloc(512);
	char *file_sec_untrimmed = malloc(512);
	char *dir = malloc(512);

	ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 3));

	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		dir = (char *)malloc(256);
		memcpy(dir + 0, "", 1);
		
		for(;;){
			
			
			filelist = dirlist(dir, NULL, false, true);
			u32 i = 0;
			

			if (filelist)
			{
				// Build configuration menu.
				ments[0].type = MENT_HANDLER;
				ments[0].caption = "Main Menu";
				ments[0].handler = home_dir;
				ments[1].type = MENT_HANDLER;
				ments[1].caption = "Back";
				ments[1].handler = prev_menu;
				ments[2].type = MENT_CHGLINE;

				while (true)
				{
					if (i > max_entries || !filelist[i * 256])
						break;
					ments[i + 3].type = INI_CHOICE;
					ments[i + 3].caption = &filelist[i * 256];
					ments[i + 3].data = &filelist[i * 256];

					i++;
				}
			}
						
			if (i > 0)
			{
				memset(&ments[i + 3], 0, sizeof(ment_t));
				menu_t menu = {
						ments,
						"Browse for a file", 0, 0
				};
				//free(file_sec_untrimmed);
				file_sec_untrimmed = (char *)tui_do_menu(&menu);
				u32 trimlength = strlen(file_sec_untrimmed) - 7; //get untrimmed length
				memcpy (file_sec + 0, file_sec_untrimmed, trimlength);
				memcpy (file_sec + trimlength, "\0", 2);//nullterm
				memcpy(dir + strlen(dir), "/", 2);	
				memcpy(dir + strlen(dir), file_sec, strlen (file_sec) + 1);
				trimlength = 0;
					
			}
			
			if (i == 0) break;
			
		}
		
	}
	free (file_sec_untrimmed);
	free (file_sec);
	return dir;
}

void copy_file (char* src_file, char* dst_file){
	DIR dir;
    FIL fsrc, fdst;      
    BYTE buffer[8000000];  //8mb buff 
    FRESULT fr;          
    UINT br, bw;
	FILINFO fno;
	u8 btn = btn_wait_timeout(0, BTN_VOL_DOWN | BTN_VOL_UP);
					if (btn & BTN_VOL_DOWN || btn & BTN_VOL_UP)
						{
							f_close(&fsrc);
							f_close(&fdst);
							gfx_printf ("\nCancelled.");
							msleep(1000);
							goto out;
						}
	fr = f_open(&fsrc, src_file, FA_READ);	//open src
			if (fr)
				{ 
				EPRINTFARGS ("\nStopped. Last file:\n%s\n%s", src_file, dst_file);
				btn_wait(); return;
				} else gfx_printf ("\nSuccess reading src file\n%s", src_file);
		fr = f_unlink(dst_file);
		fr = f_open(&fdst, dst_file, FA_WRITE | FA_CREATE_ALWAYS);
			if (fr)
				{ 
				EPRINTFARGS ("\nFailed reading dest file\n%s", dst_file);
				btn_wait(); return;
				} else gfx_printf ("\nSuccess reading dest file\n%s\n", dst_file);
				
			
			gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);
			u8 spin = 1;
				for (;;) 
				{
					++spin;
					fr = f_read(&fsrc, buffer, sizeof buffer, &br);
					
					if (fr || br == 0) break;
					gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
					switch (spin)
					{
					case 1:
					gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
					gfx_printf ("|");
					break;
					case 2:
					gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
					gfx_printf ("/");
					break;
					case 3:
					gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
					gfx_printf ("-");
					break;
					case 4:
					gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
					gfx_printf ("\\");
					break;
					}
							
					fr = f_write(&fdst, buffer, br, &bw);            
					if (fr || bw < br) break;
					gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
					
					btn = btn_wait_timeout(0, BTN_VOL_DOWN | BTN_VOL_UP);
					if (btn & BTN_VOL_DOWN || btn & BTN_VOL_UP)
						{
							f_close(&fsrc);
							f_close(&fdst);
							gfx_printf ("\nCancelled.");
							msleep(1000);
							break;
							goto out;
						}
					if (spin == 4) spin = 1;
					gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);
					if (gfx_con.savedy >= 1200){
						gfx_clear_black(0x00);
						gfx_con_setpos(0, 0);
						}
				}	
out:					
				f_close(&fsrc);
				f_close(&fdst);
				
	return;
}

void sxos_emunand_setup(const u8 selection){   

if (sd_mount()){  

	char *emunand_folder_src = malloc(64);
	char *emunand_folder_dst = malloc(64);
	char *temp_filename = malloc(64);
	char *temp_src = malloc(64);
	char *temp_dst = malloc(64);
	char *marker_file = malloc(64);
	char *marker = malloc(64); 
	char *current_emunand_folder = malloc(64);
	char *orig_emunand_folder = malloc(64);
	char *marker_buffer = malloc(2);
	char *emunand_part = malloc(8);
	u8 current_emunand = 0;
	u8 src_nand_no = 1;
	u8 dst_nand_no = 2;


	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);
	FRESULT fr; 
	FIL fp;
	UINT br, bw;
	if (selection == 6 || selection == 7){goto setup;} else 
		{
				fr = f_open(&fp,"sxos/emunand/marker", FA_READ);
				if (fr) {EPRINTF("Error opening marker.\n\nSelect Setup folders option first,"); return;}
				fr = f_read(&fp, marker_buffer, 1, &br);
				if (fr) {EPRINTF("Error reading marker"); return;}
				fr = f_close(&fp);
				if (fr) {EPRINTF("Error closing marker"); return;}
				current_emunand = atoi(marker_buffer);
				gfx_printf ("Currently selected slot:%d\n\n", current_emunand);
			//determine home slot for this emunand from marker file
				
				//move marker from default to home slot
				memcpy (current_emunand_folder+0,"sxos/emunand/", 14);
				memcpy (current_emunand_folder+strlen(current_emunand_folder),"marker", 7);
				memcpy (current_emunand_folder+strlen(current_emunand_folder),"\0", 2);
				
				memcpy (orig_emunand_folder+0,"sxos/emunand", 13);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),marker_buffer, 2);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"/", 2);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"marker", 7);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"\0", 2);
				
				fr = (f_rename (current_emunand_folder,orig_emunand_folder));
				gfx_printf("\nMoved success\norig:%s\ncurrent:%s", orig_emunand_folder, current_emunand_folder);
				if (!fr){gfx_printf("\nMoved success");}
				btn_wait();
				
				//move boots from default to home slot
				
				memcpy (current_emunand_folder+0,"sxos/emunand/", 14);
				memcpy (current_emunand_folder+13,"boot0.bin", 10);
				memcpy (current_emunand_folder+strlen(current_emunand_folder),"\0", 2);
				
				memcpy (orig_emunand_folder+0,"sxos/emunand", 13);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),marker_buffer, 2);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"/", 2);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"boot0.bin", 10);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"\0", 2);
				
				fr = (f_rename (current_emunand_folder,orig_emunand_folder));
				if (!fr){gfx_printf("\nMoved success"); btn_wait();}
				
				memcpy (current_emunand_folder+0,"sxos/emunand/", 14);
				memcpy (current_emunand_folder+13,"boot0.bin", 10);
				memcpy (current_emunand_folder+strlen(current_emunand_folder),"\0", 2);
				
				memcpy (orig_emunand_folder+0,"sxos/emunand", 13);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),marker_buffer, 2);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"/", 2);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"boot1.bin", 10);
				memcpy (orig_emunand_folder+strlen(orig_emunand_folder),"\0", 2);
				
				fr = (f_rename (current_emunand_folder,orig_emunand_folder));
				if (!fr){gfx_printf("\nMoved success"); btn_wait();}				
				btn_wait();
				return;
				}
	
setup:

	if (selection == 6){
				
				for (u8 a = 0; a > 7; ++a){
					memcpy (emunand_folder_dst+0,"sxos/emunand", 13);
					if (a != 0){
					itoa((a), temp_filename, 10);
					memcpy (emunand_folder_dst+12,temp_filename, 2);
					}
					fr = f_mkdir(emunand_folder_dst);
					if (!fr) {gfx_printf("Created %s\n", emunand_folder_dst);}
					else {EPRINTFARGS("Skipped %s\n", emunand_folder_dst);}
					btn_wait();
					memcpy (marker_file + 0, emunand_folder_dst, strlen(emunand_folder_dst)+1);
					memcpy (marker_file + strlen(marker_file), "/", 2);
					memcpy (marker_file + strlen(marker_file), "marker" , 7);
					fr = f_open(&fp, marker_file, FA_WRITE | FA_CREATE_ALWAYS);
					itoa((a), marker, 10);
					if (a == 0){
						f_puts("1", &fp);
						f_close(&fp);
					} else {
					f_puts(marker, &fp);
					f_close(&fp);
					}
					if (!fr) gfx_printf("Created marker %d\n", a);
					else EPRINTF("Marker exists. Skipping...");
					btn_wait();
				}
				gfx_printf("Complete. Press any key.\n"); btn_wait();
		
	} else if (selection == 7){
			gfx_printf ("%kSelect slot to copy FROM\n\n[VOL+] Increase slot\n[VOL-] Quit to main menu\n[PWR] Select\n\n%k", 0xFF00FF00, 0xFFFFFFFF);
			gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);
			for(;;){
				gfx_con_setpos(gfx_con.savedx,gfx_con.savedy);
				gfx_printf("EmuNAND selected: %d\n\n", src_nand_no);
				u8 btn = btn_wait();
				if (btn & BTN_VOL_UP) ++src_nand_no;
				if (btn & BTN_VOL_DOWN) home_dir();
				if (btn & BTN_POWER) break;
				if (src_nand_no > 5) src_nand_no = 0;
			}
			gfx_printf ("%k\n\nSelect slot to copy TO\n[VOL+] Increase slot\n[VOL-] Quit to main menu\n[PWR] Select\n\n%k", 0xFF00FF00, 0xFFFFFFFF);
			gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);
			for(;;){
				gfx_con_setpos(gfx_con.savedx,gfx_con.savedy);
				gfx_printf("EmuNAND selected: %d\n\n", dst_nand_no);
				u8 btn = btn_wait();
				if (btn & BTN_VOL_UP) ++dst_nand_no;
				if (btn & BTN_VOL_DOWN) home_dir();
				if (btn & BTN_POWER) break;
				if (dst_nand_no > 5) dst_nand_no = 1;
			}
			if (src_nand_no == dst_nand_no){
				EPRINTF("\n\nSource and Destination slots aree the same!\nNot possible.\n");
				btn_wait();
				return;
			}
			gfx_printf("This can take a LONG time.\n\nHold [VOL] to cancel current file.\n");
				
			switch (src_nand_no){
				case 0:
				memcpy(emunand_folder_src+0, "sxos/emunand", 12);
				break;
				case 1:
				memcpy(emunand_folder_src+0, "sxos/emunand1", 14);
				break;
				case 2:
				memcpy(emunand_folder_src+0, "sxos/emunand2", 14);
				break;
				case 3:
				memcpy(emunand_folder_src+0, "sxos/emunand3", 14);
				break;
				case 4:
				memcpy(emunand_folder_src+0, "sxos/emunand4", 14);
				break;
				case 5:
				memcpy(emunand_folder_src+0, "sxos/emunand5", 14);
				break;
			}
			
			switch (dst_nand_no){
				case 1:
				memcpy(emunand_folder_dst+0, "sxos/emunand1", 13);
				break;
				case 2:
				memcpy(emunand_folder_dst+0, "sxos/emunand2", 14);
				break;
				case 3:
				memcpy(emunand_folder_dst+0, "sxos/emunand3", 14);
				break;
				case 4:
				memcpy(emunand_folder_dst+0, "sxos/emunand4", 14);
				break;
				case 5:
				memcpy(emunand_folder_dst+0, "sxos/emunand5", 14);
				break;
			}
			fr = f_mkdir(emunand_folder_dst);
			if (fr) {EPRINTFARGS("Folder %s not created.\nContinuing...\n", emunand_folder_dst);}
			else	{gfx_printf("Created %s\n", emunand_folder_dst);}
			btn_wait();
			//make marker file
			
			memcpy (marker_file + 0, emunand_folder_dst, strlen(emunand_folder_dst)+1);
			memcpy (marker_file + strlen(marker_file), "/", 2);
			memcpy (marker_file + strlen(marker_file), "marker" , 7);
			
			fr = f_open(&fp, marker_file, FA_WRITE | FA_CREATE_ALWAYS);
			itoa((dst_nand_no), marker, 10);
			f_puts(marker, &fp);
			f_close(&fp);
			if (!fr) gfx_printf("Created marker\n");
			btn_wait();
			
			bool copy1 = true;
			bool copy2 = false;
			bool copy3 = false;
			u8 emunand_parts = 0;
			
			for(;;){
			
			
			memset(temp_filename, 0x00, 0x40);
			if (copy1 && !copy2 && !copy3){
			memcpy(temp_filename, "boot0.bin", 10);
			copy1 = false; copy2 = true;goto start;
			}
			if (!copy1 && copy2 && !copy3){
			memcpy(temp_filename, "boot1.bin", 10);
			copy1 = false; copy2 = false; copy3 = true;goto start;
			}
			if (!copy1 && !copy2 && copy3){
			memcpy(temp_filename, "full.0", 7);
			itoa((emunand_parts), emunand_part, 10);
			memcpy(temp_filename+strlen(temp_filename), emunand_part, 2);
			memcpy(temp_filename+strlen(temp_filename), ".bin", 5);
			++emunand_parts;
			}
			if(emunand_parts >= 9) {copy1 = false; copy2 = false; copy3 = false;}
			else{copy1 = false; copy2 = false; copy3 = true; goto start;}
			
			if (!copy1 && !copy2 && !copy3){
			gfx_printf("\nCompleted. Press any key.\n");
			free(temp_src);
			free(temp_dst);
			free(temp_src);
			free(temp_src);
			free(emunand_folder_src);
			free(emunand_folder_dst);
			btn_wait();
			break;
			}
start:
			memset(temp_src, 0x00, 0x40);
			memset(temp_dst, 0x00, 0x40);
			memcpy(temp_src+0, emunand_folder_src, strlen(emunand_folder_src)+1);
			memcpy(temp_src+strlen(temp_src), "/" ,2);
			memcpy(temp_src+strlen(temp_src), temp_filename, strlen(temp_filename)+1);
			memcpy(temp_dst+0, emunand_folder_dst, strlen(emunand_folder_dst)+1);
			memcpy(temp_dst+strlen(temp_dst), "/" ,2);
			memcpy(temp_dst+strlen(temp_dst), temp_filename, strlen(temp_filename)+1);
			copy_file (temp_src, temp_dst);
			}
			return;	
		}
	btn_wait();
	return;
}
}


void emunand_1(){sxos_emunand_setup(1);}
void emunand_2(){sxos_emunand_setup(2);}
void emunand_3(){sxos_emunand_setup(3);}
void emunand_4(){sxos_emunand_setup(4);}
void emunand_5(){sxos_emunand_setup(5);}
void emunand_6(){sxos_emunand_setup(6);}
void emunand_7(){sxos_emunand_setup(7);}

void about()
{
	static const char credits[] =
		"\nhekate     (C) 2018 naehrwert, st4rk\n\n"
		"CTCaer mod (C) 2019 CTCaer\n"
		"Lockpick (C) 2019 Shchmue\n"
		"Argon-NX (C) 2019 Guillem96, MrDude\n"
		"Initial SAMD-Fusee implementation Atlas44\n"
		"Switchboot brought to you by Mattytrog\n\n";

	static const char uf2_strap_info[] =
		"Running in standalone mode. Modchip          "
		"\n\n"
		"information is not available. Strap data     "
		"\n\n"
		"requires your SAMD21 to be fitted inside the "
		"\n\n"
		"console. Thank-you for your interest in      "
		"\n\n"
		"Fusee-UF2 and Switchboot.                    "
		"\n\n"
		"<                                           >"
		"\n\n"
		"<                                           >"
		"\n\n";
		
	gfx_clear_black(0x00);
	gfx_con_setpos(0, 0);

	gfx_printf("%k%s", 0xFFFFFFFF, credits);
	if (read_strap_info() == 0) {
		gfx_printf("%k%s", 0xFF00FF00, uf2_strap_info);
	} else gfx_printf ("\nStraps_info.txt OK");
	

	btn_wait();
	
}

ment_t ment_options[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Auto boot", config_autoboot),
	MDEF_HANDLER("Boot time delay", config_bootdelay),
	MDEF_HANDLER("Auto NoGC", config_nogc),
	MDEF_HANDLER("Auto HOS power off", config_auto_hos_poweroff),
	MDEF_HANDLER("Backlight", config_backlight),
	MDEF_END()
};

menu_t menu_options = {
	ment_options,
	"Launch Options", 0, 0
};

ment_t ment_restore[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Safety Backup --", 0xFF00FF00),
	MDEF_HANDLER("Restore BOOT0/1 + Prodinfo (safe folder)", restore_emmc_quick),
	MDEF_HANDLER("Restore Prodinfo ONLY (safe folder) folder", restore_emmc_quick_prodinfo),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Full --", 0xFF00FF00),
	MDEF_HANDLER("Restore eMMC BOOT0/1", restore_emmc_boot),
	MDEF_HANDLER("Restore eMMC RAW GPP (exFAT only)", restore_emmc_rawnand),
	MDEF_HANDLER("Restore eMMC RAW GPP", restore_emmc_rawnand),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- GPP Partitions --", 0xFF00FF00),
	MDEF_HANDLER("Restore GPP partitions", restore_emmc_gpp_parts),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Misc --", 0xFF00FF00),
	MDEF_HANDLER("Restore SXOS license.dat", restore_license_dat),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Dangerous --", 0xFFFF0000),
	MDEF_HANDLER("Restore BOOT0/1 without size check", restore_emmc_boot_noszchk),
	MDEF_END()
};
menu_t menu_restore = {
	ment_restore,
	"Restore Options", 0, 0
};

ment_t ment_backup[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Safety Backup --", 0xFF00FF00),
	MDEF_HANDLER("Backup BOOT0/1 + Prodinfo (safe folder)", dump_emmc_quick),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Full --", 0xFF00FF00),
	MDEF_HANDLER("Backup eMMC BOOT0/1", dump_emmc_boot),
	MDEF_HANDLER("Backup eMMC RAW GPP", dump_emmc_rawnand),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- GPP Partitions --", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Backup eMMC SYS", dump_emmc_system),
	MDEF_HANDLER("Backup eMMC USER", dump_emmc_user),
	MDEF_END()
};

menu_t menu_backup = {
	ment_backup,
	"Backup Options", 0, 0
};

ment_t ment_tools[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Backup & Restore --", 0xFF00FF00),
	MDEF_MENU("Backup", &menu_backup),
	MDEF_MENU("Restore", &menu_restore),
	MDEF_HANDLER("Verification options", config_verification),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Select Backup Folder --", 0xFF00FF00),
	MDEF_HANDLER("BACKUP_1", select_folder_1),
	MDEF_HANDLER("BACKUP_2", select_folder_2),
	MDEF_HANDLER("BACKUP_3", select_folder_3),
	MDEF_HANDLER("BACKUP_4", select_folder_4),
	MDEF_HANDLER("BACKUP_5", select_folder_5),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Misc --", 0xFF00FF00),
	//MDEF_HANDLER("Dump firmware nca files", nca_to_sdcard),
	//MDEF_HANDLER("Dump emmc save files", save_to_sdcard),
	//MDEF_HANDLER("Dump all keys", dump_all_keys),
	MDEF_HANDLER("Dump fuse info", print_fuseinfo),
	MDEF_HANDLER("Dump TSEC key", print_tsec_key),
	//MDEF_CHGLINE(),
	//MDEF_HANDLER("Fix archive bit (except Nintendo)", fix_sd_all_attr),
	//MDEF_HANDLER("Fix archive bit (Nintendo only)", fix_sd_nin_attr),
	//MDEF_HANDLER("Fix fuel gauge configuration", fix_fuel_gauge_configuration),
	//MDEF_HANDLER("Reset all battery cfg", reset_pmic_fuel_gauge_charger_config),
	//MDEF_HANDLER("Minerva", minerva), // Uncomment for testing Minerva Training Cell
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Other --", 0xFFFFFF00),
	MDEF_HANDLER("AutoRCM", menu_autorcm),
	MDEF_END()
};

menu_t menu_tools = {
	ment_tools,
	"Tools", 0, 0
};

ment_t ment_sxos[] = {
	
	MDEF_CAPTION("-- Select Option --", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Restore SXOS licence.dat", restore_license_dat),
	MDEF_HANDLER("Setup emuNAND folders", emunand_6),
	MDEF_HANDLER("Clone emuNAND to slot", emunand_7),
	MDEF_HANDLER("Use EmuNAND slot 1(default)", emunand_1),
	MDEF_HANDLER("Use EmuNAND slot 2", emunand_2),
	MDEF_HANDLER("Use EmuNAND slot 3", emunand_3),
	MDEF_HANDLER("Use EmuNAND slot 4", emunand_4),
	MDEF_HANDLER("Use EmuNAND slot 5", emunand_5),
	MDEF_CHGLINE(),
	MDEF_END()
};
		

menu_t menu_sxos = {
	ment_sxos,
	"-- SXOS Menu --", 0, 0
};

ment_t ment_launch[] = {
	
	MDEF_CAPTION("-- Select Option --", 0xFF00FF00),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Back", home_dir),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Other configs", ini_launch),
	
	MDEF_HANDLER("Browse .bin / .ini", type1),
	
	MDEF_HANDLER("Add to boot ini", type2),
	
	MDEF_HANDLER("Add [Stock] to boot ini", type3),
	MDEF_CHGLINE(),
	MDEF_END()
};
		

menu_t menu_launch = {
	ment_launch,
	"-- Payload Menu --", 0, 0
};

ment_t ment_top[] = {
	MDEF_HANDLER("Boot Firmware", launch_firmware),
	MDEF_CHGLINE(),
	MDEF_MENU("Payload / Linux / ini menu", &menu_launch),
	MDEF_CHGLINE(),
	MDEF_MENU("SXOS Options", &menu_sxos),
	MDEF_CHGLINE(),
	MDEF_MENU("Settings", &menu_options),
	MDEF_CHGLINE(),
	MDEF_MENU("System Tools / Info", &menu_tools),
	MDEF_CHGLINE(),
	MDEF_HANDLER("SAMD21 Update mode", restore_septprimary_dat),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Reboot (Normal)", reboot_normal),
	MDEF_HANDLER("Reboot (RCM)", reboot_rcm),
	MDEF_HANDLER("Power off", power_off),
	MDEF_CHGLINE(),
	MDEF_HANDLER("About / Chip information", about),
	MDEF_CAPTION("---------------------", 0xFF444444),
	MDEF_CHGLINE(),
	MDEF_END()
};
menu_t menu_top = {
	ment_top,
	"Switchboot v1.3.6 - Hekate v4.10.1", 0, 0
};

#define IPL_STACK_TOP  0x90010000
#define IPL_HEAP_START 0x90020000
#define IPL_HEAP_END   0xB5000000

extern void pivot_stack(u32 stack_top);

void ipl_main()
{
	// Do initial HW configuration. This is compatible with consecutive reruns without a reset.
	config_hw();

	//Pivot the stack so we have enough space.
	pivot_stack(IPL_STACK_TOP);

	//Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
	heap_init(IPL_HEAP_START);

#ifdef DEBUG_UART_PORT
	uart_send(DEBUG_UART_PORT, (u8 *)"Hekate: Hello!\r\n", 16);
	uart_wait_idle(DEBUG_UART_PORT, UART_TX_IDLE);
#endif

	// Set bootloader's default configuration.
	set_default_configuration();

	// Save sdram lp0 config.
	if (ianos_loader(true, "bootloader/sys/libsys_lp0.bso", DRAM_LIB, (void *)sdram_get_params_patched()))
		h_cfg.errors |= ERR_LIBSYS_LP0;

	display_init();

	u32 *fb = display_init_framebuffer();
	gfx_init_ctxt(fb, 720, 1280, 720);

#ifdef MENU_LOGO_ENABLE
	Kc_MENU_LOGO = (u8 *)malloc(ALIGN(SZ_MENU_LOGO, 0x1000));
	blz_uncompress_srcdest(Kc_MENU_LOGO_blz, SZ_MENU_LOGO_BLZ, Kc_MENU_LOGO, SZ_MENU_LOGO);
#endif

	gfx_con_init();
	display_backlight_pwm_init();
	
	
	u8 btn = btn_wait_timeout(0, BTN_VOL_DOWN | BTN_VOL_UP);
	if (btn & BTN_VOL_DOWN || btn & BTN_VOL_UP){
		//auto_launch_firmware();
		while (true){
		
		tui_do_menu(&menu_top);
		}
	}
		else {
			if (EMC(EMC_SCRATCH0) & EMC_SEPT_RUN){
			auto_launch_firmware();
			dump_all_keys();
			while (true){
			tui_do_menu(&menu_top);
			}
			} else {
			auto_launch_argon();
			auto_launch_payload_bin();
			auto_launch_numbered_payload();
			auto_launch_firmware();
			while (true)
				{
				tui_do_menu(&menu_top);
				}
			}
	}
}