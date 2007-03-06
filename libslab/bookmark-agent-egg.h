/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2007 Novell, Inc.
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

#ifndef __BOOKMARK_AGENT_EGG_H__
#define __BOOKMARK_AGENT_EGG_H__

#include <glib.h>

G_BEGIN_DECLS

#include "eggbookmarkfile.h"

#define GBookmarkFile                    EggBookmarkFile
                                                                            
#define g_bookmark_file_new              egg_bookmark_file_new
#define g_bookmark_file_free             egg_bookmark_file_free
                                                                            
#define g_bookmark_file_load_from_file   egg_bookmark_file_load_from_file
#define g_bookmark_file_to_file          egg_bookmark_file_to_file
                                                                            
#define g_bookmark_file_has_item         egg_bookmark_file_has_item
#define g_bookmark_file_remove_item      egg_bookmark_file_remove_item
#define g_bookmark_file_get_uris         egg_bookmark_file_get_uris
#define g_bookmark_file_get_size         egg_bookmark_file_get_size
#define g_bookmark_file_get_title        egg_bookmark_file_get_title
#define g_bookmark_file_set_title        egg_bookmark_file_set_title
#define g_bookmark_file_get_mime_type    egg_bookmark_file_get_mime_type
#define g_bookmark_file_set_mime_type    egg_bookmark_file_set_mime_type
#define g_bookmark_file_get_modified     egg_bookmark_file_get_modified
#define g_bookmark_file_set_modified     egg_bookmark_file_set_modified
#define g_bookmark_file_get_icon         egg_bookmark_file_get_icon
#define g_bookmark_file_set_icon         egg_bookmark_file_set_icon
#define g_bookmark_file_get_applications egg_bookmark_file_add_application
#define g_bookmark_file_add_application  egg_bookmark_file_get_applications
#define g_bookmark_file_get_app_info     egg_bookmark_file_get_app_info
#define g_bookmark_file_get_groups       egg_bookmark_file_get_groups
#define g_bookmark_file_add_group        egg_bookmark_file_add_group
#define g_bookmark_file_move_item        egg_bookmark_file_move_item

G_END_DECLS

#endif
