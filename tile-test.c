#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnome/gnome-desktop-item.h>

#include "application-tile.h"
#include "document-tile.h"

static gboolean
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return FALSE;
}

static void
destroy_cb (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

int
main (int argc, char **argv)
{
	GnomeProgram *prog;

	GtkWidget *window;
	GtkWidget *vbox;

	GtkWidget *entry;

	DocumentTile    *odp_tile;
	DocumentTile    *png_tile;
	DocumentTile    *nfs_tile;
	DocumentTile    *txt_tile;
	ApplicationTile *gvim_tile;
	ApplicationTile *gcnf_tile;

    
	prog = gnome_program_init ("tile-test", "1.0", LIBGNOMEUI_MODULE, argc, argv, NULL, NULL);

	gtk_init (& argc, & argv);
    
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    
	g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (delete_event_cb), NULL);
	g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_cb), NULL);

	entry = gtk_entry_new ();
    
	odp_tile  = document_tile_new ("file:///home/jimmyk/docs/tiles.odp");
	png_tile  = document_tile_new ("file:///home/jimmyk/img/slab.png");
	nfs_tile  = document_tile_new ("file:///mnt/dudley/scratch/jimmyk/gilouche.png");
	txt_tile  = document_tile_new ("file:///home/jimmyk/slab/svn/gnome-main-menu/branches/tiles-2/hehe.txt");
	gvim_tile = application_tile_new ("gvim.desktop");
	gcnf_tile = application_tile_new ("gconf-editor.desktop");

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), tile_get_widget (TILE (odp_tile)),  FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), tile_get_widget (TILE (png_tile)),  FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), tile_get_widget (TILE (nfs_tile)),  FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), tile_get_widget (TILE (txt_tile)),  FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), tile_get_widget (TILE (gvim_tile)), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), tile_get_widget (TILE (gcnf_tile)), FALSE, FALSE, 0);
    
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (window), 6);
    
	gtk_widget_show_all (window);
    
	gtk_main ();

	gnome_vfs_shutdown ();

	g_object_unref (G_OBJECT (odp_tile));
	g_object_unref (G_OBJECT (png_tile));
	g_object_unref (G_OBJECT (nfs_tile));
	g_object_unref (G_OBJECT (txt_tile));
	g_object_unref (G_OBJECT (gvim_tile));
	g_object_unref (G_OBJECT (gcnf_tile));
    
	return 0;
}
