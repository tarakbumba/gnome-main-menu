/*
 * This file is part of the Control Center.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Control Center; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006 Novell, Inc.
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

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkentry.h>
#include <panel-applet.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomeui/libgnomeui.h>
#include <dirent.h>

#include "app-shell.h"
#include "app-shell-startup.h"
#include "slab-gnome-util.h"

#define NEW_APPS_MAX_ITEMS          "/desktop/gnome/applications/main-menu/ab_new_apps_max_items"
#define APPLICATION_BROWSER_PREFIX  "/desktop/gnome/applications/main-menu/ab_"

int
main (int argc, char *argv [])
{
	BonoboApplication *bonobo_app = NULL;
	gboolean hidden = FALSE;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif
	/* gnome_program_init clears the env variable that we depend on, so
	   call this first */
	if (apss_already_running
	    (argc, argv, &bonobo_app, "GNOME-NLD-AppBrowser")) {
		gnome_program_init ("Gnome Application Browser", "0.1",
				    LIBGNOMEUI_MODULE, argc, argv, NULL,
				    NULL);
		gdk_notify_startup_complete ();
		bonobo_debug_shutdown ();
		exit (1);
	}

	gnome_program_init ("Gnome Application Browser", "0.1",
			    LIBGNOMEUI_MODULE, argc, argv, NULL, NULL);

	if (argc > 1) {
		if (argc != 2 || strcmp ("-h", argv [1])) {
			printf ("Usage - application-browser  [-h]\n");
			printf ("Options: -h : hide on start\n");
			printf ("\tUseful if you want to autostart the application-browser singleton so it can get all it's slow loading done\n");
			exit (1);
		}
		hidden = TRUE;
	}

	NewAppConfig *config = g_new0 (NewAppConfig, 1);

	config->max_items = get_slab_gconf_int (NEW_APPS_MAX_ITEMS);
	config->name = _("New Applications");
	AppShellData *app_data =
		appshelldata_new ("applications.menu", config,
				  APPLICATION_BROWSER_PREFIX,
				  GTK_ICON_SIZE_DND);
	generate_categories (app_data);

	layout_shell (app_data, _("Filter"), _("Groups"),
		      _("Application Actions"), NULL, NULL);

	g_signal_connect (bonobo_app, "new-instance",
			  G_CALLBACK (apss_new_instance_cb), app_data);
	create_main_window (app_data, "MyApplicationBrowser",
			    _("Application Browser"), "gnome-fs-client", 940,
			    600, hidden);

	if (bonobo_app)
		bonobo_object_unref (bonobo_app);
	bonobo_debug_shutdown ();
	return 0;
};
