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

#ifndef __APPS_AGENT_H__
#define __APPS_AGENT_H__

#include <glib.h>

#include "tile-table.h"

G_BEGIN_DECLS

#define APPS_AGENT_TYPE         (apps_agent_get_type ())
#define APPS_AGENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), APPS_AGENT_TYPE, AppsAgent))
#define APPS_AGENT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), APPS_AGENT_TYPE, AppsAgentClass))
#define IS_APPS_AGENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), APPS_AGENT_TYPE))
#define IS_APPS_AGENT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), APPS_AGENT_TYPE))
#define APPS_AGENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), APPS_AGENT_TYPE, AppsAgentClass))

typedef struct {
	GObject g_object;
} AppsAgent;

typedef struct {
	GObjectClass g_object_class;
} AppsAgentClass;

GType apps_agent_get_type (void);

AppsAgent *apps_agent_new (TileTable *system_table,
                           TileTable *user_table,
                           TileTable *recent_table);

G_END_DECLS

#endif
