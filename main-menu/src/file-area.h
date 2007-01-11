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

#ifndef __FILE_AREA_H__
#define __FILE_AREA_H__

#include <gtk/gtk.h>

#include "tile-table.h"

G_BEGIN_DECLS

#define FILE_AREA_TYPE           (file_area_get_type ())
#define FILE_AREA(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILE_AREA_TYPE, FileArea))
#define FILE_AREA_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c), FILE_AREA_TYPE, FileAreaClass))
#define IS_FILE_AREA(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILE_AREA_TYPE))
#define IS_FILE_AREA_CLASS(c)    (G_TYPE_CHECK_CLASS_TYPE ((c), FILE_AREA_TYPE))
#define FILE_AREA_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FILE_AREA_TYPE, FileAreaClass))

#define FILE_AREA_USER_HDR_PROP         "user-header"
#define FILE_AREA_RECENT_HDR_PROP       "recent-header"
#define FILE_AREA_MAX_TOTAL_ITEMS_PROP  "max-total-items"
#define FILE_AREA_MIN_RECENT_ITEMS_PROP "min-recent-items"

typedef struct {
	GtkVBox vbox;

	TileTable *user_spec_table;
	TileTable *recent_table;
} FileArea;

typedef struct {
	GtkVBoxClass vbox_class;
} FileAreaClass;

GType file_area_get_type (void);

GtkWidget *file_area_new (void);

G_END_DECLS

#endif /* __FILE_AREA_H__ */
