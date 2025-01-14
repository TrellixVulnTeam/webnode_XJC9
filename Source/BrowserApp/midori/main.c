/*
*/

#include "midori-frontend.h"
#include "midori-app.h"
#include "midori-platform.h"
#include "midori-privatedata.h"
#include "midori-searchaction.h"
#include "midori/midori-session.h"
#include <midori/cdosbrowser-core.h>

#include <config.h>
#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include "katze/katze.h"
#include <sqlite3.h>

static void
plain_entry_activate_cb (GtkWidget* entry,
                         GtkWidget* web_view)
{
    gchar* uri = sokoke_magic_uri (gtk_entry_get_text (GTK_ENTRY (entry)), FALSE, TRUE);
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (web_view), uri);
    g_free (uri);
}

#ifndef HAVE_WEBKIT2
static void
snapshot_load_finished_cb (GtkWidget*      web_view,
                           WebKitWebFrame* web_frame,
                           gchar*          filename)
{
    GdkPixbuf* pixbuf = gtk_offscreen_window_get_pixbuf (GTK_OFFSCREEN_WINDOW (
        gtk_widget_get_parent (web_view)));
    gdk_pixbuf_save (pixbuf, filename, "png", NULL, "compression", "7", NULL);
    g_object_unref (pixbuf);
    g_print (_("Snapshot saved to: %s\n"), filename);
    gtk_main_quit ();
}
#endif

//extern guint web_extension_count;

static void 
initialize_web_extensions (WebKitWebContext* web_context, gboolean save_password)
{
  GVariant *user_data;
  gboolean private_profile;
  char *web_extension_id;

  //webkit_web_context_set_web_extensions_directory (web_context, /*BROWSER_WEB_EXTENSION_PATH*/"/home/wangjiayu/workspace/web-extension/Webkit2Browser/lib/web-extension");
  webkit_web_context_set_web_extensions_directory (web_context, BROWSER_WEB_EXTENSION_PATH);
  guint* webExtensionsCount = midori_app_get_web_extension_count();
  web_extension_id = g_strdup_printf ("%u-%u", getpid (), ++(*webExtensionsCount));

  private_profile = false;
  user_data = g_variant_new ("(sb)", web_extension_id, save_password);
  webkit_web_context_set_web_extensions_initialization_user_data (web_context, user_data);
}

int
main (int    argc,
      char** argv)
{
#ifdef APP_LEVEL_TIME
printf("main start time = %lld\n",g_get_real_time());
#endif
    gchar* webapp;
    gchar* config;
    gboolean private;
    gboolean portable;
    gboolean plain;
    gboolean diagnostic_dialog = FALSE;
    gboolean debug = FALSE;
    gboolean run;
    gchar* snapshot;
    gchar** execute;
    gboolean help_execute;
    gboolean version;
    gchar** uris;
    gchar* block_uris;
    gint inactivity_reset;
    gboolean single_process = FALSE;
    GOptionEntry entries[] =
    {
       { "app", 'a', 0, G_OPTION_ARG_STRING, &webapp,
       N_("Run ADDRESS as a web application"), N_("ADDRESS") },
       { "config", 'c', 0, G_OPTION_ARG_FILENAME, &config,
       N_("Use FOLDER as configuration folder"), N_("FOLDER") },
       { "private", 'p', 0, G_OPTION_ARG_NONE, &private,
       N_("Private browsing, no changes are saved"), NULL },
       { "plain", '\0', 0, G_OPTION_ARG_NONE, &plain,
       N_("Plain GTK+ window with WebKit, akin to GtkLauncher"), NULL },
       { "diagnostic-dialog", 'd', 0, G_OPTION_ARG_NONE, &diagnostic_dialog,
       N_("Show a diagnostic dialog"), NULL },
       { "debug", 'g', 0, G_OPTION_ARG_NONE, &debug,
       N_("Run within gdb and save a backtrace on crash"), NULL },
       { "run", 'r', 0, G_OPTION_ARG_NONE, &run,
       N_("Run the specified filename as javascript"), NULL },
       { "snapshot", 's', 0, G_OPTION_ARG_STRING, &snapshot,
       N_("Take a snapshot of the specified URI"), NULL },
       { "execute", 'e', 0, G_OPTION_ARG_STRING_ARRAY, &execute,
       N_("Execute the specified command"), NULL },
       { "help-execute", 0, 0, G_OPTION_ARG_NONE, &help_execute,
       N_("List available commands to execute with -e/ --execute"), NULL },
       { "single-process", 0, 0, G_OPTION_ARG_NONE, &single_process,
       N_("single-process with --single-process"), NULL },
#if 0
       { "version", 'V', 0, G_OPTION_ARG_NONE, &version,
       N_("Display program version"), NULL },
#endif
       { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &uris,
       N_("Addresses"), NULL },
       { "block-uris", 'b', 0, G_OPTION_ARG_STRING, &block_uris,
       N_("Block URIs according to regular expression PATTERN"), _("PATTERN") },
       #ifdef HAVE_X11_EXTENSIONS_SCRNSAVER_H
       { "inactivity-reset", 'i', 0, G_OPTION_ARG_INT, &inactivity_reset,
       /* i18n: CLI: Close tabs, clear private data, open starting page */
       N_("Reset Midori after SECONDS seconds of inactivity"), N_("SECONDS") },
       #endif
     { NULL }
    };

    // 设置InjectedBundle相关的全局变量
    g_setenv("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);
    g_setenv("WEBKIT_EXEC_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);

    /* Parse cli options */
    webapp = NULL;
    config = NULL;
    private = FALSE;
    portable = FALSE;
    plain = FALSE;
    run = FALSE;
    snapshot = NULL;
    execute = NULL;
    help_execute = FALSE;
    version = FALSE;
    uris = NULL;
    block_uris = NULL;
    inactivity_reset = 0;
    
    midori_app_setup (&argc, &argv, entries);

    // ZRL ignore TLS error. Otherwise, mail.cstnet.cn cannot be accessed. 2014.12.02
    WebKitWebContext* context = webkit_web_context_get_default ();
    webkit_web_context_set_tls_errors_policy(context, WEBKIT_TLS_ERRORS_POLICY_IGNORE);
    g_signal_connect (context, "initialize-web-extensions",
                    G_CALLBACK (initialize_web_extensions),
                    NULL);
#if 1    
    if(!single_process)
#else		
//add by luyue 2015/3/30
//保证用户名密码登录功能能够正常使用，暂时屏蔽多进程 
    if(single_process)
#endif		
    {
        webkit_web_context_set_process_model(context, WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);
    }

    if (debug)
    {
        gchar* gdb = g_find_program_in_path ("gdb");
        if (gdb == NULL)
            midori_error (_("Error: \"gdb\" can't be found\n"));
        sokoke_spawn_gdb (gdb, TRUE);
        g_free (gdb);
        return 0;
    }

    g_set_application_name (_("Midori"));
    /* Versioned prgname to override menuproxy blacklist */
    g_set_prgname (PACKAGE_NAME "4");

    if (version)
    {
        GString* versions = g_string_new ("");
        g_string_append_c (versions, '\n');
        midori_view_list_versions (versions, FALSE);
        /* FIXME Plugins may clutter output and hang
        midori_view_list_plugins (NULL, versions, FALSE); */
        g_print ("%s\n", versions->str);
        g_string_free (versions, TRUE);

        g_print (
          "Copyright (c) 2013-2014 Christian Dywan\n\n"
          "%s\n"
          "\t%s\n\n"
          "%s\n"
          "\thttp://www.midori-browser.org\n",
          _("Please report comments, suggestions and bugs to:"),
          PACKAGE_BUGREPORT,
          _("Check for new versions at:")
        );
        return 0;
    }

    if (help_execute)
    {
        MidoriBrowser* browser = midori_browser_new ();
        GtkActionGroup* action_group = midori_browser_get_action_group (browser);
        GList* actions = gtk_action_group_list_actions (action_group);
        GList* temp = actions;
        GObjectClass* class = G_OBJECT_GET_CLASS (midori_browser_get_settings (browser));
        guint i, n_properties;
        GParamSpec** pspecs = g_object_class_list_properties (class, &n_properties);
        guint length = 1;
        gchar* space;

        for (; temp; temp = g_list_next (temp))
            length = MAX (length, 1 + strlen (gtk_action_get_name (temp->data)));
        for (i = 0; i < n_properties; i++)
            length = MAX (length, 1 + strlen (g_param_spec_get_name (pspecs[i])));

        space = g_strnfill (length, ' ');
        for (; actions; actions = g_list_next (actions))
        {
            GtkAction* action = actions->data;
            const gchar* name = gtk_action_get_name (action);
            gchar* padding = g_strndup (space, strlen (space) - strlen (name));
            gchar* label = katze_object_get_string (action, "label");
            gchar* stripped = katze_strip_mnemonics (label);
            gchar* tooltip = katze_object_get_string (action, "tooltip");
            g_print ("%s%s%s%s%s\n", name, padding, stripped,
                     tooltip ? ": " : "", tooltip ? tooltip : "");
            g_free (tooltip);
            g_free (padding);
            g_free (label);
            g_free (stripped);
        }
        g_list_free (actions);
        g_print ("\n");

        for (i = 0; i < n_properties; i++)
        {
            GParamSpec* pspec = pspecs[i];
            if (!(pspec->flags & G_PARAM_WRITABLE))
                continue;
            const gchar* property = g_param_spec_get_name (pspec);
            gchar* padding = g_strndup (space, strlen (space) - strlen (property));
            GType type = G_PARAM_SPEC_TYPE (pspec);
            const gchar* tname;
            GString* tname_string = NULL;
            if (type == G_TYPE_PARAM_STRING)
                tname = "string";
            else if (type == G_TYPE_PARAM_BOOLEAN)
                tname = "true/ false";
            else if (type == G_TYPE_PARAM_ENUM)
            {
                GEnumClass* enum_class = G_ENUM_CLASS (g_type_class_peek (pspec->value_type));
                guint j = 0;
                tname_string = g_string_new ("");
                for (j = 0; j < enum_class->n_values; j++)
                {
                    g_string_append (tname_string, enum_class->values[j].value_name);
                    g_string_append (tname_string, j == 2 ? "\n    " : " ");
                }
                tname = tname_string->str;
            }
            else
                tname = "number";
            g_print ("%s%s%s\n", property, padding, tname);
            if (tname_string != NULL)
                g_string_free (tname_string, TRUE);
            g_free (padding);
        }
        g_free (pspecs);

        g_free (space);
        gtk_widget_destroy (GTK_WIDGET (browser));
        return 0;
    }

    if (snapshot)
    {
        GError* error = NULL;
        gchar* filename;
        GtkWidget* web_view;
        gchar* uri;
        GtkWidget* offscreen;
        GdkScreen* screen;

        gint fd = g_file_open_tmp ("snapshot-XXXXXX.png", &filename, &error);
        close (fd);

        if (error)
        {
            g_error ("%s", error->message);
            return 1;
        }

        if (g_unlink (filename) == -1)
        {
            g_error ("%s", g_strerror (errno));
            return 1;
        }

        web_view = webkit_web_view_new ();
        offscreen = gtk_offscreen_window_new ();
        gtk_container_add (GTK_CONTAINER (offscreen), web_view);
        if ((screen = gdk_screen_get_default ()))
            gtk_widget_set_size_request (web_view,
                gdk_screen_get_width (screen), gdk_screen_get_height (screen));
        else
            gtk_widget_set_size_request (web_view, 800, 600);
        gtk_widget_show_all (offscreen);
        #ifndef HAVE_WEBKIT2
        g_signal_connect (web_view, "load-finished",
            G_CALLBACK (snapshot_load_finished_cb), filename);
        #endif
        uri = sokoke_magic_uri (snapshot, FALSE, TRUE);
        webkit_web_view_load_uri (WEBKIT_WEB_VIEW (web_view), uri);
        g_free (uri);
        gtk_main ();
        g_free (filename);
        return 0;
    }

    if (plain)
    {
        GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
        GtkWidget* entry = gtk_entry_new ();
#ifndef HAVE_WEBKIT2
        GtkWidget* scrolled = gtk_scrolled_window_new (NULL, NULL);
#endif
        GtkWidget* web_view = webkit_web_view_new ();
        katze_window_set_sensible_default_size (GTK_WINDOW (window));

        gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
#ifndef HAVE_WEBKIT2
        gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
        gtk_container_add (GTK_CONTAINER (scrolled), web_view);
#else
        gtk_box_pack_start (GTK_BOX (vbox), web_view, TRUE, TRUE, 0);
#endif
        gtk_container_add (GTK_CONTAINER (window), vbox);
        gtk_entry_set_text (GTK_ENTRY (entry), uris && *uris ? *uris : "http://www.baidu.com");
        plain_entry_activate_cb (entry, web_view);
        g_signal_connect (entry, "activate",
            G_CALLBACK (plain_entry_activate_cb), web_view);
        g_signal_connect (window, "delete-event",
            G_CALLBACK (gtk_main_quit), window);
        gtk_widget_show_all (window);
        gtk_main ();
        return 0;
    }

    midori_private_data_register_built_ins ();

    if (run)
    {
        gchar* script = NULL;
        GError* error = NULL;

        if (g_file_get_contents (uris ? *uris : NULL, &script, NULL, &error))
        {
            midori_paths_init (MIDORI_RUNTIME_MODE_PRIVATE, config);

            MidoriBrowser* browser = midori_browser_new ();
            MidoriWebSettings* settings = midori_browser_get_settings (browser);
#ifndef HAVE_WEBKIT2
            g_object_set_data (G_OBJECT (webkit_get_default_session ()), "pass-through-console", (void*)1);
#endif
            midori_load_soup_session (settings);

            gchar* msg = NULL;
            GtkWidget* view = midori_view_new_with_item (NULL, settings);
            g_object_set (settings, "open-new-pages-in", MIDORI_NEW_PAGE_WINDOW, NULL);
            midori_browser_add_tab (browser, view);
            gtk_widget_show_all (GTK_WIDGET (browser));
            gtk_widget_hide (GTK_WIDGET (browser));
            midori_view_execute_script (MIDORI_VIEW (view), script, &msg);
            if (msg != NULL)
            {
                g_error ("%s\n", msg);
                g_free (msg);
            }
            gtk_main ();
        }
        else if (error != NULL)
        {
            g_error ("%s\n", error->message);
            g_error_free (error);
        }
        else
            g_error ("%s\n", _("An unknown error occured"));
        g_free (script);
        return 0;
    }

    if (private)
    {
        MidoriBrowser* browser = midori_private_app_new (config, webapp,
            uris, execute, inactivity_reset, block_uris);
        g_signal_connect (browser, "destroy", G_CALLBACK (gtk_main_quit), NULL);
        g_signal_connect (browser, "quit", G_CALLBACK (gtk_main_quit), NULL);
        gtk_main ();
        return 0;
    }

    if (webapp)
    {
        MidoriBrowser* browser = midori_web_app_new (webapp,
            uris, execute, inactivity_reset, block_uris);
        g_signal_connect (browser, "destroy", G_CALLBACK (gtk_main_quit), NULL);
        g_signal_connect (browser, "quit", G_CALLBACK (gtk_main_quit), NULL);
        gtk_main ();
        return 0;
    }
#ifdef APP_LEVEL_TIME
printf("app create start time = %lld\n",g_get_real_time());
#endif
    MidoriApp* app = midori_normal_app_new (config,
        portable ? "portable" : "normal", diagnostic_dialog,
        uris, execute, inactivity_reset, block_uris);
#ifdef APP_LEVEL_TIME
printf("app create end time = %lld\n",g_get_real_time());
#endif
    if (app == NULL)
        return 0;
    if (app == (void*)0xdeadbeef)
        return 1;

    g_signal_connect (app, "quit", G_CALLBACK (gtk_main_quit), NULL);
    gtk_main ();
    midori_normal_app_on_quit (app);
    g_object_unref (app);
    return 0;
}

