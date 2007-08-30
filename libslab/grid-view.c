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

#include "grid-view.h"

typedef struct {
	gchar    *title;
	GtkTable *table;
} GridNode;

typedef struct {
	GtkTreeModel    *model;

	GridNode       **nodes;
	GtkRequisition   max_cell;

#if 0
	gint            limit;
	gboolean        reorderable;
	gboolean        modifiable;

	BookmarkAgent   *agent;

	GList           *tiles;

	GtkBin         **bins;
	gint             n_bins;


	gint             reord_bin_orig;
	gint             reord_bin_curr;

	ItemToTileFunc   create_tile_func;
	gpointer         tile_func_data;
	URIToItemFunc    create_item_func;
	gpointer         item_func_data;
#endif
} GridViewPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GRID_VIEW_TYPE, GridViewPrivate))
#define DEFAULT_N_COLS 3

/* GType funcs */

static void class_intern_init (GridViewClass *);
static void class_init        (GridViewClass *);
static void init              (GridView *);

/* GObject funcs */

static void finalize (GObject *);

#if 0
static void get_property  (GObject *, guint, GValue *, GParamSpec *);
static void set_property  (GObject *, guint, const GValue *, GParamSpec *);
#endif

/* GtkWidget funcs */

static void size_request  (GtkWidget *, GtkRequisition *);
static void size_allocate (GtkWidget *, GtkAllocation  *);

#if 0
static void size_allocate (GtkWidget *, GtkAllocation *);

static gboolean drag_motion   (GtkWidget *, GdkDragContext *, gint, gint, guint);
static void     drag_leave    (GtkWidget *, GdkDragContext *, guint);
static void     drag_data_rcv (GtkWidget *, GdkDragContext *, gint, gint,
#endif

static void load_model (GridView *);
static void layout     (GridView *);

#if 0
static void   update_bins                  (GridView *, GList *);
static void   insert_into_bin              (GridView *, Tile *, gint);
static void   empty_bin                    (GridView *, gint);
static void   resize_table                 (GridView *, guint, guint);
static GList *reorder_tiles                (GridView *, gint, gint);
static void   save_reorder                 (GridView *, GList *);
static void   connect_signal_if_not_exists (Tile *, const gchar *, GCallback, gpointer);

static void tile_drag_begin_cb (GtkWidget *, GdkDragContext *, gpointer);
static void tile_drag_end_cb   (GtkWidget *, GdkDragContext *, gpointer);
static void agent_notify_cb    (GObject *, GParamSpec *, gpointer);
#endif

static GtkViewportClass *parent_class;

enum {
	PROP_0,
	PROP_LIMIT,
	PROP_REORDERABLE,
	PROP_MODIFIABLE
};

GType
grid_view_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			GTK_TYPE_VIEWPORT, "GridView",
			sizeof (GridViewClass), (GClassInitFunc) class_intern_init,
			sizeof (GridView), (GInstanceInitFunc) init, 0);

	return type_id;
}

GtkWidget *
grid_view_new_with_model (GtkTreeModel *model)
{
	GridView        *this;
	GridViewPrivate *priv;


	g_return_val_if_fail (model, NULL);
	g_return_val_if_fail (gtk_tree_model_get_column_type (model, 0) == G_TYPE_STRING,   NULL);
	g_return_val_if_fail (gtk_tree_model_get_column_type (model, 1) == GTK_TYPE_WIDGET, NULL);

	this = GRID_VIEW (g_object_new (
		GRID_VIEW_TYPE,
		"hadjustment", NULL,
		"vadjustment", NULL,
		NULL));
	priv = PRIVATE (this);

	priv->model = model;

	load_model (this);
	layout     (this);

	return GTK_WIDGET (this);
}

static void
class_intern_init (GridViewClass *this_class)
{
	parent_class = g_type_class_peek_parent (this_class);
	class_init (this_class);
}

static void
class_init (GridViewClass *this_class)
{
	GObjectClass   *g_obj_class  = G_OBJECT_CLASS   (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);

	g_obj_class->finalize = finalize;

	widget_class->size_request  = size_request;
	widget_class->size_allocate = size_allocate;

#if 0

	GParamSpec *limit_pspec;
	GParamSpec *reorder_pspec;
	GParamSpec *modify_pspec;


	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;

	widget_class->drag_motion        = drag_motion;
	widget_class->drag_leave         = drag_leave;
	widget_class->drag_data_received = drag_data_rcv;

	limit_pspec = g_param_spec_int (
		GRID_VIEW_LIMIT_PROP, GRID_VIEW_LIMIT_PROP,
		"the maximum number of items this table can hold, -1 if unlimited",
		-1, G_MAXINT, -1,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	reorder_pspec = g_param_spec_boolean (
		GRID_VIEW_REORDERABLE_PROP, GRID_VIEW_REORDERABLE_PROP,
		"TRUE if reordering allowed for this table",
		TRUE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	modify_pspec = g_param_spec_boolean (
		GRID_VIEW_MODIFIABLE_PROP, GRID_VIEW_MODIFIABLE_PROP,
		"TRUE if tiles are allowed to be dragged into this table",
		TRUE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_LIMIT,       limit_pspec);
	g_object_class_install_property (g_obj_class, PROP_REORDERABLE, reorder_pspec);
	g_object_class_install_property (g_obj_class, PROP_MODIFIABLE,  modify_pspec);
#endif

	g_type_class_add_private (this_class, sizeof (GridViewPrivate));
}

static void
init (GridView *this)
{
	GridViewPrivate *priv = PRIVATE (this);

	priv->model           = NULL;

	priv->nodes           = NULL;
	priv->max_cell.width  = 0;
	priv->max_cell.height = 0;

#if 0
	priv->widget_col      = 0;

	priv->limit           = -1;
	priv->reorderable     = FALSE;
	priv->modifiable      = FALSE;

	priv->agent               = NULL;

	priv->tiles               = NULL;

	priv->bins                = NULL;
	priv->n_bins              = 0;


	priv->reord_bin_orig      = -1;
	priv->reord_bin_curr      = -1;

	priv->create_tile_func    = NULL;
	priv->tile_func_data      = NULL;
	priv->create_item_func    = NULL;
	priv->item_func_data      = NULL;
#endif
}

#if 0
void
grid_view_reload (GridView *this)
{
	GridViewPrivate *priv = PRIVATE (this);

	BookmarkItem **items = NULL;
	GList         *tiles = NULL;
	Tile          *tile;
	gint           n_tiles;

	GtkSizeGroup *icon_size_group;

	GList *node;
	gint   i;


	g_object_get (G_OBJECT (priv->agent), BOOKMARK_AGENT_ITEMS_PROP, & items, NULL);

	for (i = 0, n_tiles = 0; (priv->limit < 0 || n_tiles < priv->limit) && items && items [i]; ++i) {
		tile = priv->create_tile_func (items [i], priv->tile_func_data);

		if (tile) {
			g_object_set_data (G_OBJECT (tile), "tile-table-uri", items [i]->uri);
			tiles = g_list_append (tiles, tile);
			++n_tiles;
		}
	}

	for (node = priv->tiles; node; node = node->next)
		g_object_unref (G_OBJECT (node->data));

	g_list_free (priv->tiles);

	priv->tiles = NULL;

	icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	for (node = tiles; node; node = node->next) {
		tile = TILE (node->data);

		g_object_set_data (G_OBJECT (node->data), "tile-table", this);

		connect_signal_if_not_exists (
			TILE (tile), "drag-begin", G_CALLBACK (tile_drag_begin_cb), this);
		connect_signal_if_not_exists (
			TILE (tile), "drag-end", G_CALLBACK (tile_drag_end_cb), this);

		priv->tiles = g_list_append (priv->tiles, tile);

		if (IS_TILE_BUTTON_VIEW (tile_get_widget (tile)))
			gtk_size_group_add_widget (
				icon_size_group,
				TILE_BUTTON_VIEW (tile_get_widget (tile))->icon);
	}

	g_list_free (tiles);

	update_bins (this, priv->tiles);

	g_object_notify (G_OBJECT (this), GRID_VIEW_TILES_PROP);
}

void
grid_view_add_uri (GridView *this, const gchar *uri)
{
	GridViewPrivate *priv = PRIVATE (this);

	BookmarkItem *item;


	item = priv->create_item_func (uri, priv->item_func_data);
	bookmark_agent_add_item (priv->agent, item);
	bookmark_item_free (item);
}
#endif

#if 0
static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GridViewPrivate *priv = PRIVATE (g_obj);

	switch (prop_id) {
		case PROP_LIMIT:
			g_value_set_int (value, priv->limit);
			break;

		case PROP_REORDERABLE:
			g_value_set_boolean (value, priv->reorderable);
			break;

		case PROP_MODIFIABLE:
			g_value_set_boolean (value, priv->modifiable);
			break;

		default:
			break;
	}
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GridView        *this = GRID_VIEW (g_obj);
	GridViewPrivate *priv = PRIVATE    (this);

	gint limit;


	switch (prop_id) {
		case PROP_LIMIT:
			limit = g_value_get_int (value);

			if (limit != priv->limit) {
				priv->limit = limit;
				reload (this);
			}

			break;

		case PROP_REORDERABLE:
			priv->reorderable = g_value_get_boolean (value);
			break;

		case PROP_MODIFIABLE:
			priv->modifiable = g_value_get_boolean (value);

			if (priv->modifiable) {
				gtk_drag_dest_set (
					GTK_WIDGET (this),
					GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
					NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);
				gtk_drag_dest_add_uri_targets (GTK_WIDGET (this));
			}
			else
				gtk_drag_dest_unset (GTK_WIDGET (this));

			break;

		default:
			break;
	}
}
#endif

static void
finalize (GObject *g_obj)
{
	GridViewPrivate *priv = PRIVATE (g_obj);

	gint i;


	g_object_unref (priv->model);

	for (i = 0; priv->nodes && priv->nodes [i]; ++i) {
		g_free (priv->nodes [i]->title);
		g_free (priv->nodes [i]);
	}
	g_free (priv->nodes);

	G_OBJECT_CLASS (parent_class)->finalize (g_obj);
}

static void
size_request (GtkWidget *widget, GtkRequisition *req)
{
	GridViewPrivate *priv = PRIVATE (widget);

	GtkRequisition req_i;

	GList *children;
	GList *node;

	gint i;


	for (i = 0; priv->nodes && priv->nodes [i]; ++i) {
		children = gtk_container_get_children (GTK_CONTAINER (priv->nodes [i]->table));

		for (node = children; node; node = node->next) {
			gtk_widget_size_request (GTK_WIDGET (node->data), & req_i);

			if (req_i.width > priv->max_cell.width)
				priv->max_cell.width = req_i.width;
			if (req_i.height > priv->max_cell.height)
				priv->max_cell.height = req_i.height;
		}
	}

	GTK_WIDGET_CLASS (parent_class)->size_request (widget, req);

	g_printf ("priv->max_cell = %dx%d\n", priv->max_cell.width, priv->max_cell.height);
	g_printf ("this->req      = %dx%d\n", req->width, req->height);
}

static void
size_allocate (GtkWidget *widget, GtkAllocation *alloc)
{
	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, alloc);

	g_printf ("this->alloc = %dx%d\n", alloc->width, alloc->height);
}

#if 0
static void
size_allocate (GtkWidget *widget, GtkAllocation *alloc)
{
	GtkContainer    *this = GTK_CONTAINER (widget);
	GridViewPrivate *priv = PRIVATE (widget);

	gint n_rows;
	gint n_cols;
	gint border;

	GList         *children;
	GtkWidget     *child;
	GtkAllocation  cell_alloc;

	GList *node;
	gint   i;


	children = gtk_container_get_children (this);

	if (! children)
		return;

	border = gtk_container_get_border_width (this);

	n_cols = (alloc->width - 2 * border + priv->cell_spacing) / (priv->max_cell.width + priv->cell_spacing);
	n_rows = (g_list_length (children) + n_cols - 1) / n_cols;

	cell_alloc.width  = (alloc->width  - 2 * border - (n_cols - 1) * priv->cell_spacing) / n_cols;
	cell_alloc.height = (alloc->height - 2 * border - (n_rows - 1) * priv->cell_spacing) / n_rows;

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (widget->window, alloc->x, alloc->y, alloc->width, alloc->height);

      		gdk_window_resize (
			GTK_LAYOUT (this)->bin_window,
			 MAX (GTK_LAYOUT (this)->width,  alloc->width),
			 MAX (GTK_LAYOUT (this)->height, alloc->height));
	}

	for (i = 0, node = children; node; ++i, node = node->next) {
		cell_alloc.x = border + (i % n_cols) * (cell_alloc.width  + priv->cell_spacing);
		cell_alloc.y = border + (i / n_cols) * (cell_alloc.height + priv->cell_spacing);

		child = GTK_WIDGET (node->data);

		gtk_layout_move (GTK_LAYOUT (this), child, cell_alloc.x, cell_alloc.y);
		gtk_widget_size_allocate (child, & cell_alloc);
	}

/*	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, alloc); */
}
#endif

static void
load_model (GridView *this)
{
	GridViewPrivate *priv = PRIVATE (this);

	GValue *value;
	gint    n;

	GtkTreeIter iter_p;
	GtkTreeIter iter_c;
	gint        i;


	if (gtk_tree_model_get_iter_first (priv->model, & iter_p)) {
		n = gtk_tree_model_iter_n_children (priv->model, NULL);

		priv->nodes = g_new0 (GridNode *, n + 1);
		value       = g_new0 (GValue, 1);

		i = 0;
		do {
			priv->nodes [i] = g_new0 (GridNode, 1);

			gtk_tree_model_get_value (priv->model, & iter_p, 0, value);
			priv->nodes [i]->title = g_value_dup_string (value);
			g_value_unset (value);

			if (gtk_tree_model_iter_children (priv->model, & iter_c, & iter_p)) {
				priv->nodes [i]->table = GTK_TABLE (gtk_widget_new (
					GTK_TYPE_TABLE, "homogeneous", TRUE, NULL));

				do {
					gtk_tree_model_get_value (priv->model, & iter_c, 1, value);
					gtk_container_add (
						GTK_CONTAINER (priv->nodes [i]->table),
						GTK_WIDGET (g_value_dup_object (value)));
					g_value_unset (value);

				} while (gtk_tree_model_iter_next (priv->model, & iter_c));
			}

			++i;

		} while (gtk_tree_model_iter_next (priv->model, & iter_p) && i < n);
	}

	g_free (value);
}

static void
layout (GridView *this)
{
	GridViewPrivate *priv = PRIVATE (this);

	GtkBox *vbox;

	GList *children;
	GList *node;
	gint   n;
	gint   n_cols;
	gint   n_rows;

	gint i;
	gint j;


	vbox = GTK_BOX (gtk_vbox_new (FALSE, 0));
	gtk_container_add (GTK_CONTAINER (this), GTK_WIDGET (vbox));

	for (i = 0; priv->nodes && priv->nodes [i]; ++i) {
		gtk_box_pack_start (
			vbox,
			gtk_widget_new (
				GTK_TYPE_LABEL,
				"label",  priv->nodes [i]->title,
				"xalign", 0.0,
				NULL),
			FALSE, FALSE, 0);
		gtk_box_pack_start (vbox, GTK_WIDGET (priv->nodes [i]->table), FALSE, FALSE, 0);

		children = gtk_container_get_children (GTK_CONTAINER (priv->nodes [i]->table));
		n        = g_list_length (children);
		n_cols   = DEFAULT_N_COLS;
		n_rows   = (n + n_cols - 1) / n_cols;

		gtk_table_resize (priv->nodes [i]->table, n_rows, n_cols);

		for (j = 0, node = children; j < n && node; ++j, node = node->next)
			gtk_container_child_set (
				GTK_CONTAINER (priv->nodes [i]->table), GTK_WIDGET (node->data),
				"left-attach", j % n_cols, "right-attach",  (j % n_cols) + 1,
				"top-attach",  j / n_cols, "bottom-attach", (j / n_cols) + 1,
				NULL);
	}
}

#if 0
static gboolean
drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time)
{
	GridView        *this = GRID_VIEW (widget);
	GridViewPrivate *priv = PRIVATE    (this);

	GtkWidget *src_tile;
	GridView *src_table;

	GList *tiles_reord;

	gint n_rows, n_cols;
	gint bin_row, bin_col;

	gint bin_index;


	if (! priv->reorderable)
		return FALSE;

	src_tile = gtk_drag_get_source_widget (context);

	if (! (src_tile && IS_TILE (src_tile))) {
		gtk_drag_highlight (widget);

		return FALSE;
	}

	src_table = GRID_VIEW (g_object_get_data (G_OBJECT (src_tile), "tile-table"));

	if (src_table != GRID_VIEW (widget)) {
		gtk_drag_highlight (widget);

		return FALSE;
	}

	g_object_get (G_OBJECT (widget), "n-rows", & n_rows, "n-columns", & n_cols, NULL);

	bin_row = y * n_rows / widget->allocation.height;
	bin_col = x * n_cols / widget->allocation.width;

	bin_index = bin_row * n_cols + bin_col;

	if (priv->reord_bin_curr != bin_index) {
		priv->reord_bin_curr = bin_index;

		tiles_reord = reorder_tiles (this, priv->reord_bin_orig, priv->reord_bin_curr);

		if (tiles_reord)
			update_bins (this, tiles_reord);
		else
			update_bins (this, priv->tiles);

		g_list_free (tiles_reord);
	}

	return FALSE;
}

static void
drag_leave (GtkWidget *widget, GdkDragContext *context, guint time)
{
	GridView        *this = GRID_VIEW (widget);
	GridViewPrivate *priv = PRIVATE    (this);


	gtk_drag_unhighlight (widget);

	update_bins (GRID_VIEW (widget), priv->tiles);
}

static void
drag_data_rcv (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
               GtkSelectionData *selection, guint info, guint time)
{
	GridView        *this = GRID_VIEW (widget);
	GridViewPrivate *priv = PRIVATE    (this);

	GtkWidget *src_tile;
	GridView *src_table;

	gboolean reordering;

	GList *tiles_new;

	gchar **uris;
	gint    i;


	src_tile = gtk_drag_get_source_widget (context);

	if (src_tile && IS_TILE (src_tile)) {
		src_table = GRID_VIEW (g_object_get_data (G_OBJECT (src_tile), "tile-table"));

		reordering = (src_table == GRID_VIEW (widget));
	}
	else
		reordering = FALSE;

	if (reordering) {
		if (priv->reorderable) {
			tiles_new = reorder_tiles (this, priv->reord_bin_orig, priv->reord_bin_curr);
			save_reorder (this, tiles_new);
			g_list_free (tiles_new);
		}
	}
	else {
		uris = gtk_selection_data_get_uris (selection);

		for (i = 0; uris && uris [i]; ++i)
			grid_view_add_uri (this, uris [i]);

		g_strfreev (uris);
	}

	gtk_drag_finish (context, TRUE, FALSE, (guint32) time);
}

static void
connect_signal_if_not_exists (Tile *tile, const gchar *signal, GCallback cb, gpointer user_data)
{
	gulong handler_id = g_signal_handler_find (tile, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, cb, NULL);

	if (! handler_id)
		g_signal_connect (G_OBJECT (tile), signal, cb, user_data);
}

static GList *
reorder_tiles (GridView *this, gint src_index, gint dst_index)
{
	GridViewPrivate *priv = PRIVATE (this);

	GList *tiles_reord;
	gint   n_tiles;

	GList *src_node;
	GList *dst_node;


	n_tiles = g_list_length (priv->tiles);

	if (dst_index >= n_tiles)
		dst_index = n_tiles - 1;

	if (src_index == dst_index)
		return NULL;

	tiles_reord = g_list_copy (priv->tiles);

	src_node = g_list_nth (tiles_reord, src_index);
	dst_node = g_list_nth (tiles_reord, dst_index);

	tiles_reord = g_list_remove_link (tiles_reord, src_node);

	if (src_index < dst_index)
		dst_node = dst_node->next;

	tiles_reord = g_list_insert_before (tiles_reord, dst_node, src_node->data);

	return tiles_reord;
}

static void
update_bins (GridView *this, GList *tiles)
{
	GridViewPrivate *priv = PRIVATE (this);

	gint n_tiles;
	gint n_rows;
	gint n_cols;

	GList *node;
	gint   i;


	if (! tiles)
		return;

	g_object_get (G_OBJECT (this), "n-columns", & n_cols, NULL);

	n_tiles = g_list_length (tiles);

	n_rows = (n_tiles + n_cols - 1) / n_cols;

	resize_table (this, n_rows, n_cols);

	for (node = tiles, i = 0; node && i < priv->n_bins; node = node->next)
		insert_into_bin (this, TILE (node->data), i++);

	for (; i < priv->n_bins; ++i)
		empty_bin (this, i);

	gtk_widget_show_all (GTK_WIDGET (this));
}

static void
insert_into_bin (GridView *this, Tile *tile, gint index)
{
	GridViewPrivate *priv = PRIVATE (this);

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

		if (tile_equals (tile, child))
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

	g_object_set_data (G_OBJECT (tile), "tile-table-bin", GINT_TO_POINTER (index));
}

static void
empty_bin (GridView *this, gint index)
{
	GridViewPrivate *priv = PRIVATE (this);

	GtkWidget *child;


	if (priv->bins [index]) {
		if ((child = gtk_bin_get_child (priv->bins [index]))) {
			g_object_ref (G_OBJECT (child));
			gtk_container_remove (GTK_CONTAINER (priv->bins [index]), child);
		}
	}
}

static void
resize_table (GridView *this, guint n_rows_new, guint n_cols_new)
{
	GridViewPrivate *priv = PRIVATE (this);

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
save_reorder (GridView *this, GList *tiles_new)
{
	GridViewPrivate *priv = PRIVATE (this);

	gboolean equal = FALSE;

	gchar **uris;
	gint    n_items;

	GList *node_u;
	GList *node_v;
	gint   i;


	if (! tiles_new || priv->tiles == tiles_new)
		return;

	n_items = g_list_length (priv->tiles);

	if (n_items == g_list_length (tiles_new)) {
		node_u = priv->tiles;
		node_v = tiles_new;
		equal  = TRUE;

		while (equal && node_u && node_v) {
			if (! tile_equals (TILE (node_u->data), node_v->data))
                		equal = FALSE;

			node_u = node_u->next;
			node_v = node_v->next;
		}
	}

	if (! equal) {
		g_list_free (priv->tiles);
		priv->tiles = g_list_copy (tiles_new);
		update_bins (this, priv->tiles);

		uris = g_new0 (gchar *, n_items + 1);

		for (node_u = priv->tiles, i = 0; node_u && i < n_items; node_u = node_u->next, ++i)
			uris [i] = g_strdup (g_object_get_data (
				G_OBJECT (node_u->data), "tile-table-uri"));

		bookmark_agent_reorder_items (priv->agent, (const gchar **) uris);
		g_object_notify (G_OBJECT (this), GRID_VIEW_TILES_PROP);

		g_strfreev (uris);
	}
}

static void
tile_drag_begin_cb (GtkWidget *widget, GdkDragContext *context, gpointer user_data)
{
	GridViewPrivate *priv = PRIVATE (user_data);

	priv->reord_bin_orig = GPOINTER_TO_INT (
		g_object_get_data (G_OBJECT (widget), "tile-table-bin"));
}

static void
tile_drag_end_cb (GtkWidget *widget, GdkDragContext *context, gpointer user_data)
{
	GridViewPrivate *priv = PRIVATE (user_data);

	priv->reord_bin_orig = -1;
	priv->reord_bin_curr = -1;
}

static void
agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	grid_view_reload (GRID_VIEW (user_data));
}
#endif
