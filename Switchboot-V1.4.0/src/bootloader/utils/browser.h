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

#ifndef _BROWSER_H_
#define _BROWSER_H_
#include "../utils/types.h"
char *file_browser(char* start_dir, char* required_extn, char* browser_caption, bool get_dir_only);
int copy_file (char* src_file, char* dst_file);
#endif
