/*
 * This file is part of libslab.
 *
 * Copyright (c) 2007 Novell, Inc.
 *
 * Libslab is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libslab is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __MENU_BROWSER_H__
#define __MENU_BROWSER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MENU_BROWSER_TYPE         (menu_browser_get_type ())
#define MENU_BROWSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MENU_BROWSER_TYPE, MenuBrowser))
#define MENU_BROWSER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    MENU_BROWSER_TYPE, MenuBrowserClass))
#define IS_MENU_BROWSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MENU_BROWSER_TYPE))
#define IS_MENU_BROWSER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    MENU_BROWSER_TYPE))
#define MENU_BROWSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  MENU_BROWSER_TYPE, MenuBrowserClass))

#define MENU_BROWSER_MENU_TREE_PROP "menu-tree"

typedef struct {
	GtkAlignment gtk_alignment;
} MenuBrowser;

typedef struct {
	GtkAlignmentClass gtk_alignment_class;
} MenuBrowserClass;

GType menu_browser_get_type (void);

GtkWidget *menu_browser_new (const gchar *menu_name);

G_END_DECLS

#endif
