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

#include "tile-table.h"

#include "tile.h"

G_DEFINE_TYPE (TileTable, tile_table, GTK_TYPE_TABLE)

typedef struct {
	GList *tiles;
	
	GtkBin **bins;
	gint n_bins;
	
	gint limit;

	TileTableReorderingPriority priority;
} TileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_TABLE_TYPE, TileTablePrivate))

enum {
	PROP_0,
	PROP_TILES,
	PROP_REORDER,
	PROP_LIMIT
};

enum {
	UPDATE_SIGNAL,
	URI_ADDED_SIGNAL,
	LAST_SIGNAL
};

static guint tile_table_signals [LAST_SIGNAL] = { 0 };

static void tile_table_get_property (GObject *, guint, GValue *, GParamSpec *);
static void tile_table_set_property (GObject *, guint, const GValue *, GParamSpec *);
static void tile_table_finalize     (GObject *);

static void replace_tiles      (TileTable *, GList *);
static void set_limit          (TileTable *, gint);
static void update_bins        (TileTable *);
static void insert_into_bin    (TileTable *, Tile *, gint);
static void empty_bin          (TileTable *, gint);
static void resize_table       (TileTable *, guint, guint); 
static void emit_update_signal (TileTable *, GList *, GList *, guint32);

static void tile_activated_cb     (Tile *, TileEvent *, gpointer);
static void tile_drag_data_rcv_cb (GtkWidget *, GdkDragContext *, gint, gint,
                                   GtkSelectionData *, guint, guint, gpointer);

void
tile_table_reload (TileTable *this)
{
	if (TILE_TABLE_GET_CLASS (this)->reload)
		TILE_TABLE_GET_CLASS (this)->reload (this);
}

void
tile_table_uri_added (TileTable *this, const gchar *uri)
{
	TileTableURIAddedEvent *uri_event;

	uri_event = g_new0 (TileTableURIAddedEvent, 1);
	uri_event->time = (guint32) time;
	uri_event->uri = g_strdup (uri);

	g_signal_emit (this, tile_table_signals [URI_ADDED_SIGNAL], 0, uri_event);
}

static void
tile_table_class_init (TileTableClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	GParamSpec *tiles_pspec;
	GParamSpec *reorder_pspec;
	GParamSpec *limit_pspec;


	g_obj_class->get_property = tile_table_get_property;
	g_obj_class->set_property = tile_table_set_property;
	g_obj_class->finalize     = tile_table_finalize;

	this_class->reload    = NULL;
	this_class->update    = NULL;
	this_class->uri_added = NULL;

	tiles_pspec = g_param_spec_pointer (
		TILE_TABLE_TILES_PROP, TILE_TABLE_TILES_PROP,
		"the GList which contains the Tiles for this table",
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	reorder_pspec = g_param_spec_int (
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDER_PROP,
		"the type of reordering allowed for this table",
		TILE_TABLE_REORDERING_SWAP, TILE_TABLE_REORDERING_NONE, TILE_TABLE_REORDERING_PUSH_PULL,
		G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	limit_pspec = g_param_spec_int (
		TILE_TABLE_LIMIT_PROP, TILE_TABLE_LIMIT_PROP,
		"the maximum number of Tiles this table can hold, -1 if unlimited",
		-1, G_MAXINT, G_MAXINT,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_TILES,   tiles_pspec);
	g_object_class_install_property (g_obj_class, PROP_REORDER, reorder_pspec);
	g_object_class_install_property (g_obj_class, PROP_LIMIT,   limit_pspec);

	tile_table_signals [UPDATE_SIGNAL] = g_signal_new (
		TILE_TABLE_UPDATE_SIGNAL,
		G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (TileTableClass, update),
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

	tile_table_signals [URI_ADDED_SIGNAL] = g_signal_new (
		TILE_TABLE_URI_ADDED_SIGNAL,
		G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (TileTableClass, uri_added),
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

	g_type_class_add_private (this_class, sizeof (TileTablePrivate));
}

static void
tile_table_init (TileTable *this)
{
	TileTablePrivate *priv = PRIVATE (this);

	priv->tiles = NULL;

	priv->limit = G_MAXINT;
	priv->priority = TILE_TABLE_REORDERING_PUSH_PULL;
}

static void
tile_table_get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	TileTablePrivate *priv = PRIVATE (g_obj);

	switch (prop_id) {
		case PROP_TILES:
			g_value_set_pointer (value, priv->tiles);
			break;

		case PROP_LIMIT:
			g_value_set_int (value, priv->limit);
			break;

		default:
			break;
	}
}

static void
tile_table_set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	TileTable *this = TILE_TABLE (g_obj);
	TileTablePrivate *priv = PRIVATE (g_obj);

	switch (prop_id) {
		case PROP_TILES:
			replace_tiles (this, (GList *) g_value_get_pointer (value));
			break;

		case PROP_REORDER:
			priv->priority = g_value_get_int (value);
			break;

		case PROP_LIMIT:
			set_limit (this, g_value_get_int (value));
			break;

		default:
			break;
	}
}

static void
tile_table_finalize (GObject *g_obj)
{
	TileTablePrivate *priv = PRIVATE (g_obj);

	g_free (priv->bins);

	G_OBJECT_CLASS (tile_table_parent_class)->finalize (g_obj);
}

static void
replace_tiles (TileTable *this, GList *tiles)
{
	TileTablePrivate *priv = PRIVATE (this);

	GtkWidget *tile;
	gulong     handler_id;

	GList *node;


	for (node = priv->tiles; node; node = node->next)
		gtk_widget_destroy (GTK_WIDGET (node->data));

	g_list_free (priv->tiles);

	priv->tiles = NULL;

	for (node = tiles; node; node = node->next) {
		g_assert (IS_TILE (node->data));

		tile = GTK_WIDGET (node->data);

		handler_id = g_signal_handler_find (
			tile, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
			tile_drag_data_rcv_cb, NULL);

		if (! handler_id && priv->priority != TILE_TABLE_REORDERING_NONE) {
			gtk_drag_dest_set (
				tile, GTK_DEST_DEFAULT_ALL,
				NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);

			gtk_drag_dest_add_uri_targets (tile);

			g_signal_connect (
				G_OBJECT (tile), "drag-data-received",
				G_CALLBACK (tile_drag_data_rcv_cb), this);
		}
		else if (handler_id && priv->priority == TILE_TABLE_REORDERING_NONE)
			g_signal_handler_disconnect (tile, handler_id);
		else
			/* do nothing */ ;

		handler_id = g_signal_handler_find (
			tile, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
			tile_activated_cb, NULL);

		if (! handler_id)
			g_signal_connect (
				G_OBJECT (tile), "tile-activated",
				G_CALLBACK (tile_activated_cb), NULL);

		priv->tiles = g_list_append (priv->tiles, tile);
	}

	update_bins (this);
}

static void
set_limit (TileTable *this, gint limit)
{
	TileTablePrivate *priv = PRIVATE (this);

	if (limit < 0)
		limit = G_MAXINT;

	if (limit != priv->limit) {
		priv->limit = limit;

		update_bins (this);
	}
}

static void
update_bins (TileTable *this)
{
	TileTablePrivate *priv = PRIVATE (this);

	gint n_tiles;
	gint n_rows;
	gint n_cols;

	GList *node;
	gint index;


	g_object_get (G_OBJECT (this), "n-columns", & n_cols, NULL);

	n_tiles = g_list_length (priv->tiles);

	if (n_tiles > priv->limit)
		n_tiles = priv->limit;

	n_rows = (n_tiles + n_cols - 1) / n_cols;

	resize_table (this, n_rows, n_cols);

	for (node = priv->tiles, index = 0; node && index < priv->n_bins; node = node->next)
		insert_into_bin (this, TILE (node->data), index++);

	for (; index < priv->n_bins; ++index)
		empty_bin (this, index);

	gtk_widget_show_all (GTK_WIDGET (this));
}

static void
insert_into_bin (TileTable *this, Tile *tile, gint index)
{
	TileTablePrivate *priv = PRIVATE (this);

	guint n_cols;

	GtkWidget *parent;
	GtkWidget *child;


	if (! GTK_IS_BIN (priv->bins [index])) {
		priv->bins [index] = GTK_BIN (gtk_alignment_new (0.0, 0.5, 1.0, 1.0));

		g_object_get (G_OBJECT (this), "n-columns", & n_cols, NULL);

		gtk_table_attach (
			GTK_TABLE (this), GTK_WIDGET (priv->bins [index]),
			index % n_cols, index % n_cols + 1,
			index / n_cols, index / n_cols + 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	}
	else {
		child = gtk_bin_get_child (priv->bins [index]);

		if (! tile_compare (child, tile))
			return;

		if (child) {
			g_object_ref (G_OBJECT (child));
			gtk_container_remove (GTK_CONTAINER (priv->bins [index]), child);
		}
	}

	if ((parent = gtk_widget_get_parent (GTK_WIDGET (tile)))) {
		g_object_ref (G_OBJECT (tile));
		gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (tile));
		gtk_container_add (GTK_CONTAINER (priv->bins [index]), GTK_WIDGET (tile));
		g_object_unref (G_OBJECT (tile));
	}
	else
		gtk_container_add (GTK_CONTAINER (priv->bins [index]), GTK_WIDGET (tile));
}

static void
empty_bin (TileTable *this, gint index)
{
	TileTablePrivate *priv = PRIVATE (this);

	GtkWidget *child;


	if (priv->bins [index]) {
		if ((child = gtk_bin_get_child (priv->bins [index]))) {
			g_object_ref (G_OBJECT (child));
			gtk_container_remove (GTK_CONTAINER (priv->bins [index]), child);
		}
	}
}

static void
resize_table (TileTable *this, guint n_rows_new, guint n_cols_new)
{
	TileTablePrivate *priv = PRIVATE (this);

	GtkBin **bins_new;
	gint     n_bins_new;

	GtkWidget *child;

	gint i;


	n_bins_new = n_rows_new * n_cols_new;

	if (n_bins_new == priv->n_bins)
		return;

	bins_new = g_new0 (GtkBin *, n_bins_new);

	if (priv->bins) {
		for (i = 0; i < priv->n_bins; ++i) {
			if (i < n_bins_new)
				bins_new [i] = priv->bins [i];
			else {
				if (priv->bins [i]) {
					if ((child = gtk_bin_get_child (priv->bins [i]))) {
						g_object_ref (G_OBJECT (child));
						gtk_container_remove (
							GTK_CONTAINER (priv->bins [i]), child);
					}

					gtk_widget_destroy (GTK_WIDGET (priv->bins [i]));
				}
			}
		}

		g_free (priv->bins);
	}

	priv->bins   = bins_new;
	priv->n_bins = n_bins_new;

	gtk_table_resize (GTK_TABLE (this), n_rows_new, n_cols_new);
}

static void
emit_update_signal (TileTable *this, GList *tiles_prev, GList *tiles_curr, guint32 time)
{
	TileTablePrivate *priv = PRIVATE (this);

	TileTableUpdateEvent *update_event;

	gboolean equal = FALSE;

	GList *node_u;
	GList *node_v;


	if (g_list_length (tiles_prev) == g_list_length (tiles_curr)) {
		node_u = tiles_prev;
		node_v = tiles_curr;
		equal  = TRUE;

		while (equal && node_u && node_v) {
			if (tile_compare (node_u->data, node_v->data))
                		equal = FALSE;

			node_u = node_u->next;
			node_v = node_v->next;
		}
	}

	if (! equal) {
		g_list_free (priv->tiles);
		priv->tiles = tiles_curr;
		update_bins (this);

		update_event = g_new0 (TileTableUpdateEvent, 1);
		update_event->time  = time;
		update_event->tiles = tiles_curr;

		g_signal_emit (this, tile_table_signals [UPDATE_SIGNAL], 0, update_event);
	}
	else {
		g_list_free (tiles_prev);
		g_list_free (tiles_curr);
	}
}

static void
tile_activated_cb (Tile *tile, TileEvent *event, gpointer user_data)
{
	if (event->type == TILE_EVENT_ACTIVATED_DOUBLE_CLICK)
		return;

	tile_trigger_action_with_time (tile, tile->default_action, event->time);
}

static void
tile_drag_data_rcv_cb (
	GtkWidget *dst_widget, GdkDragContext *drag_context, gint x, gint y,
	GtkSelectionData *selection, guint info, guint time, gpointer user_data)
{
	TileTable *this = TILE_TABLE (user_data);
	TileTablePrivate *priv = PRIVATE (this);

	GList *tiles_prev;
	GList *tiles_curr;

	GtkWidget *src_widget;
	gchar **uris;

	GList *src_node;
	GList *dst_node;
	GList *last_node;

	gint src_index;
	gint dst_index;

	gboolean new_uri;

	gpointer tmp;
	gint i;


	src_widget = gtk_drag_get_source_widget (drag_context);

	new_uri = (! src_widget || ! IS_TILE (src_widget) || ! tile_compare (src_widget, dst_widget));

	if (! new_uri)
		src_node = g_list_find_custom (priv->tiles, src_widget, tile_compare);
	else
		src_node = NULL;

	if (new_uri || ! src_node) {
		uris = gtk_selection_data_get_uris (selection);

		for (i = 0; uris && uris [i]; ++i)
			tile_table_uri_added (this, uris [i]);

		goto exit;
	}

	tiles_prev = g_list_copy (priv->tiles);
	tiles_curr = g_list_copy (priv->tiles);

	src_node = g_list_find_custom (tiles_curr, src_widget, tile_compare);
	dst_node = g_list_find_custom (tiles_curr, dst_widget, tile_compare);

	if (priv->priority == TILE_TABLE_REORDERING_SWAP) {
		tmp            = src_node->data;
		src_node->data = dst_node->data;
		dst_node->data = tmp;
	}
	else {
		src_index = g_list_position (tiles_curr, src_node);
		dst_index = g_list_position (tiles_curr, dst_node);

		tiles_curr = g_list_remove_link (tiles_curr, src_node);

		if (priv->priority == TILE_TABLE_REORDERING_PUSH) {
			if (src_index < dst_index) {
				last_node = g_list_last (tiles_curr);

				tiles_curr = g_list_remove_link (tiles_curr, last_node);
				tiles_curr = g_list_prepend (tiles_curr, last_node->data);
			}
		}
		else if (src_index < dst_index)
			dst_node = dst_node->next;
		else
			/* do nothing */ ;

		tiles_curr = g_list_insert_before (tiles_curr, dst_node, src_node->data);
	}

	emit_update_signal (this, tiles_prev, tiles_curr, (guint32) time);

exit:

	gtk_drag_finish (drag_context, TRUE, FALSE, (guint32) time);
}
