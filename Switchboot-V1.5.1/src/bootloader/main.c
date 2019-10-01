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
#include "mem/minerva.h"
#include "mem/sdram.h"
#include "power/max77620.h"
#include "rtc/max77620-rtc.h"
#include "soc/bpmp.h"
#include "soc/fuse.h"
#include "soc/hw_init.h"
#include "soc/i2c.h"
#include "soc/t210.h"
#include "soc/uart.h"
#include "storage/emummc.h"
#include "storage/nx_emmc.h"
#include "storage/sdmmc.h"
#include "utils/btn.h"
//#include "utils/dirlist.h"
#include "utils/list.h"
#include "utils/util.h"
#include "utils/browser.h"

#include "frontend/fe_emmc_tools.h"
#include "frontend/fe_tools.h"
#include "frontend/fe_info.h"



//TODO: ugly.
sdmmc_t sd_sdmmc;
sdmmc_storage_t sd_storage;
FATFS sd_fs;
static bool sd_mounted;

hekate_config h_cfg;
boot_cfg_t __attribute__((section ("._boot_cfg"))) b_cfg;
const volatile ipl_ver_meta_t __attribute__((section ("._ipl_version"))) ipl_ver = {
	.magic = BL_MAGIC,
	.version = (BL_VER_MJ + '0') | ((BL_VER_MN + '0') << 8) | ((BL_VER_HF + '0') << 16),
	.rsvd0 = 0,
	.rsvd1 = 0
};

volatile nyx_storage_t *nyx_str = (nyx_storage_t *)NYX_STORAGE_ADDR;

bool sd_mount()
{
	if (sd_mounted)
		return true;

	if (!sdmmc_storage_init_sd(&sd_storage, &sd_sdmmc, SDMMC_1, SDMMC_BUS_WIDTH_4, 11))
	{
		EPRINTF("\n SD Card not found.\n");
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
			EPRINTFARGS("\n SD card error %d.\n FAT partition incorrect.\n", res);
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
		return res;
	}

	f_write(&fp, buf, size, NULL);
	f_close(&fp);

	return 0;
}

void emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage)
{
	sdmmc_storage_t storage2;
	sdmmc_t sdmmc;
	char emmcSN[9];
	char *file_sec = NULL;
	char *caption = (char*)malloc(64);
	bool init_done = false;

	if (!storage)
	{
		if (!sdmmc_storage_init_mmc(&storage2, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4))
			memcpy(emmcSN, "00000000", 9);
		else
		{
			init_done = true;
			itoa(storage2.cid.serial, emmcSN, 16);
		}
	}
	else itoa(storage->cid.serial, emmcSN, 16);



if(!strcmp(sub_dir, "<browser>"))
{
	memcpy(path+0, "\0", 2);
	
	memcpy(caption+0, "Select ", 8);
	memcpy(caption+strlen(caption), filename, strlen(filename)+1);
	memcpy(caption+strlen(caption), " file", 6);
	
	file_sec = file_browser(NULL, NULL, caption, false, true);
	if(!file_sec) goto out;
	
	memcpy(path+0, file_sec, strlen(file_sec)+1);
	goto out;
}

u32 sub_dir_len = strlen(sub_dir);   // Can be a null-terminator.
u32 filename_len = strlen(filename); // Can be a null-terminator.

if(strcmp(sub_dir, "/safe"))
{
	memcpy(path, "backup", 7);
	f_mkdir(path);
	
	
	memcpy(path + strlen(path), "/", 2);
	memcpy(path + strlen(path), emmcSN, 9);	
	f_mkdir(path);
	memcpy(path + strlen(path), sub_dir, sub_dir_len + 1);
}
else 
{
	memcpy(path, "safe", 5);
}
	
	if (sub_dir_len){
	f_mkdir(path);}
	memcpy(path + strlen(path), "/", 2);
	memcpy(path + strlen(path), filename, filename_len + 1);

out:
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

		if (h_cfg.autohosoff == 1)
		{
			gfx_clear_grey(BG_COL);

			display_backlight_brightness(10, 5000);
			display_backlight_brightness(100, 25000);
			msleep(600);
			display_backlight_brightness(0, 20000);
		}
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

void ini_list_launcher();

int launch_payload(char *path, bool update)
{
	if (!update)
		gfx_clear_grey(BG_COL);
	gfx_con_setpos(0, 0);
	if (!path)
		return 1;

	if (sd_mount())
	{
		FIL fp;
		if (f_open(&fp, path, FA_READ))
		{
			EPRINTF("Payload missing!\n");
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
		//free(path);

		sd_unmount();

		if (size < 0x30000)
		{
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

		msleep(100);

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
			EPRINTF ("Strap size error\n\n");
			read_info = false;
			goto out;
		}

		if (f_read(&fp, strap_buffer, size, NULL))
		{
			f_close(&fp);
			sd_unmount();
			EPRINTF ("Strap info fail\n\n");
			read_info = false;
			goto out;
		}
		const char* strap_info = strap_buffer;
		free(strap_buffer);
		f_close(&fp);

	gfx_printf("%k%s%k\n", INFO_TEXT_COL, strap_info, MAIN_TEXT_COL);
	
	read_info = true;
	}
out:
sd_unmount();
if (read_info){	
	return 1;
	} else return 0;
	}

void launch_tools(u8 type)
{
	
	char *file_sec = NULL;


	gfx_clear_grey(BG_COL);
	gfx_con_setpos(0, 0);
if (sd_mount())
	{
		if (type == 1)
		{	
		
			file_sec = file_browser("bootloader/payloads", ".bin", "Choose a payload", false, true);
			if (!file_sec) return;
		}	

		if (file_sec)
			{
				if (launch_payload(file_sec, false))
				{
					EPRINTFARGS("Failed to launch payload.\n%s", file_sec);
					free(file_sec);
				}
			}
		

		if (type == 2 || type == 3)
			
			{
				if (type == 2){
				file_sec = file_browser(NULL, NULL, "Choose a payload", false, true);
				if (!file_sec) return;
				}
				gfx_clear_grey(BG_COL);
				gfx_con_setpos(0, 0);
				
				
				u8 res = create_config_entry();
				if (res){
					gfx_printf ("Failed creating config entry.\n\nPress any key.");
					btn_wait();
					return;
				}
				FIL fp;
				sd_mount();
				if (f_open(&fp, "bootloader/hekate_ipl.ini", FA_WRITE | FA_READ) != FR_OK){
					gfx_printf ("Failed opening hekate_ipl.ini\n\nPress any key.");
					btn_wait();
					return;
					}
				f_lseek(&fp, f_size(&fp));
				if (type == 2){
					if(strcmp(file_sec+strlen(file_sec) - 4, ".bin")) EPRINTF("Not a payload bin file.\n");
					else 
					{
					f_puts("[Payload: ", &fp);
					f_puts(file_sec, &fp);
					f_puts("]\n", &fp);
					f_puts("payload=", &fp);
					f_puts(file_sec, &fp);
					gfx_printf ("Added:\n\n%s\n\nPress any key.", file_sec);
					}
				}
				
				else if (type == 3){
				f_puts("[Stock]\n", &fp);
				f_puts("fss0=atmosphere/fusee-secondary.bin\n", &fp);
				f_puts("stock=1\n", &fp);
				gfx_printf ("Done. Press any key.", file_sec);
				}
				btn_wait();
				f_close(&fp);
							
			}
			
		if (type == 4)
		{
			if (!f_rename("disabled.bin", "payload.bin")){gfx_printf ("Renamed disabled.bin"); goto out;}
			else if (!f_rename("payload.bin", "disabled.bin")){gfx_printf ("Renamed payload.bin"); goto out;}
			else {gfx_printf ("Missing bin file."); goto out;}
		}
		
	}
	free(file_sec);
	
out:
	sd_unmount();
	btn_wait();
}

void type1() {launch_tools(1); }
void type2() {launch_tools(2); }
void type3() {launch_tools(3); }
void type4() {launch_tools(4); }

void ini_list_launcher()
{
	u8 max_entries = 61;
	char *payload_path = NULL;
	char *path_sec = malloc(256);
	
	ini_sec_t *cfg_sec = NULL;
	LIST_INIT(ini_list_sections);

	gfx_clear_grey(BG_COL);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		path_sec = file_browser("bootloader/ini", ".ini", "Choose folder / INI", false, true);
		if (!path_sec) return;
		u8 fr1 = 0; u8 fr2 = 0;
		
		fr1 = (ini_parse(&ini_list_sections, path_sec, true));
		if(!fr1) {fr2 = (ini_parse(&ini_list_sections, path_sec, false));}
		
		if(fr1 || fr2)
		{
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
					ments, "Ini configurations", 0, 0
				};

				cfg_sec = (ini_sec_t *)tui_do_menu(&menu);

				if (cfg_sec)
				{
					u32 non_cfg = 1;
					for (int j = 2; j < i; j++)
					{
						if (ments[j].type != INI_CHOICE)
							non_cfg++;

						if (ments[j].data == cfg_sec)
						{
							b_cfg.boot_cfg = BOOT_CFG_FROM_LAUNCH;
							b_cfg.autoboot = j - non_cfg;
							b_cfg.autoboot_list = 1;

							break;
						}
					}
				}

				payload_path = ini_check_payload_section(cfg_sec);

				if (cfg_sec && !payload_path)
					check_sept();

				if (!cfg_sec)
				{
					free(ments);

					return;
				}
			}
			
		}
		else
		EPRINTF("Nothing here.\n");
	}

	if (!cfg_sec)
		goto out;

	if (payload_path)
	{
		if (launch_payload(payload_path, false))
		{
			EPRINTF("Payload failed.");
			free(payload_path);
		}
	}
	else if (!hos_launch(cfg_sec))
	{
		EPRINTF("Firmware failed.");
	}

out:
	EPRINTF("Press any key.");
	btn_wait();
}

void launch_firmware()
{
	u8 max_entries = 61;
	char *payload_path = NULL;

	ini_sec_t *cfg_sec = NULL;
	LIST_INIT(ini_sections);

	gfx_clear_grey(BG_COL);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		if (ini_parse(&ini_sections, "bootloader/hekate_ipl.ini", false))
		{
			// Build configuration menu.
			ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 6));
			ments[0].type = MENT_BACK;
			ments[0].caption = "Back";
			ments[1].type = MENT_CHGLINE;
			ments[2].type = MENT_HANDLER;
			ments[2].caption = "Payloads...";
			ments[2].handler = type1;
			ments[3].type = MENT_HANDLER;
			ments[3].caption = "More configs...";
			ments[3].handler = ini_list_launcher;
			ments[4].type = MENT_CHGLINE;

			u32 i = 5;
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

				if ((i - 5) > max_entries)
					break;
			}
			if (i < 5)
			{
				ments[i].type = MENT_CAPTION;
				ments[i].caption = "Nothing here.";
				ments[i].color = WARN_TEXT_COL;
				i++;
			}
			memset(&ments[i], 0, sizeof(ment_t));
			menu_t menu = {
				ments, "Launch configurations", 0, 0
			};

			cfg_sec = (ini_sec_t *)tui_do_menu(&menu);

			if (cfg_sec)
			{
				u8 non_cfg = 4;
				for (int j = 5; j < i; j++)
				{
					if (ments[j].type != INI_CHOICE)
						non_cfg++;
					if (ments[j].data == cfg_sec)
					{
						b_cfg.boot_cfg = BOOT_CFG_FROM_LAUNCH;
						b_cfg.autoboot = j - non_cfg;
						b_cfg.autoboot_list = 0;

						break;
					}
				}
			}

			payload_path = ini_check_payload_section(cfg_sec);

			if (cfg_sec)
			{
				LIST_FOREACH_ENTRY(ini_kv_t, kv, &cfg_sec->kvs, link)
				{
					if (!strcmp("emummc_force_disable", kv->key))
						h_cfg.emummc_force_disable = atoi(kv->val);
				}
			}

			if (cfg_sec && !payload_path)
				check_sept();

			if (!cfg_sec)
			{
				free(ments);
				sd_unmount();
				return;
			}

			free(ments);
		}
		else
			EPRINTF("Could not open 'bootloader/hekate_ipl.ini'.\nMake sure it exists!");
	}

	if (!cfg_sec)
	{
		gfx_puts("\n Trying default configuration...\n");
		gfx_puts("\n [PWR] Continue.\n [VOL+] Boot Stock with FuseCheck\n [VOL-] Menu.");

		u32 btn = btn_wait();
		if ((btn & BTN_VOL_DOWN))
			goto out;
		if((btn & BTN_VOL_UP))
			reboot_normal();
	}

	if (payload_path)
	{
		if (launch_payload(payload_path, false))
				{
				EPRINTF("Payload failed.");	
				}
		 
	}
		else if (!hos_launch(cfg_sec))
			EPRINTF("Firmware failed.");
out:
	
	free(payload_path);
	sd_unmount();

	h_cfg.emummc_force_disable = false;

	btn_wait();
}

void nyx_load_run()
{
	sd_mount();

	u8 *nyx = sd_file_read("bootloader/sys/nyx.bin", false);
	if (!nyx)
		return;

	sd_unmount();

	gfx_clear_grey(BG_COL);

	display_backlight_brightness(h_cfg.backlight, 1000);

	nyx_str->cfg = 0;
	if (b_cfg.extra_cfg & EXTRA_CFG_NYX_DUMP)
	{
		b_cfg.extra_cfg &= ~(EXTRA_CFG_NYX_DUMP);
		nyx_str->cfg |= NYX_CFG_DUMP;
	}
	if (nyx_str->mtc_cfg.mtc_table)
		nyx_str->cfg |= NYX_CFG_MINERVA;

	nyx_str->version = ipl_ver.version - 0x303030;

	//memcpy((u8 *)nyx_str->irama, (void *)IRAM_BASE, 0x8000);
	volatile reloc_meta_t *reloc = (reloc_meta_t *)(IPL_LOAD_ADDR + RELOC_META_OFF);
	memcpy((u8 *)nyx_str->hekate, (u8 *)reloc->start, reloc->end - reloc->start);

	void (*nyx_ptr)() = (void *)nyx;

	bpmp_mmu_disable();
	bpmp_clk_rate_set(BPMP_CLK_NORMAL);
	minerva_periodic_training();

	// Some cards (Sandisk U1), do not like a fast power cycle. Wait min 100ms.
	u32 sd_poweroff_time = (u32)get_tmr_ms() - h_cfg.sd_timeoff;
	if (sd_poweroff_time < 100)
		msleep(100 - sd_poweroff_time);

	(*nyx_ptr)();
}

void auto_launch_firmware()
{
	u32 btn = 0;
	if (EMC(EMC_SCRATCH0) & EMC_SEPT_RUN) goto skip;
	else 
	{	
		
		btn = btn_wait_timeout(0, BTN_VOL_DOWN | BTN_VOL_UP);
		
		if ((btn & BTN_VOL_DOWN) && (btn & BTN_VOL_UP)) goto out2;
		if (btn & BTN_VOL_DOWN) {goto skip;}
		
		if (sd_mount())
			{
				if (!f_stat("payload.bin", NULL))
				{
				launch_payload ("payload.bin", false);} 
				else if (!f_stat("payload1.bin", NULL))
				{
				launch_payload ("payload1.bin", false);
				}
			}
	}
skip:
	
	if(b_cfg.extra_cfg & EXTRA_CFG_NYX_DUMP)
	{
		if (!h_cfg.sept_run)
			EMC(EMC_SCRATCH0) |= EMC_HEKA_UPD;
		check_sept();
	}
	
	

	u8 *BOOTLOGO = NULL;
	char *payload_path = NULL;
	

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
							else if (!strcmp("brand", kv->key))
							{
								h_cfg.brand = malloc(strlen(kv->val) + 1);
								strcpy(h_cfg.brand, kv->val);
							}
							else if (!strcmp("tagline", kv->key))
							{
								h_cfg.tagline = malloc(strlen(kv->val) + 1);
								strcpy(h_cfg.tagline, kv->val);
							}
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
						cfg_sec = ini_sec;
						LIST_FOREACH_ENTRY(ini_kv_t, kv, &cfg_sec->kvs, link)
						{
							if (!strcmp("logopath", kv->key))
								bootlogoCustomEntry = kv->val;
							if (!strcmp("emummc_force_disable", kv->key))
								h_cfg.emummc_force_disable = atoi(kv->val);
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
								h_cfg.emummc_force_disable = false;
								cfg_sec = ini_sec_list;
								LIST_FOREACH_ENTRY(ini_kv_t, kv, &cfg_sec->kvs, link)
								{
									if (!strcmp("logopath", kv->key))
										bootlogoCustomEntry = kv->val;
									if (!strcmp("emummc_force_disable", kv->key))
										h_cfg.emummc_force_disable = atoi(kv->val);
								}
								break;
							}
							boot_entry_id++;
						}
						
					}

				}

			}

			// Add  configuration entry.
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
	if (!(b_cfg.boot_cfg & BOOT_CFG_FROM_LAUNCH) && h_cfg.bootwait && !h_cfg.sept_run)
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
		else
		{
			gfx_clear_grey(BG_COL);
		}
		free(BOOTLOGO);
	}

	if (b_cfg.boot_cfg & BOOT_CFG_FROM_LAUNCH)
		display_backlight_brightness(h_cfg.backlight, 0);
	else if (!h_cfg.sept_run && h_cfg.bootwait)
		display_backlight_brightness(h_cfg.backlight, 1000);

	
	// Wait before booting. If VOL- is pressed go into bootloader menu.
		
	if (!h_cfg.sept_run && !(b_cfg.boot_cfg & BOOT_CFG_FROM_LAUNCH))
	{
		btn = btn_wait_timeout(h_cfg.bootwait * 1000, BTN_VOL_DOWN | BTN_VOL_UP);

		if (btn & BTN_VOL_DOWN)
			goto out;
	}

	payload_path = ini_check_payload_section(cfg_sec);

	if (payload_path)
	{
		if (launch_payload(payload_path, false))
			free(payload_path);
	}
	else
	{
		check_sept();
		hos_launch(cfg_sec);
	}

out:
	gfx_con.mute = false;

	b_cfg.boot_cfg &= ~(BOOT_CFG_AUTOBOOT_EN | BOOT_CFG_FROM_LAUNCH);
	h_cfg.emummc_force_disable = false;		
		
	nyx_load_run();
out2:

	sd_unmount();
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

void sxos_emunand_setup(const u8 selection){
						
char *sx_names[11] =  {"full.00.bin", "full.01.bin", "full.02.bin",
									"full.03.bin", "full.04.bin", "full.05.bin",
									"full.06.bin", "full.07.bin",
									"boot0.bin", "boot1.bin", "Emutendo"};

char *sx_slots[10] = {"emunand", "emunand1", "emunand2", "emunand3", "emunand4", "emunand5"};
//set up some variables

	FILINFO fno;
	FIL fp;
	FRESULT fr;
	char *browse_dir = malloc(128);
	char *enand0 = malloc(128);
	char *enandx = malloc(128);
	char *temp = malloc(128);
	char *i_chr = malloc(2);
	u8 enandx_len;
	u8 enand0_len;

	u8 in_use[] = {0,0,0,0,0,0};
//clear screen
gfx_clear_grey(BG_COL);
	gfx_con_setpos(0, 0);
//check if SXOS emunand is set up

	if(sd_mount())
	{
		if(!f_stat("sxos/emunand", &fno))
		{
			switch (selection)
			{
			
				case 1:
				{//quick file check
					memcpy(enand0, "/sxos/emunand/", 15);
					u8 i = 0; while (i < 10)
					{
						memcpy(enand0+14, sx_names[i], strlen(sx_names[i])+1);
						fr = f_stat(enand0, &fno);
						if (!fr) gfx_printf("%s found\n",enand0);
							else {gfx_printf("%s not found\n",enand0); break;}
					++i;
					}
					
					gfx_printf("Found %d parts.\n",i);
					//make dirs
					
					f_mkdir("sxos/MultiNAND");
					for (i = 1; i < 6; ++i)
					{
					memcpy(temp+0, "sxos/MultiNAND/emunand", 23);
					itoa(i, i_chr, 10);
					memcpy(temp+strlen(temp), i_chr, 2);
					f_mkdir(temp);
					}
					memcpy(enand0+0, "/sxos/emunand/", 15);
					memcpy(enandx+0, "/sxos/MultiNAND", 16);
					memcpy (enandx+strlen(enandx), "/", 2);
					memcpy(enandx+strlen(enandx), "emunand1/", 10);
					
					enand0_len = strlen(enand0);
					enandx_len = strlen(enandx);
					i = 0; while (i < 10)
					{
						
						memcpy(enand0+enand0_len, sx_names[i], strlen(sx_names[i])+1);
						memcpy(enandx+enandx_len, sx_names[i], strlen(sx_names[i])+1);
						fr = f_stat(enand0, &fno);
						fr = f_rename (enand0, enandx);
						if (!fr) gfx_printf("%s moved\n",enand0);
							else {gfx_printf("\nAlready set up!\n\nEnsure slots are empty!\n"); break;}
					++i;
					}
					
					if(!fr)
					{
						fr = (f_stat("Emutendo", &fno));
						
							memcpy(enandx+enandx_len, "Emutendo",9);
							fr = f_rename ("Emutendo", enandx);
							if(!fr) 
						{
							gfx_printf("Emutendo moved\n");
						}
							else 
						{
							EPRINTF("Cannot move Emutendo.\n");
						}
						
						for (i = 1; i < 6; ++i)
							{
							memcpy(temp+0, "sxos/MultiNAND/emunand", 23);
							itoa(i, i_chr, 10);
							memcpy(temp+strlen(temp), i_chr, 2);
							memcpy(temp+strlen(temp), "/active", 8);
							if (f_unlink(temp) == FR_OK) gfx_printf("Slot %d deactivated\n", i);
							}
					}
					break;	
				}
				
				
				case 2:
				{
					browse_dir = file_browser("sxos/MultiNAND", NULL, "Select slot to activate.", true, false);
					if (!browse_dir) break;
					memcpy(enandx+0, browse_dir, strlen(browse_dir)+1);
					memcpy (enandx+strlen(enandx), "/", 2);
					memcpy (enand0+0, "sxos/emunand/", 14);
					enandx_len = strlen(enandx);
					enand0_len = strlen(enand0);
					memcpy(enandx+enandx_len, "active", 7);
					fr = (f_stat(enandx, &fno));
					if (!fr) {gfx_printf("Already active!\n");}
					u8 i = 0; while (i < 10)
					{
						memcpy(enand0+enand0_len, sx_names[i], strlen(sx_names[i])+1);
						memcpy(enandx+enandx_len, sx_names[i], strlen(sx_names[i])+1);
						fr = f_rename (enandx, enand0);
						if (!fr) gfx_printf("%s moved\n",enand0);
							
						++i;
					}
					if (fr) {gfx_printf("Failed. Empty?\n");}
					if(!fr)
					{
						memcpy(enandx+enandx_len, "active", 7);
						fr = f_open(&fp, enandx, FA_WRITE | FA_CREATE_ALWAYS);
						if (!fr) {gfx_printf("Opened new marker\n");
						f_close(&fp);
						}
					
					//check for emutendo folder
					memcpy(enandx+enandx_len, "Emutendo",9);
								fr = f_rename (enandx, "Emutendo");
								if(!fr) 
								{
									gfx_printf("Emutendo moved\n"); break;
								}
					}
					break;		
				}
				
				
				case 3:
				{
					browse_dir = file_browser("sxos/MultiNAND", NULL, "Select slot for current emuNAND", true, false);
					if (!browse_dir) break;
					memcpy(enandx+0, browse_dir, strlen(browse_dir)+1);
					memcpy (enandx+strlen(enandx), "/", 2);
					memcpy (enand0+0, "sxos/emunand/", 14);
					enandx_len = strlen(enandx);
					enand0_len = strlen(enand0);
					u8 i = 0; while (i < 10)
					{
						memcpy(enand0+enand0_len, sx_names[i], strlen(sx_names[i])+1);
						memcpy(enandx+enandx_len, sx_names[i], strlen(sx_names[i])+1);
						fr = f_rename (enand0, enandx);
						if (!fr) gfx_printf("%s moved\n",enand0);
							
						++i;
					}
					if(fr) {gfx_printf("File problem!\nAlready moved?\n");}
					memcpy(enandx+enandx_len, "Emutendo",9);
						fr = f_rename ("Emutendo", enandx);
							if(!fr) 
								{
									gfx_printf("Emutendo moved\n");
								}
									else 
									{
										EPRINTF("Cannot move Emutendo.\n");
									}
					
						memcpy(enandx+enandx_len, "active", 7);
						fr = f_unlink(enandx);
						if (!fr) 
						{
							gfx_printf("Deactivated\n");
						} else 
						{
							for (i = 1; i < 6; ++i)
							{
							memcpy(temp+0, "sxos/MultiNAND/emunand", 23);
							itoa(i, i_chr, 10);
							memcpy(temp+strlen(temp), i_chr, 2);
							memcpy(temp+strlen(temp), "/active", 8);
							if (f_unlink(temp) == FR_OK) gfx_printf("Slot %d deactivated\n", i);
							}
						}
						
					
					break;		
				}
				
				
				case 4:
				{
					u8 i = 0; u8 j = 0;
					
					while(j < 6)
					{
						if (j == 0)
						{
						memcpy(enandx+0, "/sxos", 6);
						memcpy (enandx+strlen(enandx), "/", 2);
						memcpy (enandx+strlen(enandx), "emunand", 8);
						memcpy (enandx+strlen(enandx), "/", 2);
						enandx_len = strlen(enandx);
						}
						if (j != 0)
						{
						memcpy(enandx+0, "/sxos/MultiNAND", 16);
						memcpy (enandx+strlen(enandx), "/", 2);
						memcpy (enandx+strlen(enandx), sx_slots[j], strlen(sx_slots[j]) + 1);
						memcpy (enandx+strlen(enandx), "/", 2);
						enandx_len = strlen(enandx);
						}
						
						
							while (i < 10)
							{
							
							memcpy(enandx+enandx_len, sx_names[i], strlen(sx_names[i])+1);
							fr = (f_stat(enandx, &fno));
							if(!fr) {in_use[j] = 1; break;}
							++i;
							}
						if(j == 0)
						{
							if (in_use[0] == 1) gfx_printf("EmuNAND active\n\n");
						else gfx_printf("EmuNAND disabled\n\n");	
						}
						
						if(j != 0)
						{
						memcpy(enandx+enandx_len, "active", 7);
						fr = (f_stat(enandx, &fno));
						if (!fr) {gfx_printf("%kSlot %d, Active!\n\n%k",INFO_TEXT_COL, j, MAIN_TEXT_COL);}
						else if(in_use[j] == 1) gfx_printf("Slot %d, Not active. Files OK.\n\n", j);
						else gfx_printf("Slot %d, Folder empty.\n\n", j);
						
						}
						i = 0;
						++j;
					}	
						
					
					break;
				}
			}
		} else EPRINTF("SXOS EmuNAND is not set up.\nPlease set up within SXOS");
	}
if (!browse_dir) return;
btn_wait();
}

void emunand_1(){sxos_emunand_setup(1);}
void emunand_2(){sxos_emunand_setup(2);}
void emunand_3(){sxos_emunand_setup(3);}
void emunand_4(){sxos_emunand_setup(4);}
void emunand_5(){sxos_emunand_setup(5);}
void emunand_6(){sxos_emunand_setup(6);}
void emunand_7(){sxos_emunand_setup(7);}
void emunand_8(){sxos_emunand_setup(8);}

void about()
{
	static const char credits[] =
		"\nhekate     (c) 2018 naehrwert, st4rk\n\n"
		"CTCaer mod (c) 2018 CTCaer\n"
		"Switchboot from Mattytrog\n"
		" ___________________________________________\n\n";
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
		
	gfx_clear_grey(BG_COL);
	gfx_con_setpos(0, 0);

	gfx_printf("%k%s", MAIN_TEXT_COL, credits);
	if (read_strap_info() == 0) {
		gfx_printf("%k%s", INFO_TEXT_COL, uf2_strap_info);
	} else gfx_printf ("\nStraps_info.txt OK");
	

	btn_wait();
	
}

ment_t ment_options[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Boot Options --", INFO_TEXT_COL),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Browse Payloads", type1),
	MDEF_HANDLER("Add payload to hekate_ipl.ini", type2),
	MDEF_HANDLER("Browse for INI / configs", ini_list_launcher),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Autoboot payload / INI", config_autoboot),
	MDEF_HANDLER("Disable / Enable payload.bin", type4),
	MDEF_HANDLER("Boot delay", config_bootdelay),
	MDEF_HANDLER("Auto disable cart slot update", config_nogc),
	MDEF_CHGLINE(),
	//MDEF_HANDLER("Add stock entry to INI (Atmosphere)", type3),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Power Off / Display --", INFO_TEXT_COL),
	MDEF_HANDLER("Prevent reboot after power-off", config_auto_hos_poweroff),
	MDEF_HANDLER("LCD Backlight", config_backlight),
	MDEF_END()
};

menu_t menu_options = { ment_options, "Launch Options", 0, 0 };

ment_t ment_cinfo[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Peripheral Info --", INFO_TEXT_COL),
	//MDEF_HANDLER("eMMC Partitions", print_mmc_info),
	MDEF_HANDLER("SD Card", print_sdcard_info),
	MDEF_HANDLER("Battery", print_battery_info),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Advanced Info --", INFO_TEXT_COL),
	//MDEF_HANDLER("Ipatches/Bootrom", bootrom_ipatches_info),
	MDEF_HANDLER("Save fuses.bin", print_fuseinfo),
	//MDEF_HANDLER("Save kfuse.bin", print_kfuseinfo),
	MDEF_HANDLER("Save TSEC keys", print_tsec_key),
	MDEF_CHGLINE(),
	
	MDEF_END()
};

menu_t menu_cinfo = { ment_cinfo, "Console Info", 0, 0 };

ment_t ment_restore[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- EMMC --", INFO_TEXT_COL),
	MDEF_HANDLER("All", restore_emmc_all),
	MDEF_HANDLER("Safe Minimal", perform_minimal_restore),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Partitions --", INFO_TEXT_COL),
	MDEF_HANDLER("BOOT0", restore_emmc_boot0),
	MDEF_HANDLER("BOOT1", restore_emmc_boot1),
	MDEF_HANDLER("Prodinfo", restore_emmc_prodinfo),
	MDEF_HANDLER("Prodinfof", restore_emmc_prodinfof),
	MDEF_HANDLER("BCPKG2-1-Normal-Main", restore_emmc_2_1),
	MDEF_HANDLER("BCPKG2-2-Normal-Sub", restore_emmc_2_2),
	MDEF_HANDLER("BCPKG2-3-Safemode-Main", restore_emmc_2_3),
	MDEF_HANDLER("BCPKG2-4-Safemode-Sub", restore_emmc_2_4),
	MDEF_HANDLER("BCPKG2-5-Repair-Main", restore_emmc_2_5),
	MDEF_HANDLER("BCPKG2-6-Repair-Main", restore_emmc_2_6),
	MDEF_HANDLER("SAFE", restore_emmc_safe),
	MDEF_HANDLER("SYSTEM", restore_emmc_system),
	MDEF_HANDLER("USER", restore_emmc_user),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Raw --", INFO_TEXT_COL),
	MDEF_HANDLER("Rawnand (Rawnand.bin)", restore_emmc_rawnand),
	MDEF_END()
};
menu_t menu_restore = {
	ment_restore,
	"Choose Restore Option", 0, 0
};

ment_t ment_restore_noszchk[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Partitions --", INFO_TEXT_COL),
	MDEF_HANDLER("BOOT0", restore_emmc_boot0_noszchk),
	MDEF_HANDLER("BOOT1", restore_emmc_boot1_noszchk),
	MDEF_HANDLER("Prodinfo", restore_emmc_prodinfo_noszchk),
	MDEF_HANDLER("Prodinfof", restore_emmc_prodinfof_noszchk),
	MDEF_HANDLER("BCPKG2-1-Normal-Main", restore_emmc_2_1_noszchk),
	MDEF_HANDLER("BCPKG2-2-Normal-Sub", restore_emmc_2_2_noszchk),
	MDEF_HANDLER("BCPKG2-3-Safemode-Main", restore_emmc_2_3_noszchk),
	MDEF_HANDLER("BCPKG2-4-Safemode-Sub", restore_emmc_2_4_noszchk),
	MDEF_HANDLER("BCPKG2-5-Repair-Main", restore_emmc_2_5_noszchk),
	MDEF_HANDLER("BCPKG2-6-Repair-Main", restore_emmc_2_6_noszchk),
	MDEF_HANDLER("SAFE", restore_emmc_safe_noszchk),
	MDEF_HANDLER("SYSTEM", restore_emmc_system_noszchk),
	MDEF_HANDLER("USER", restore_emmc_user_noszchk),
	MDEF_END()
};
menu_t menu_restore_noszchk = {
	ment_restore_noszchk,
	"Choose Partition to RESTORE TO", 0, 0
};

ment_t ment_backup[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- EMMC --", INFO_TEXT_COL),
	MDEF_HANDLER("All", backup_emmc_all),
	MDEF_HANDLER("Safe Minimal", perform_minimal_backup),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Partitions --", INFO_TEXT_COL),
	MDEF_HANDLER("BOOT0", backup_emmc_boot0),
	MDEF_HANDLER("BOOT1", backup_emmc_boot1),
	MDEF_HANDLER("Prodinfo", backup_emmc_prodinfo),
	MDEF_HANDLER("Prodinfof", backup_emmc_prodinfof),
	MDEF_HANDLER("BCPKG2-1-Normal-Main", backup_emmc_2_1),
	MDEF_HANDLER("BCPKG2-2-Normal-Sub", backup_emmc_2_2),
	MDEF_HANDLER("BCPKG2-3-Safemode-Main", backup_emmc_2_3),
	MDEF_HANDLER("BCPKG2-4-Safemode-Sub", backup_emmc_2_4),
	MDEF_HANDLER("BCPKG2-5-Repair-Main", backup_emmc_2_5),
	MDEF_HANDLER("BCPKG2-6-Repair-Main", backup_emmc_2_6),
	MDEF_HANDLER("SAFE", backup_emmc_safe),
	MDEF_HANDLER("SYSTEM", backup_emmc_system),
	MDEF_HANDLER("USER", backup_emmc_user),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Raw --", INFO_TEXT_COL),
	MDEF_HANDLER("Rawnand (Rawnand.bin)", backup_emmc_rawnand),
	MDEF_END()
};

menu_t menu_backup = {
	ment_backup,
	"Choose Backup Option", 0, 0
};

ment_t ment_sxos[] = {
	
	MDEF_CAPTION("-- MultiNAND --", INFO_TEXT_COL),
	MDEF_CHGLINE(),
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_HANDLER("MultiNAND setup", emunand_1),
	MDEF_HANDLER("Activate emuNAND", emunand_2),
	MDEF_HANDLER("Deactivate emuNAND / move slot", emunand_3),
	MDEF_HANDLER("Slot status", emunand_4),
	MDEF_CHGLINE(),
	MDEF_HANDLER("Restore licence.dat", restore_license_dat),
	MDEF_CHGLINE(),
	MDEF_END()
};
		

menu_t menu_sxos = {
	ment_sxos,
	"-- SXOS Menu --", 0, 0
};

ment_t ment_tools[] = {
	MDEF_BACK(),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Backup & Restore --", INFO_TEXT_COL),
	MDEF_MENU("Backup", &menu_backup),
	MDEF_MENU("Restore", &menu_restore),
	MDEF_HANDLER("Verification options", config_verification),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Misc --", INFO_TEXT_COL),
	//MDEF_HANDLER("Dump package1/2", dump_packages12),
	MDEF_HANDLER("Set folder archive bit", set_sd_attr),
	MDEF_HANDLER("Unset folder archive bit", unset_sd_attr),
	MDEF_HANDLER("Fix fuel gauge", fix_fuel_gauge_configuration),
	MDEF_HANDLER("Reset battery cfg", reset_pmic_fuel_gauge_charger_config),
	MDEF_CHGLINE(),
	MDEF_CAPTION("-- Other --", WARN_TEXT_COL),
	MDEF_HANDLER("AutoRCM", menu_autorcm),
	MDEF_MENU("DANGER! - Restore No Size Check - DANGER!", &menu_restore_noszchk),
	MDEF_END()
};

menu_t menu_tools = { ment_tools, "Tools", 0, 0 };

ment_t ment_top[] = {
	MDEF_HANDLER("Launch", launch_firmware),
	MDEF_CAPTION("------", STATIC_TEXT_COL),
	MDEF_MENU("Boot Options", &menu_options),
	MDEF_CAPTION("------------", STATIC_TEXT_COL),
	MDEF_MENU("SXOS MultiNAND", &menu_sxos),
	MDEF_CAPTION("------------------------", STATIC_TEXT_COL),
	MDEF_MENU("Tools", &menu_tools),
	MDEF_MENU("Info", &menu_cinfo),
	MDEF_CAPTION("-----", STATIC_TEXT_COL),
	MDEF_HANDLER("Mount USB (tidy_memloader)", reboot_memloader),
	MDEF_HANDLER("Reboot (SAMD21 Update)", reboot_samd21_update),
	MDEF_HANDLER("Reboot (Normal)", reboot_normal),
	MDEF_HANDLER("Reboot (RCM)", reboot_rcm),
	MDEF_HANDLER("Power off", power_off),
	MDEF_CAPTION("---------", STATIC_TEXT_COL),
	MDEF_HANDLER("About", about),
	MDEF_END()
};

menu_t menu_top = { ment_top, "Switchboot v1.5.0 - Hekate CTCaer v5.0.2", 0, 0 };

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

	sd_mount();

	// Save sdram lp0 config.
	if (!ianos_loader(false, "bootloader/sys/libsys_lp0.bso", DRAM_LIB, (void *)sdram_get_params_patched()))
		h_cfg.errors |= ERR_LIBSYS_LP0;
		
	minerva_init();
	minerva_change_freq(FREQ_1600);

	display_init();

	u32 *fb = display_init_framebuffer();
	gfx_init_ctxt(fb, 720, 1280, 720);

	gfx_con_init();

	display_backlight_pwm_init();
	//display_backlight_brightness(h_cfg.backlight, 1000);

	// Overclock BPMP.
	bpmp_clk_rate_set(BPMP_CLK_SUPER_BOOST);

	// Check if we had a panic while in CFW.
	secmon_exo_check_panic();

	// Check if RCM is patched and protect from a possible brick.
	patched_rcm_protection();

	// Load emuMMC configuration from SD.
	emummc_load_cfg();

	// Load saved configuration and auto boot if enabled.
	auto_launch_firmware();

	minerva_change_freq(FREQ_800);
	
	
	while (true)
		tui_do_menu(&menu_top);

	while (true)
		;
}