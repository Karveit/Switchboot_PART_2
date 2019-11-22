/*
 * Copyright (c) 2018 Rajko Stojadinovic
 * Copyright (c) 2018 CTCaer
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

#ifndef _FE_EMMC_TOOLS_H_
#define _FE_EMMC_TOOLS_H_

void backup_emmc_all();
void backup_emmc_rawnand();
void backup_emmc_boot0();
void backup_emmc_boot1();
void backup_emmc_prodinfo();
void backup_emmc_prodinfof();
void backup_emmc_2_1();
void backup_emmc_2_2();
void backup_emmc_2_3();
void backup_emmc_2_4();
void backup_emmc_2_5();
void backup_emmc_2_6();
void backup_emmc_safe();
void backup_emmc_system();
void backup_emmc_user();

void restore_emmc_all();
void restore_emmc_rawnand();
void restore_emmc_boot0();
void restore_emmc_boot1();
void restore_emmc_prodinfo();
void restore_emmc_prodinfof();
void restore_emmc_2_1();
void restore_emmc_2_2();
void restore_emmc_2_3();
void restore_emmc_2_4();
void restore_emmc_2_5();
void restore_emmc_2_6();
void restore_emmc_safe();
void restore_emmc_system();
void restore_emmc_user();

void restore_emmc_boot0_noszchk();
void restore_emmc_boot1_noszchk();
void restore_emmc_prodinfo_noszchk();
void restore_emmc_prodinfof_noszchk();
void restore_emmc_2_1_noszchk();
void restore_emmc_2_2_noszchk();
void restore_emmc_2_3_noszchk();
void restore_emmc_2_4_noszchk();
void restore_emmc_2_5_noszchk();
void restore_emmc_2_6_noszchk();
void restore_emmc_safe_noszchk();
void restore_emmc_system_noszchk();
void restore_emmc_user_noszchk();
void perform_minimal_backup();
void perform_minimal_restore();

void restore_license_dat();
void reboot_samd21_update();
void reboot_memloader();
#endif
