/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006, 2007 Novell, Inc.
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

#ifndef __USER_DIRS_TILE_TABLE_H__
#define __USER_DIRS_TILE_TABLE_H__

#include <gtk/gtk.h>

#include "tile-table.h"

G_BEGIN_DECLS

#define USER_DIRS_TILE_TABLE_TYPE         (user_dirs_tile_table_get_type ())
#define USER_DIRS_TILE_TABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), USER_DIRS_TILE_TABLE_TYPE, UserDirsTileTable))
#define USER_DIRS_TILE_TABLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), USER_DIRS_TILE_TABLE_TYPE, UserDirsTileTableClass))
#define IS_USER_DIRS_TILE_TABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), USER_DIRS_TILE_TABLE_TYPE))
#define IS_USER_DIRS_TILE_TABLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), USER_DIRS_TILE_TABLE_TYPE))
#define USER_DIRS_TILE_TABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), USER_DIRS_TILE_TABLE_TYPE, UserDirsTileTableClass))

typedef struct {
	TileTable tile_table;
} UserDirsTileTable;

typedef struct {
	TileTableClass tile_table_class;
} UserDirsTileTableClass;

GType user_dirs_tile_table_get_type (void);

GtkWidget *user_dirs_tile_table_new (void);

G_END_DECLS

#endif
