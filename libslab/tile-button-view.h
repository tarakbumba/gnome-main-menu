#ifndef __TILE_BUTTON_VIEW_H__
#define __TILE_BUTTON_VIEW_H__

#include <gtk/gtk.h>

#include "tile-attribute.h"

G_BEGIN_DECLS

#define TILE_BUTTON_VIEW_TYPE         (tile_button_view_get_type ())
#define TILE_BUTTON_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_BUTTON_VIEW_TYPE, TileButtonView))
#define TILE_BUTTON_VIEW_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TILE_BUTTON_VIEW_TYPE, TileButtonViewClass))
#define IS_TILE_BUTTON_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_BUTTON_VIEW_TYPE))
#define IS_TILE_BUTTON_VIEW_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TILE_BUTTON_VIEW_TYPE))
#define TILE_BUTTON_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TILE_BUTTON_VIEW_TYPE, TileButtonViewClass))

#define TILE_BUTTON_VIEW_DEBOUNCE_PROP "tile-button-view-debounce"

#define TILE_BUTTON_VIEW_URI_ATTR            "tile-button-uri"
#define TILE_BUTTON_VIEW_ICON_ID_ATTR        "tile-button-icon-id"
#define TILE_BUTTON_VIEW_HDR_TXT_ATTR_PREFIX "tile-button-header-text-"

typedef struct {
	GtkButton gtk_button;

	GtkWidget  *icon;
	GtkWidget **headers;
	gint        n_hdrs;
} TileButtonView;

typedef struct {
	GtkButtonClass gtk_button_class;
} TileButtonViewClass;

GType tile_button_view_get_type (void);

TileButtonView *tile_button_view_new                  (gint n_hdrs);
TileAttribute  *tile_button_view_get_uri_attr         (TileButtonView *this);
TileAttribute  *tile_button_view_get_icon_id_attr     (TileButtonView *this);
TileAttribute  *tile_button_view_get_header_text_attr (TileButtonView *this, gint index);
void            tile_button_view_activate_header_edit (TileButtonView *this, gint index);

G_END_DECLS

#endif
