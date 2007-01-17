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

#ifndef __SLAB_WINDOW_H__
#define __SLAB_WINDOW_H__

#include <glib.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkbox.h>

G_BEGIN_DECLS

#define SLAB_WINDOW_TYPE         (slab_window_get_type ())
#define SLAB_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SLAB_WINDOW_TYPE, SlabWindow))
#define SLAB_WINDOW_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), SLAB_WINDOW_TYPE, SlabWindowClass))
#define IS_SLAB_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SLAB_WINDOW_TYPE))
#define IS_SLAB_WINDOW_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), SLAB_WINDOW_TYPE))
#define SLAB_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SLAB_WINDOW_TYPE, SlabWindowClass))

typedef struct _SlabWindow SlabWindow;
typedef struct _SlabWindowClass SlabWindowClass;

struct _SlabWindow
{
	GtkWindow window;

	GtkBox *_hbox;
	GtkBox *_vbox;
	GtkWidget *_top_pane;
	GtkWidget *_bottom_pane;
	GtkWidget *_right_pane;
};

struct _SlabWindowClass
{
	GtkWindowClass parent_class;
};

GType slab_window_get_type (void);
GtkWidget *slab_window_new (void);
void slab_window_set_contents (SlabWindow *window, GtkWidget *top_pane, GtkWidget *bottom_pane, GtkWidget *right_pane);

G_END_DECLS
#endif /* __SLAB_WINDOW_H__ */
