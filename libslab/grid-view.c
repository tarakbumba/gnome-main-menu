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
	GtkTreeModel *model;
	GHashTable   *nodes;
	GtkBox       *node_box;
} GridViewPrivate;

typedef struct {
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *table;
} GridNode;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GRID_VIEW_TYPE, GridViewPrivate))
#define DEFAULT_N_COLS 3

static void class_intern_init (GridViewClass *);
static void class_init        (GridViewClass *);
static void init              (GridView *);

static void finalize (GObject *);

static void row_changed_cb (GtkTreeModel *, GtkTreePath *, GtkTreeIter *, gpointer);

static GtkViewportClass *parent_class;

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

	priv->model    = model;
	priv->nodes    = g_hash_table_new (g_str_hash, g_str_equal);
	priv->node_box = GTK_BOX (gtk_vbox_new (FALSE, 0));

	gtk_container_add (GTK_CONTAINER (this), GTK_WIDGET (priv->node_box));

	g_signal_connect (priv->model, "row-changed", G_CALLBACK (row_changed_cb), this);

	return GTK_WIDGET (this);
}

void
grid_view_scroll_to_node (GridView *this, const gchar *name)
{
	GridViewPrivate *priv = PRIVATE (this);

	GridNode *node;


	node = (GridNode *) g_hash_table_lookup (priv->nodes, name);
	g_return_if_fail (node);

	gtk_widget_set_state (node->box, GTK_STATE_ACTIVE);
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
	G_OBJECT_CLASS (this_class)->finalize = finalize;

	g_type_class_add_private (this_class, sizeof (GridViewPrivate));
}

static void
init (GridView *this)
{
	GridViewPrivate *priv = PRIVATE (this);

	priv->model = NULL;
	priv->nodes = NULL;
}

static void
finalize (GObject *g_obj)
{
	GridViewPrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->model);
	g_hash_table_destroy (priv->nodes);

	G_OBJECT_CLASS (parent_class)->finalize (g_obj);
}

static void
row_changed_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GridViewPrivate *priv = PRIVATE (data);

	GtkWidget *widget = NULL;
	gchar     *cat_name;
	GridNode  *node;
	GList     *children;
	gint       n_rows;
	gint       n_cols;
	gint       n_children;

	GtkTreeIter iter_p;


	if (gtk_tree_model_iter_parent (model, & iter_p, iter)) {
		gtk_tree_model_get (model, iter, 1, & widget, -1);
		gtk_tree_model_get (model, & iter_p, 0, & cat_name, -1);
	}
	else
		gtk_tree_model_get (model, iter, 0, & cat_name, -1);

	node = (GridNode *) g_hash_table_lookup (priv->nodes, cat_name);

	if (! node) {
		node = g_new0 (GridNode, 1);

		node->box   = gtk_vbox_new (FALSE, 0);
		node->label = gtk_widget_new (GTK_TYPE_LABEL, "label", cat_name, "xalign", 0.0, NULL);
		node->table = gtk_table_new (1, DEFAULT_N_COLS, TRUE);

		g_hash_table_insert (priv->nodes, cat_name, node);

		gtk_box_pack_start (GTK_BOX (node->box), node->label, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (node->box), node->table, FALSE, FALSE, 0);
		gtk_box_pack_start (priv->node_box, node->box, FALSE, FALSE, 0);
	}

	if (widget) {
		children   = gtk_container_get_children (GTK_CONTAINER (node->table));
		n_children = g_list_length (children);
		g_list_free (children);

		g_object_get (G_OBJECT (node->table), "n-rows", & n_rows, "n-columns", & n_cols, NULL);

		if (n_rows * n_cols < n_children)
			gtk_table_resize (GTK_TABLE (node->table), n_rows + 1, n_cols);

		gtk_table_attach_defaults (
			GTK_TABLE (node->table), widget,
			n_children % n_cols, (n_children % n_cols) + 1,
			n_children / n_cols, (n_children / n_cols) + 1);
	}
}
