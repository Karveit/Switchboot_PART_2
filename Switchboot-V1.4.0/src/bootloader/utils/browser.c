/*
 * Copyright (c) 2019 Mattytrog
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

#include "browser.h"
#include "../config/config.h"
#include "../config/ini.h"
#include "../gfx/gfx.h"
#include "../gfx/tui.h"
#include "../libs/fatfs/ff.h"
#include "../mem/heap.h"
#include "../sec/se.h"
#include "../storage/nx_emmc.h"
#include "../storage/sdmmc.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "../utils/dirlist.h"

extern bool sd_mount();
extern void sd_unmount();
extern char *browse_dir;
extern menu_t menu_top;

char *file_browser(char* start_dir, char* required_extn, char* browser_caption, bool get_dir_only){
	
u8 max_entries = 57;
	char *browse_dir = malloc(256);
	char *filelist = malloc(256);
	char *file_sec = malloc(256);
	char *file_sec_untrimmed = malloc(256);
	char *select_display = malloc(256);
	bool dir_selected = false;
	//browse_dir = (char*)malloc(512);
	FILINFO fno;
	if(!browser_caption) browser_caption = (char *)"Select A File";
	if(start_dir){
	memcpy(browse_dir + 0, start_dir, strlen (start_dir) +1);
	}
	else {memcpy(browse_dir + 0, "", 1);}
		
	
	ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 6));

	gfx_clear_grey(0x1B);
	gfx_con_setpos(0, 0);
	
	
start:

				
		while(true){
			
			memcpy(select_display+0, "SD:", 4);
			memcpy(select_display+ strlen(select_display), browse_dir, strlen(browse_dir)+1);
			filelist = dirlist(browse_dir, NULL, false, true);
			u32 i = 0;
			
			
				

			if (filelist)
			{
				// Build configuration menu.
				ments[0].type = MENT_CAPTION;
				ments[0].caption = "Choose Path:";
				ments[0].color = 0xFFFFFFFF;
				ments[1].type = MENT_DATA;
				ments[1].caption = select_display;
				ments[1].data = "<selected_dir>";
				//ments[0].handler = exit_loop;
				ments[2].type = MENT_CHGLINE;
				ments[3].type = MENT_DATA;
				ments[3].caption = "Main Menu";
				ments[3].data = "<home_menu>";
				ments[4].type = MENT_CHGLINE;
				ments[5].type = MENT_DATA;
				ments[5].caption = "Parent (..)";
				ments[5].data = "<go_back>";
				ments[6].type = MENT_CHGLINE;
				

				while (true)
				{
					if (i > max_entries || !filelist[i * 256])
						break;
					ments[i + 7].type = INI_CHOICE;
					ments[i + 7].caption = &filelist[i * 256];
					ments[i + 7].data = &filelist[i * 256];

					i++;
				}
			}
						
			if (i > 0)
			{
				memset(&ments[i + 7], 0, sizeof(ment_t));
				menu_t menu = {
						ments,
						browser_caption, 0, 0
				};
				//free(file_sec_untrimmed);
				file_sec_untrimmed = (char *)tui_do_menu(&menu);
				
				if (strlen(file_sec_untrimmed) == 0){break;}
				
				if (!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 11,"<home_menu>")) {tui_do_menu(&menu_top);}
				if (!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 14,"<selected_dir>")) {dir_selected = true; break;}
				if (!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 9,"<go_back>"))
				{
					char *back_trim_len = strrchr (browse_dir, '/');
					memcpy(browse_dir + (strlen(browse_dir) - strlen(back_trim_len)), "\0", 2);
				} else if (!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 7," <Dir> "))
				{
					memcpy (file_sec + 0, file_sec_untrimmed, strlen(file_sec_untrimmed) - 7);
					memcpy (file_sec + strlen(file_sec_untrimmed) - 7, "\0", 2);//nullterm
					memcpy(browse_dir+strlen(browse_dir), "/", 2);
					memcpy(browse_dir+strlen(browse_dir), file_sec, strlen (file_sec) + 1);
				} else if(!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 7,"       ")){
					memcpy (file_sec + 0, file_sec_untrimmed, strlen(file_sec_untrimmed) - 7);
					memcpy (file_sec + strlen(file_sec_untrimmed) - 7, "\0", 2);//nullterm
					memcpy(browse_dir+strlen(browse_dir), "/", 2);
					memcpy(browse_dir+strlen(browse_dir), file_sec, strlen (file_sec) + 1);
					break;}
			}
			
			
			
			if (i == 0){
				break;
			}
			
			
		}


if(required_extn)
	{
	if ((!dir_selected) &&(strcmp(browse_dir+strlen(browse_dir) - strlen(required_extn), required_extn)))
			{gfx_printf(" No %s file in this location.", required_extn); msleep(2000); browse_dir[0] = '\0';goto start;}	
	}
if(get_dir_only){	
	if(!f_stat(browse_dir, &fno))
			{
				if (!(fno.fattrib & AM_DIR))
					{
						char *back_trim = strrchr (browse_dir, '/');
						memcpy (browse_dir+(strlen(browse_dir) - strlen(back_trim)), "\0", 2);
					}
			}
}
		
if (strlen(browse_dir) < 1) {
	browse_dir[1] = '\0';
	gfx_printf(" Nothing selected. Exiting."); msleep(2000);
	goto out2;
	}

out2:
	gfx_clear_grey(0x00);
	gfx_con_setpos(0, 0);
	
	free (file_sec_untrimmed);
	free (file_sec);
	free (select_display);
	return browse_dir;
}

int copy_file (char* src_file, char* dst_file){
	FILINFO fno;
    FIL fsrc, fdst;      
    BYTE buffer[8192000];  //8mb buff 
    FRESULT fr;          
    UINT br, bw;
	
	u64 src_size;
	u64 dst_size;
	
	u8 btn = btn_wait_timeout(0, BTN_VOL_DOWN | BTN_VOL_UP);
					if (btn & BTN_VOL_DOWN || btn & BTN_VOL_UP)
						{
							f_close(&fsrc);
							f_close(&fdst);
							gfx_printf ("\nCancelled.");
							msleep(1000);
							return 1;
						}
	
	//check if exists already first

	if (!f_stat(dst_file, &fno)){
		if (!f_stat(src_file, &fno))
		{
		dst_size = f_size(&fdst);
		src_size = f_size(&fsrc);
		if (dst_size == src_size) {
			f_close(&fdst);
			f_unlink(dst_file);
			}
		}
	}
	
	fr = f_open(&fsrc, src_file, FA_READ);	//open src
			if (fr)
				{ 
				EPRINTFARGS ("\nStopped. Last file:\n%s\n%s", src_file);
				btn_wait(); return 1;
				}
	src_size = f_size(&fsrc);
	fr = f_open(&fdst, dst_file, FA_WRITE | FA_CREATE_ALWAYS);
			if (fr)
				{ 
				EPRINTFARGS ("\nFailed creating dest file\n%s", dst_file);
				btn_wait(); return 1;
				}
	dst_size = f_size(&fdst);			
	
	gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);
	u16 resultpercent = 0;
	u8 count = 0;
				for (;;) 
				{
					
					fr = f_read(&fsrc, buffer, sizeof buffer, &br);
					
					if (fr || br == 0) break;
							
					fr = f_write(&fdst, buffer, br, &bw);
					//update every 10 buffer cycles
					if(count == 10){
					dst_size = f_size(&fdst);
					resultpercent = ((dst_size * 100) / (dst_size + src_size)* 2) ; 
					gfx_con_setpos(gfx_con.savedx, gfx_con.savedy);
					gfx_printf ("%d%  ", resultpercent);
					count = 0;
					}
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
							return 1;
						}
					gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy);
					if (gfx_con.savedy >= 1200){
						gfx_clear_grey(0x00);
						gfx_con_setpos(0, 0);
						}
						++count;
				}						
				f_close(&fsrc);
				f_close(&fdst);		
	return 0;
}