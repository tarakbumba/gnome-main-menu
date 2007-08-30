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

#ifndef __GRID_VIEW_H__
#define __GRID_VIEW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GRID_VIEW_TYPE         (grid_view_get_type ())
#define GRID_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GRID_VIEW_TYPE, GridView))
#define GRID_VIEW_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    GRID_VIEW_TYPE, GridViewClass))
#define IS_GRID_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GRID_VIEW_TYPE))
#define IS_GRID_VIEW_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    GRID_VIEW_TYPE))
#define GRID_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  GRID_VIEW_TYPE, GridViewClass))

typedef struct {
	GtkViewport gtk_viewport;
} GridView;

typedef struct {
	GtkViewportClass gtk_viewport_class;
} GridViewClass;

GType grid_view_get_type (void);

GtkWidget *grid_view_new_with_model (GtkTreeModel *model);

G_END_DECLS

#endif
