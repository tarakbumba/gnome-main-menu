#ifndef __TILE_ATTRIBUTE_H__
#define __TILE_ATTRIBUTE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TILE_ATTRIBUTE_TYPE         (tile_attribute_get_type ())
#define TILE_ATTRIBUTE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_ATTRIBUTE_TYPE, TileAttribute))
#define TILE_ATTRIBUTE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TILE_ATTRIBUTE_TYPE, TileAttributeClass))
#define IS_TILE_ATTRIBUTE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_ATTRIBUTE_TYPE))
#define IS_TILE_ATTRIBUTE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TILE_ATTRIBUTE_TYPE))
#define TILE_ATTRIBUTE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TILE_ATTRIBUTE_TYPE, TileAttributeClass))

#define TILE_ATTRIBUTE_VALUE_PROP  "tile-attribute-value"
#define TILE_ATTRIBUTE_ACTIVE_PROP "tile-attribute-active"

typedef struct {
	GObject g_object;
} TileAttribute;

typedef struct {
	GObjectClass g_object_class;
} TileAttributeClass;

GType tile_attribute_get_type (void);

TileAttribute *tile_attribute_new         (GType type);
GValue        *tile_attribute_get_value   (TileAttribute *this);
void           tile_attribute_set_boolean (TileAttribute *this, gboolean b);
void           tile_attribute_set_int     (TileAttribute *this, gint i);
void           tile_attribute_set_long    (TileAttribute *this, glong l);
void           tile_attribute_set_string  (TileAttribute *this, const gchar *s);
void           tile_attribute_set_pointer (TileAttribute *this, gpointer p);
void           tile_attribute_set_active  (TileAttribute *this, gboolean active);

G_END_DECLS

#endif
