/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * The Main Menu is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Main Menu is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Main Menu; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __RECENT_FILES_H__
#define __RECENT_FILES_H__

#include <glib.h>

#define RECENT_FILES_FILE_PATH ".recently-used"
#define RECENT_APPS_FILE_PATH  ".recently-used-apps"

G_BEGIN_DECLS GList * get_recent_files (const gchar * store_path);

G_END_DECLS
#endif /* __RECENT_FILES_H__ */
