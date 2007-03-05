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

#ifndef __BOOKMARK_AGENT_H__
#define __BOOKMARK_AGENT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define BOOKMARK_AGENT_TYPE         (bookmark_agent_get_type ())
#define BOOKMARK_AGENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BOOKMARK_AGENT_TYPE, BookmarkAgent))
#define BOOKMARK_AGENT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), BOOKMARK_AGENT_TYPE, BookmarkAgentClass))
#define IS_BOOKMARK_AGENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BOOKMARK_AGENT_TYPE))
#define IS_BOOKMARK_AGENT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), BOOKMARK_AGENT_TYPE))
#define BOOKMARK_AGENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BOOKMARK_AGENT_TYPE, BookmarkAgentClass))

#define BOOKMARK_AGENT_UPDATE_SIGNAL     "update"
#define BOOKMARK_AGENT_STORE_STATUS_PROP "store-status"
#define BOOKMARK_AGENT_ITEMS_PROP        "items"

typedef struct {
	gchar  *uri;
	gchar  *title;
	gchar  *mime_type;
	time_t  mtime;
	gchar  *app_name;
	gchar  *app_exec;
} BookmarkItem;

typedef enum {
	BOOKMARK_STORE_DEFAULT_ONLY,
	BOOKMARK_STORE_DEFAULT,
	BOOKMARK_STORE_USER
} BookmarkStoreStatus;

typedef struct {
	GObject g_object;
} BookmarkAgent;

typedef struct {
	GObjectClass g_object_class;

	void (* update) (BookmarkAgent *);
} BookmarkAgentClass;

GType bookmark_agent_get_type (void);

BookmarkAgent *bookmark_agent_get_instance  (void);
gboolean       bookmark_agent_has_item      (BookmarkAgent *this, const gchar *uri);
void           bookmark_agent_add_item      (BookmarkAgent *this, const BookmarkItem *item);
void           bookmark_agent_remove_item   (BookmarkAgent *this, const gchar *uri);
void           bookmark_agent_reorder_items (BookmarkAgent *this, const gchar **uris);

G_END_DECLS

#endif
