/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2019 CTCaer
 * Copyright (c) 2018 balika011
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

#include "fe_info.h"
#include "../gfx/gfx.h"
#include "../hos/hos.h"
#include "../hos/pkg1.h"
#include "../libs/fatfs/ff.h"
#include "../mem/heap.h"
#include "../power/bq24193.h"
#include "../power/max17050.h"
#include "../sec/tsec.h"
#include "../soc/fuse.h"
#include "../soc/i2c.h"
#include "../soc/kfuse.h"
#include "../soc/smmu.h"
#include "../soc/t210.h"
#include "../storage/mmc.h"
#include "../storage/nx_emmc.h"
#include "../storage/sdmmc.h"
#include "../utils/btn.h"
#include "../utils/util.h"

extern sdmmc_storage_t sd_storage;
extern FATFS sd_fs;

extern bool sd_mount();
extern void sd_unmount();
extern int  sd_save_to_file(void *buf, u32 size, const char *filename);
extern void emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage);

#pragma GCC push_options
#pragma GCC optimize ("Os") 

void print_fuseinfo()
{
	gfx_clear_partial_grey(BG_COL, 0, 1256);
	gfx_con_setpos(0, 0);

	u32 burntFuses = 0;
	for (u32 i = 0; i < 32; i++)
	{
		if ((fuse_read_odm(7) >> i) & 1)
			burntFuses++;
	}

	gfx_printf("\nSKU:         %X - ", FUSE(FUSE_SKU_INFO));
	switch (fuse_read_odm(4) & 3)
	{
	case 0:
		gfx_printf("Retail\n");
		break;
	case 3:
		gfx_printf("Dev\n");
		break;
	}
	gfx_printf("Sdram ID:    %d\n", (fuse_read_odm(4) >> 3) & 0x1F);
	gfx_printf("Burnt fuses: %d / 64\n", burntFuses);
	gfx_printf("Secure key:  %08X%08X%08X%08X\n\n\n",
		byte_swap_32(FUSE(FUSE_PRIVATE_KEY0)), byte_swap_32(FUSE(FUSE_PRIVATE_KEY1)),
		byte_swap_32(FUSE(FUSE_PRIVATE_KEY2)), byte_swap_32(FUSE(FUSE_PRIVATE_KEY3)));

	gfx_printf("%k(Unlocked) fuse cache:\n\n%k", INFO_TEXT_COL, MAIN_TEXT_COL);
	gfx_hexdump(0x7000F900, (u8 *)0x7000F900, 0x300);

	gfx_puts("\nPress POWER to dump them to SD Card.\nPress VOL to go to the menu.\n");

	u32 btn = btn_wait();
	if (btn & BTN_POWER)
	{
		if (sd_mount())
		{
			char path[64];
			emmcsn_path_impl(path, "/dumps", "fuse_cached.bin", NULL);
			if (!sd_save_to_file((u8 *)0x7000F900, 0x300, path))
				gfx_puts("\nfuse_cached.bin saved!\n");

			u32 words[192];
			fuse_read_array(words);
			emmcsn_path_impl(path, "/dumps", "fuse_array_raw.bin", NULL);
			if (!sd_save_to_file((u8 *)words, sizeof(words), path))
				gfx_puts("\nfuse_array_raw.bin saved!\n");

			sd_unmount();
		}

		btn_wait();
	}
}

void print_kfuseinfo()
{
	gfx_clear_partial_grey(BG_COL, 0, 1256);
	gfx_con_setpos(0, 0);

	gfx_printf("%kKFuse contents:\n\n%k", INFO_TEXT_COL, MAIN_TEXT_COL);
	u32 buf[KFUSE_NUM_WORDS];
	if (!kfuse_read(buf))
		EPRINTF("CRC fail.");
	else
		gfx_hexdump(0, (u8 *)buf, KFUSE_NUM_WORDS * 4);

	gfx_puts("\nPress POWER to dump them to SD Card.\nPress VOL to go to the menu.\n");

	u32 btn = btn_wait();
	if (btn & BTN_POWER)
	{
		if (sd_mount())
		{
			char path[64];
			emmcsn_path_impl(path, "/dumps", "kfuses.bin", NULL);
			if (!sd_save_to_file((u8 *)buf, KFUSE_NUM_WORDS * 4, path))
				gfx_puts("\nDone!\n");
			sd_unmount();
		}

		btn_wait();
	}
}

void print_sdcard_info()
{
	static const u32 SECTORS_TO_MIB_COEFF = 11;

	gfx_clear_partial_grey(BG_COL, 0, 1256);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		u32 capacity;

		capacity = sd_storage.csd.capacity >> (20 - sd_storage.csd.read_blkbits);

		gfx_puts("Acquiring FAT volume info...\n\n");
		f_getfree("", &sd_fs.free_clst, NULL);
		gfx_printf("%kFound %s volume:%k\n Capacity: %d MiB\n Free: %d MiB\n Cluster: %d KiB",
				INFO_TEXT_COL, sd_fs.fs_type == FS_EXFAT ? "exFAT" : "FAT32", MAIN_TEXT_COL, capacity, sd_fs.free_clst * sd_fs.csize >> SECTORS_TO_MIB_COEFF, (sd_fs.csize > 1) ? (sd_fs.csize >> 1) : 512);
		sd_unmount();
	}

	btn_wait();
}

void print_tsec_key()
{
	gfx_clear_partial_grey(BG_COL, 0, 1256);
	gfx_con_setpos(0, 0);

	u32 retries = 0;

	tsec_ctxt_t tsec_ctxt;
	sdmmc_storage_t storage;
	sdmmc_t sdmmc;

	sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);

	// Read package1.
	u8 *pkg1 = (u8 *)malloc(0x40000);
	sdmmc_storage_set_mmc_partition(&storage, 1);
	sdmmc_storage_read(&storage, 0x100000 / NX_EMMC_BLOCKSIZE, 0x40000 / NX_EMMC_BLOCKSIZE, pkg1);
	sdmmc_storage_end(&storage);
	const pkg1_id_t *pkg1_id = pkg1_identify(pkg1);
	if (!pkg1_id)
	{
		EPRINTF("Unknown pkg1 version for reading\nTSEC firmware.");
		goto out_wait;
	}

	u8 keys[0x10 * 2];
	memset(keys, 0x00, 0x20);

	tsec_ctxt.fw = (u8 *)pkg1 + pkg1_id->tsec_off;
	tsec_ctxt.pkg1 = pkg1;
	tsec_ctxt.pkg11_off = pkg1_id->pkg11_off;
	tsec_ctxt.secmon_base = pkg1_id->secmon_base;

	if (pkg1_id->kb <= KB_FIRMWARE_VERSION_600)
		tsec_ctxt.size = 0xF00;
	else if (pkg1_id->kb == KB_FIRMWARE_VERSION_620)
		tsec_ctxt.size = 0x2900;
	else if (pkg1_id->kb == KB_FIRMWARE_VERSION_700)
	{
		tsec_ctxt.size = 0x3000;
		// Exit after TSEC key generation.
		*((vu16 *)((u32)tsec_ctxt.fw + 0x2DB5)) = 0x02F8;
	}
	else
		tsec_ctxt.size = 0x3300;

	if (pkg1_id->kb == KB_FIRMWARE_VERSION_620)
	{
		u8 *tsec_paged = (u8 *)page_alloc(3);
		memcpy(tsec_paged, (void *)tsec_ctxt.fw, tsec_ctxt.size);
		tsec_ctxt.fw = tsec_paged;
	}

	int res = 0;

	while (tsec_query(keys, pkg1_id->kb, &tsec_ctxt) < 0)
	{
		memset(keys, 0x00, 0x20);

		retries++;

		if (retries > 3)
		{
			res = -1;
			break;
		}
	}

	gfx_printf("%kTSEC key:  %k", INFO_TEXT_COL, MAIN_TEXT_COL);

	if (res >= 0)
	{
		for (u32 j = 0; j < 0x10; j++)
			gfx_printf("%02X", keys[j]);
		

		if (pkg1_id->kb == KB_FIRMWARE_VERSION_620)
		{
			gfx_printf("\n%kTSEC root: %k", INFO_TEXT_COL, MAIN_TEXT_COL);
			for (u32 j = 0; j < 0x10; j++)
				gfx_printf("%02X", keys[0x10 + j]);
		}
	}
	else
		EPRINTFARGS("ERROR %X\n", res);

	gfx_puts("\n\nPress POWER to dump them to SD Card.\nPress VOL to go to the menu.\n");

	u32 btn = btn_wait();
	if (btn & BTN_POWER)
	{
		if (sd_mount())
		{
			char path[64];
			emmcsn_path_impl(path, "/dumps", "tsec_keys.bin", NULL);
			if (!sd_save_to_file(keys, 0x10 * 2, path))
				gfx_puts("\nDone!\n");
			sd_unmount();
		}
	}
	else
		goto out;

out_wait:
	btn_wait();

out:
	free(pkg1);
}

void print_fuel_gauge_info()
{
	int value = 0;

	gfx_printf("%kFuel Gauge IC Info:\n%k", INFO_TEXT_COL, MAIN_TEXT_COL);

	max17050_get_property(MAX17050_RepSOC, &value);
	gfx_printf("Capacity now:           %3d%\n", value >> 8);

	max17050_get_property(MAX17050_RepCap, &value);
	gfx_printf("Capacity now:           %4d mAh\n", value);

	max17050_get_property(MAX17050_FullCAP, &value);
	gfx_printf("Capacity full:          %4d mAh\n", value);

	max17050_get_property(MAX17050_Current, &value);
	if (value >= 0)
		gfx_printf("Current now:            %d mA\n", value / 1000);
	else
		gfx_printf("Current now:            -%d mA\n", ~value / 1000);

	max17050_get_property(MAX17050_AvgCurrent, &value);
	if (value >= 0)
		gfx_printf("Current average:        %d mA\n", value / 1000);
	else
		gfx_printf("Current average:        -%d mA\n", ~value / 1000);

	max17050_get_property(MAX17050_VCELL, &value);
	gfx_printf("Voltage now:            %4d mV\n", value);

	max17050_get_property(MAX17050_TEMP, &value);
	if (value >= 0)
		gfx_printf("Battery temperature:    %d.%d oC\n", value / 10, value % 10);
	else
		gfx_printf("Battery temperature:    -%d.%d oC\n", ~value / 10, (~value) % 10);
}

void print_battery_charger_info()
{
	int value = 0;

	gfx_printf("%k\n\nBattery Info:\n%k", INFO_TEXT_COL, MAIN_TEXT_COL);

	bq24193_get_property(BQ24193_ChargeStatus, &value);
	gfx_printf("Charge status:             ");
	switch (value)
	{
	case 0:
		gfx_printf("Not charging\n");
		break;
	case 1:
		gfx_printf("Pre-charging\n");
		break;
	case 2:
		gfx_printf("Fast charging\n");
		break;
	case 3:
		gfx_printf("Charge terminated\n");
		break;
	default:
		gfx_printf("Unknown (%d)\n", value);
		break;
	}
}

void print_battery_info()
{
	gfx_clear_partial_grey(BG_COL, 0, 1256);
	gfx_con_setpos(0, 0);

	print_fuel_gauge_info();

	print_battery_charger_info();

	u8 *buf = (u8 *)malloc(0x100 * 2);

	gfx_printf("%k\n\nBattery Fuel Gauge Registers:\n%k", INFO_TEXT_COL, MAIN_TEXT_COL);

	for (int i = 0; i < 0x200; i += 2)
	{
		i2c_recv_buf_small(buf + i, 2, I2C_1, MAXIM17050_I2C_ADDR, i >> 1);
		usleep(2500);
	}

	gfx_hexdump(0, (u8 *)buf, 0x200);

	gfx_puts("\nPress Any Key.\n");

	btn_wait();

	free(buf);
}

void _ipatch_process(u32 offset, u32 value)
{
	gfx_printf("%8x %8x", BOOTROM_BASE + offset, value);
	u8 lo = value & 0xff;
	switch (value >> 8)
	{
	case 0x20:
		gfx_printf("    MOVS R0, #0x%02X", lo);
		break;
	case 0xDF:
		gfx_printf("    SVC #0x%02X", lo);
		break;
	}
	gfx_puts("\n");
}

void bootrom_ipatches_info()
{
	gfx_clear_partial_grey(BG_COL, 0, 1256);
	gfx_con_setpos(0, 0);
	
	u32 res = fuse_read_ipatch(_ipatch_process);
	if (res != 0)
		EPRINTFARGS("Failed to read ipatches. Error: %d", res);

	gfx_puts("\nPress any key.\n");
	

		btn_wait();
	}

#pragma GCC pop_options