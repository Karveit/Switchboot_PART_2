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

//start_dir = dir to start browsing from
//required_extn = files of certain extension only
//browser_caption = Title in browser
//get_dir_only = will only return dir. If a file is chosen, it will return its containing directory
//on fail, automatically go to sd root

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
extern void ipl_main();
char *file_browser(const char* start_dir, const char* required_extn, const char* browser_caption, const bool get_dir_only, const bool goto_root_on_fail){
	
	u8 max_entries = 55;
	char *browse_dir = malloc(256);
	
	
	//browse_dir = (char*)malloc(512);
	DIR dir;
	FILINFO fno;
	
	if(start_dir){
	memcpy(browse_dir + 0, start_dir, strlen (start_dir) +1);
	}
	else {memcpy(browse_dir + 0, "", 1);}
	
start:
	
	if(!browser_caption) browser_caption = (char *)"Select A File";
	bool home_selected = false;
	bool select_item = false;
	bool file_selected = false;
	bool back_selected = false;
	char *filelist = (char *)calloc(max_entries, 256);
	
	char *file_sec = malloc(256);
	char *file_sec_untrimmed = malloc(256);
	char *select_display = malloc(256);
	int res = 0;
	u32 i = 0;
	//u32 j = 0, k = 0;
	//char *temp = (char *)calloc(1, 256);
	ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 9));

	gfx_clear_grey(BG_COL);
	gfx_con_setpos(0, 0);
	
				
		while(true)
		{
			gfx_clear_grey(BG_COL);
			gfx_con_setpos(0, 0);
			i = 0; 
			//j = 0, k = 0;
			
			memcpy(select_display+0, "SD:", 4);
			memcpy(select_display+ strlen(select_display), browse_dir, strlen(browse_dir)+1);
			if (!f_opendir(&dir, browse_dir))
			{
				for (;;)
				{
					res = f_readdir(&dir, &fno);
					if (res || !fno.fname[0])
						break;

						
					if (fno.fattrib & AM_DIR){
					memcpy(fno.fname + strlen(fno.fname), " <Dir> ", 8);
					memcpy(filelist + ((i + 9) * 256), fno.fname, strlen(fno.fname) + 1);
					}
					
					if (!(fno.fattrib & AM_DIR))
					{

						{
							//memcpy(fno.fname + strlen(fno.fname), "       ", 8);
							memcpy(filelist + ((i + 9) * 256), fno.fname, strlen(fno.fname) + 1);
						}
					}
						
					i++;
					if ((i + 9) > (max_entries - 1))
						break;
				}
			f_closedir(&dir);
			}
			//alphabetical order. Not worth it due to size
			/*k = i; for (i = 0; i < k - 1 ; i++)
				{
					for (j = i + 1; j < k; j++)
					{
						if (strcmp(&filelist[(i+9) * 256], &filelist[(j+9) * 256]) > 0) 
						{
							memcpy(temp, &filelist[(i+9) * 256], strlen(&filelist[(i+9) * 256]) + 1);
							memcpy(&filelist[(i+9) * 256], &filelist[(j+9) * 256], strlen(&filelist[(j+9) * 256]) + 1);
							memcpy(&filelist[(j+9) * 256], temp, strlen(temp) + 1);
						}
					}
				}
				*/
			i = 0;
			
			
				

			if (filelist)
			{
				// Build configuration menu.
				ments[0].type = MENT_CAPTION;
				ments[0].caption = "Select Path:";
				ments[0].color = INFO_TEXT_COL;
				ments[1].type = MENT_CHGLINE;
				ments[2].type = MENT_DATA;
				ments[2].caption = select_display;
				ments[2].data = "<selected_dir>";
				//ments[0].handler = exit_loop;
				ments[3].type = MENT_CHGLINE;
				ments[4].type = MENT_CHGLINE;
				ments[5].type = MENT_DATA;
				ments[5].caption = "Exit";
				ments[5].data = "<home_menu>";
				ments[6].type = MENT_CHGLINE;
				ments[7].type = MENT_DATA;
				ments[7].caption = "Parent (..)";
				ments[7].data = "<go_back>";
				ments[8].type = MENT_CHGLINE;
				

				while (true)
				{
					if (i > max_entries || !filelist[(i + 9) * 256])
						break;
					ments[i + 9].type = INI_CHOICE;
					ments[i + 9].caption = &filelist[(i + 9) * 256];
					ments[i + 9].data = &filelist[(i + 9) * 256];

					i++;
				}
			}
						
			if (i > 0)
			{
				memset(&ments[i + 9], 0, sizeof(ment_t));
				menu_t menu = {
						ments,
						browser_caption, 0, 0
				};
				//free(file_sec_untrimmed);
				file_sec_untrimmed = (char *)tui_do_menu(&menu);
				
				if (strlen(file_sec_untrimmed) == 0){break;}
				//home
				if (!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 11,"<home_menu>")) home_selected = true;
				//chosen file
				if (!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 14,"<selected_dir>")) select_item = true;
				//parent
				if (!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 9,"<go_back>"))
				{
					char *back_trim_len = strrchr (browse_dir, '/');
					memcpy(browse_dir + (strlen(browse_dir) - strlen(back_trim_len)), "\0", 2);
					if(strlen(back_trim_len) == 1024) memcpy(browse_dir + 0, "\0", 2);
					back_selected = true;
					
				}
				//directory selected to enter
				if (!strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 7," <Dir> "))
				{
					memcpy (file_sec + 0, file_sec_untrimmed, strlen(file_sec_untrimmed) - 7);
					memcpy (file_sec + strlen(file_sec_untrimmed) - 7, "\0", 2);//nullterm
					memcpy(browse_dir+strlen(browse_dir), "/", 2);
					memcpy(browse_dir+strlen(browse_dir), file_sec, strlen (file_sec) + 1);
					filelist = NULL;
					free (file_sec_untrimmed);
					free (file_sec);
					free (select_display);
					goto start;
					//if (f_stat(browse_dir, &fno)) break;
					
				} else if(strcmp(file_sec_untrimmed + strlen(file_sec_untrimmed) - 1,">") != 0)
				{
					//file_selected
					memcpy (file_sec + 0, file_sec_untrimmed, strlen(file_sec_untrimmed));// - 7);
					memcpy (file_sec + strlen(file_sec_untrimmed), "\0", 2);//nullterm
					memcpy(browse_dir+strlen(browse_dir), "/", 2);
					memcpy(browse_dir+strlen(browse_dir), file_sec, strlen (file_sec) + 1);
					file_selected = true;
				}
				
				//end of menu selection
				
				//start of interpret chosen option
				
				if (home_selected || select_item) break;
				
				
				if (file_selected && get_dir_only)
				{
					if(!f_stat(browse_dir, &fno))
					{
						if (fno.fattrib & AM_DIR) break;
						else
						{
							char *back_trim = strrchr (browse_dir, '/');
							memcpy (browse_dir+(strlen(browse_dir) - strlen(back_trim)), "\0", 2);
							break;
						} 
					}
				}

				if (file_selected && !get_dir_only) break;

			}
			
			
			
			
			
			if (i == 0) break;
			
		}
		
	gfx_clear_grey(BG_COL);
	gfx_con_setpos(0, 0);
	
	
	if (!home_selected && goto_root_on_fail && !select_item && !file_selected && !back_selected)
			{
				gfx_printf("\nNo files here.\n\nGo to SD ROOT?\n\n[PWR] or [VOL+] Yes\n[VOL-] Return to menu");
				u8 btn = btn_wait();
				if(btn & BTN_VOL_DOWN) return NULL;
				
				memcpy(browse_dir + 0, "\0", 1);
				start_dir = NULL;
				file_sec_untrimmed = NULL;
				file_sec = NULL; 
				select_display = NULL;
				goto start;
			}
	
	
	if (!home_selected && required_extn && strcmp(required_extn, browse_dir + strlen(browse_dir) - strlen(required_extn)))
			{
				gfx_printf("\nExtension incorrect or directory selected.\n\nConfirm selection?\n\n[PWR] Yes\n[VOL-] No, choose another");
				u8 btn = btn_wait();
				if(btn & BTN_POWER) return browse_dir;
				else
				{
					memcpy(browse_dir + 0, "\0", 1);
					start_dir = NULL;
					file_sec_untrimmed = NULL;
					file_sec = NULL; 
					select_display = NULL;
					goto start;
				}
			}
	free (file_sec_untrimmed);
	free (file_sec);
	free (select_display);		
			
	if(home_selected) {free (browse_dir); return NULL;}
	return browse_dir;
}

/*int copy_file (char* src_file, char* dst_file){
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
						gfx_clear_grey(BG_COL);
						gfx_con_setpos(0, 0);
						}
						++count;
				}						
				f_close(&fsrc);
				f_close(&fdst);		
	return 0;
}*/