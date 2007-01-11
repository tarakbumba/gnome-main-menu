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

#include "file-area.h"

#define NUM_COLUMNS 2

G_DEFINE_TYPE (FileArea, file_area, GTK_TYPE_VBOX)

enum {
	PROP_O,
	PROP_USER_HDR_WIDGET,
	PROP_RCNT_HDR_WIDGET,
	PROP_MAX_TOTAL_ITEMS,
	PROP_MIN_RECENT_ITEMS
};

typedef struct {
	GtkWidget *user_hdr;
	GtkWidget *rcnt_hdr;

	GtkContainer *user_hdr_ctnr;
	GtkContainer *rcnt_hdr_ctnr;

	gint max_total_items;
	gint min_recent_items;
} FileAreaPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FILE_AREA_TYPE, FileAreaPrivate))

static void file_area_get_property (GObject *, guint, GValue *, GParamSpec *);
static void file_area_set_property (GObject *, guint, const GValue *, GParamSpec *);
static void file_area_finalize     (GObject *);

static void update_limits (FileArea *);

static void user_table_n_rows_notify_cb (GObject *, GParamSpec *, gpointer);

GtkWidget *
file_area_new ()
{
	FileArea *this;
	FileAreaPrivate *priv;


	this = g_object_new (FILE_AREA_TYPE, "homogeneous", FALSE, "spacing", 18, NULL);

	this->user_spec_table = TILE_TABLE (
		tile_table_new (NUM_COLUMNS, TILE_TABLE_REORDERING_PUSH_PULL));
	this->recent_table = TILE_TABLE (
		tile_table_new (NUM_COLUMNS, TILE_TABLE_REORDERING_NONE));

	priv = PRIVATE (this);

	priv->user_hdr_ctnr = GTK_CONTAINER (gtk_alignment_new (0.0, 0.5, 1.0, 1.0));
	priv->rcnt_hdr_ctnr = GTK_CONTAINER (gtk_alignment_new (0.0, 0.5, 1.0, 1.0));

	gtk_box_pack_start (GTK_BOX (this), GTK_WIDGET (priv->user_hdr_ctnr),   TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (this), GTK_WIDGET (this->user_spec_table), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (this), GTK_WIDGET (priv->rcnt_hdr_ctnr),   TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (this), GTK_WIDGET (this->recent_table),    TRUE, TRUE, 0);

	g_signal_connect (
		G_OBJECT (this->user_spec_table), "notify::n-rows",
		G_CALLBACK (user_table_n_rows_notify_cb), this);

	return GTK_WIDGET (this);
}

static void
file_area_class_init (FileAreaClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	GParamSpec *user_hdr_pspec;
	GParamSpec *rcnt_hdr_pspec;
	GParamSpec *max_pspec;
	GParamSpec *min_pspec;


	g_obj_class->get_property = file_area_get_property;
	g_obj_class->set_property = file_area_set_property;
	g_obj_class->finalize     = file_area_finalize;

	user_hdr_pspec = g_param_spec_object (
		FILE_AREA_USER_HDR_PROP, FILE_AREA_USER_HDR_PROP,
		"the GtkWidget which serves as the header for the user-specified items table",
		GTK_TYPE_WIDGET, G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	rcnt_hdr_pspec = g_param_spec_object (
		FILE_AREA_USER_HDR_PROP, FILE_AREA_USER_HDR_PROP,
		"the GtkWidget which serves as the header for the user-specified items table",
		GTK_TYPE_WIDGET, G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	max_pspec = g_param_spec_int (
		FILE_AREA_MAX_TOTAL_ITEMS_PROP, FILE_AREA_MAX_TOTAL_ITEMS_PROP,
		"the maximum total number of items allowable in this area, -1 if unlimited",
		-1, G_MAXINT, 8, G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	min_pspec = g_param_spec_int (
		FILE_AREA_MIN_RECENT_ITEMS_PROP, FILE_AREA_MIN_RECENT_ITEMS_PROP,
		"the minimum number of recent items allowable in this area, -1 if unlimited",
		-1, G_MAXINT, 2, G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_USER_HDR_WIDGET,  user_hdr_pspec);
	g_object_class_install_property (g_obj_class, PROP_RCNT_HDR_WIDGET,  rcnt_hdr_pspec);
	g_object_class_install_property (g_obj_class, PROP_MAX_TOTAL_ITEMS,  max_pspec);
	g_object_class_install_property (g_obj_class, PROP_MIN_RECENT_ITEMS, min_pspec);

	g_type_class_add_private (this_class, sizeof (FileAreaPrivate));
}

static void
file_area_init (FileArea *this)
{
	FileAreaPrivate *priv = PRIVATE (this);

	this->user_spec_table = NULL;
	this->recent_table    = NULL;

	priv->user_hdr = NULL;
	priv->rcnt_hdr = NULL;

	priv->max_total_items  = 0;
	priv->min_recent_items = 0;;
}

static void
file_area_get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	FileAreaPrivate *priv = PRIVATE (g_obj);

	switch (prop_id) {
		case PROP_USER_HDR_WIDGET:
			g_value_set_object (value, priv->user_hdr);
			break;

		case PROP_RCNT_HDR_WIDGET:
			g_value_set_object (value, priv->rcnt_hdr);
			break;

		case PROP_MAX_TOTAL_ITEMS:
			g_value_set_int (value, priv->max_total_items);
			break;

		case PROP_MIN_RECENT_ITEMS:
			g_value_set_int (value, priv->min_recent_items);
			break;

		default:
			break;
	}
}

static void
file_area_set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	FileAreaPrivate *priv = PRIVATE (g_obj);

	switch (prop_id) {
		case PROP_USER_HDR_WIDGET:
			priv->user_hdr = GTK_WIDGET (g_value_dup_object (value));
			break;

		case PROP_RCNT_HDR_WIDGET:
			priv->rcnt_hdr = GTK_WIDGET (g_value_dup_object (value));
			break;

		case PROP_MAX_TOTAL_ITEMS:
			priv->max_total_items = g_value_get_int (value);
			update_limits (FILE_AREA (g_obj));
			break;

		case PROP_MIN_RECENT_ITEMS:
			priv->min_recent_items = g_value_get_int (value);
			update_limits (FILE_AREA (g_obj));
			break;

		default:
			break;
	}
}

static void
file_area_finalize (GObject *g_obj)
{
	FileArea *this = FILE_AREA (g_obj);

	g_object_unref (this->user_spec_table);
	g_object_unref (this->recent_table);

	G_OBJECT_CLASS (file_area_parent_class)->finalize (g_obj);
}

static void
update_limits (FileArea *this)
{
	FileAreaPrivate *priv = PRIVATE (this);

	gint n_rows;
	gint rcnt_limit_new;


	g_object_get (G_OBJECT (this->user_spec_table), "n-rows", & n_rows, NULL);

	rcnt_limit_new = priv->max_total_items - NUM_COLUMNS * n_rows;

	if (rcnt_limit_new < priv->min_recent_items)
		rcnt_limit_new = priv->min_recent_items;

	g_object_set (G_OBJECT (this->user_spec_table), TILE_TABLE_LIMIT_PROP, -1, NULL);
	g_object_set (G_OBJECT (this->recent_table), TILE_TABLE_LIMIT_PROP, rcnt_limit_new, NULL);
}

static void
user_table_n_rows_notify_cb (GObject *obj, GParamSpec *spec, gpointer user_data)
{
	update_limits (FILE_AREA (user_data));
}
