/*
 * This file is part of libslab.
 *
 * Copyright (c) 2006 Novell, Inc.
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

#ifndef __DOCUMENT_TILE_H__
#define __DOCUMENT_TILE_H__

#include "nameplate-tile.h"

#include "egg-recent-item.h"

G_BEGIN_DECLS
#define DOCUMENT_TILE_TYPE         (document_tile_get_type ())
#define DOCUMENT_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DOCUMENT_TILE_TYPE, DocumentTile))
#define DOCUMENT_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), DOCUMENT_TILE_TYPE, DocumentTileClass))
#define IS_DOCUMENT_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DOCUMENT_TILE_TYPE))
#define IS_DOCUMENT_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), DOCUMENT_TILE_TYPE))
#define DOCUMENT_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DOCUMENT_TILE_TYPE, DocumentTileClass))
	typedef struct {
	NameplateTile nameplate_tile;
} DocumentTile;

typedef struct {
	NameplateTileClass nameplate_tile_class;
} DocumentTileClass;

#define DOCUMENT_TILE_ACTION_OPEN_WITH_DEFAULT    0
#define DOCUMENT_TILE_ACTION_OPEN_IN_FILE_MANAGER 1
#define DOCUMENT_TILE_ACTION_RENAME               2
#define DOCUMENT_TILE_ACTION_MOVE_TO_TRASH        3
#define DOCUMENT_TILE_ACTION_DELETE               4
#define DOCUMENT_TILE_ACTION_SEND_TO              5

GType document_tile_get_type (void);

GtkWidget *document_tile_new (EggRecentItem * recent_item);

G_END_DECLS
#endif
