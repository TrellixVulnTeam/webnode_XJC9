/*
 Copyright (C) 2007-2013 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2008 Dale Whittaker <dayul@users.sf.net>
 Copyright (C) 2009 Jérôme Geulfucci <jeromeg@xfce.org>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.

 Modified by ZRL
 2014.12.10 修复网页中打开新窗口或新Tab时，不加载网页问题。参考midori_view_new_view_cb()
*/

#include "midori-browser.h"

#include "midori-app.h"
#include "midori-extension.h"
#include "midori-array.h"
#include "midori-view.h"
#include "midori-preferences.h"
#include "midori-panel.h"
#include "midori-locationaction.h"
#include "midori-searchaction.h"
#include "midori-findbar.h"
#include "midori-platform.h"
#include "midori-privatedata.h"
#include "midori-core.h"
#include "midori-privatedata.h"
#include "midori-bookmarks-db.h"
#include "katze-cellrenderercomboboxtext.h"

#include "marshal.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

#include <config.h>

#ifdef HAVE_GRANITE
    #include <granite.h>
#endif

#ifdef HAVE_ZEITGEIST
    #include <zeitgeist.h>
#endif

#ifdef HAVE_UNISTD_H
    #include <unistd.h>
#endif

#include <sqlite3.h>

#ifdef HAVE_X11_EXTENSIONS_SCRNSAVER_H
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/extensions/scrnsaver.h>
    #include <gdk/gdkx.h>
#endif

struct _MidoriBrowser
{
    GtkWindow parent_instance;
    GtkActionGroup* action_group;
    GtkWidget* menubar;
    GtkWidget* throbber;
    GtkWidget* navigationbar;
    GtkWidget* bookmarkbar;

    GtkWidget* panel;
    GtkWidget* notebook;

    GtkWidget* inspector;
    GtkWidget* inspector_view;

    GtkWidget* find;

    GtkWidget* statusbar;
    GtkWidget* statusbar_contents;
    gchar* statusbar_text;

    gint last_window_width, last_window_height;
    guint alloc_timeout;
    guint panel_timeout;

    MidoriWebSettings* settings;
    KatzeArray* proxy_array;
    MidoriBookmarksDb* bookmarks;
    KatzeArray* trash;
    KatzeArray* search_engines;
    KatzeArray* history;
    MidoriHistoryDatabase* history_database;
    MidoriSpeedDial* dial;
    gboolean show_tabs;

    gboolean show_navigationbar;
    gboolean show_statusbar;
    guint maximum_history_age;
    guint last_web_search;

    gboolean bookmarkbar_populate;
};

G_DEFINE_TYPE (MidoriBrowser, midori_browser, GTK_TYPE_WINDOW)

enum
{
    PROP_0,

    PROP_MENUBAR,
    PROP_NAVIGATIONBAR,
    PROP_NOTEBOOK,
    PROP_PANEL,
    PROP_URI,
    PROP_TAB,
    PROP_LOAD_STATUS,
    PROP_STATUSBAR,
    PROP_STATUSBAR_TEXT,
    PROP_SETTINGS,
    PROP_PROXY_ITEMS,
    PROP_BOOKMARKS,
    PROP_TRASH,
    PROP_SEARCH_ENGINES,
    PROP_HISTORY,
    PROP_SPEED_DIAL,
    PROP_SHOW_TABS,
};

enum
{
    NEW_WINDOW,
    ADD_TAB,
    REMOVE_TAB,
    MOVE_TAB,
    SWITCH_TAB,
    ACTIVATE_ACTION,
    ADD_DOWNLOAD,
    SEND_NOTIFICATION,
    POPULATE_TOOL_MENU,
    POPULATE_TOOLBAR_MENU,
    QUIT,
    SHOW_PREFERENCES,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
midori_browser_dispose (GObject* object);

static void
midori_browser_finalize (GObject* object);

#ifdef HAVE_WEBKIT2
void download_created_destination_cb (WebKitDownload *download,
                                      gchar          *destination,
                                      gpointer        user_data);
#endif

static void
midori_browser_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec);

static void
midori_browser_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec);

gboolean
midori_browser_open_bookmark (MidoriBrowser* browser,
                              KatzeItem*     item);

static void
midori_bookmarkbar_populate (MidoriBrowser* browser);
static void
midori_bookmarkbar_populate_idle (MidoriBrowser* browser);

static void
midori_bookmarkbar_clear (GtkWidget* toolbar);

static void
_midori_browser_set_toolbar_style (MidoriBrowser*     browser,
                                   MidoriToolbarStyle toolbar_style);

static void
midori_browser_settings_notify (MidoriWebSettings* web_settings,
                                GParamSpec*        pspec,
                                MidoriBrowser*     browser);

void
midori_panel_set_toolbar_style (MidoriPanel*    panel,
                                GtkToolbarStyle style);

static void
midori_browser_set_bookmarks (MidoriBrowser*     browser,
                              MidoriBookmarksDb* bookmarks);

static void
midori_browser_add_speed_dial (MidoriBrowser* browser);

static void
midori_browser_step_history (MidoriBrowser* browser,
                             MidoriView*    view);

#define _action_by_name(brwsr, nme) \
    gtk_action_group_get_action (brwsr->action_group, nme)
#define _action_set_sensitive(brwsr, nme, snstv) \
    gtk_action_set_sensitive (_action_by_name (brwsr, nme), snstv);
#define _action_set_visible(brwsr, nme, vsbl) \
    gtk_action_set_visible (_action_by_name (brwsr, nme), vsbl);
#define _action_set_active(brwsr, nme, actv) \
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION ( \
    _action_by_name (brwsr, nme)), actv);

static void
midori_browser_disconnect_tab (MidoriBrowser* browser,
                               MidoriView*    view);


static gboolean
midori_browser_is_fullscreen (MidoriBrowser* browser)
{
    GdkWindow* window = gtk_widget_get_window (GTK_WIDGET (browser));
    GdkWindowState state = window ? gdk_window_get_state (window) : 0;
    return state & GDK_WINDOW_STATE_FULLSCREEN;
}

static gboolean
_toggle_tabbar_smartly (MidoriBrowser* browser,
                        gboolean       ignore_fullscreen)
{
    gboolean has_tabs = midori_browser_get_n_pages (browser) > 1;
    gboolean show_tabs = !midori_browser_is_fullscreen (browser) || ignore_fullscreen;
    if (!browser->show_tabs)
        show_tabs = FALSE;
    midori_notebook_set_labels_visible (MIDORI_NOTEBOOK (browser->notebook), show_tabs);
    return has_tabs;
}

static void
midori_browser_trash_clear_cb (KatzeArray*    trash,
                               MidoriBrowser* browser)
{
    gboolean trash_empty = katze_array_is_empty (browser->trash);
    _action_set_sensitive (browser, "UndoTabClose", !trash_empty);
    _action_set_sensitive (browser, "Trash", !trash_empty);
}

static void
_midori_browser_update_actions (MidoriBrowser* browser)
{
    gboolean has_tabs = _toggle_tabbar_smartly (browser, FALSE);
    _action_set_sensitive (browser, "TabPrevious", has_tabs);
    _action_set_sensitive (browser, "TabNext", has_tabs);

    if (browser->trash)
        midori_browser_trash_clear_cb (browser->trash, browser);
}

static void
midori_browser_update_secondary_icon (MidoriBrowser* browser,
                                      MidoriView*    view,
                                      GtkAction*     action)
{
    if (g_object_get_data (G_OBJECT (view), "news-feeds"))
    {
        midori_location_action_set_secondary_icon (
            MIDORI_LOCATION_ACTION (action), STOCK_NEWS_FEED);
        _action_set_sensitive (browser, "AddNewsFeed", TRUE);
    }
    else
    {
        midori_location_action_set_secondary_icon (
            MIDORI_LOCATION_ACTION (action), NULL);
        _action_set_sensitive (browser, "AddNewsFeed", FALSE);
    }
}

static void
_midori_browser_update_interface (MidoriBrowser* browser,
                                  MidoriView*    view)
{
    GtkAction* action;

    _action_set_sensitive (browser, "Back", midori_view_can_go_back (view));
    _action_set_sensitive (browser, "Forward", midori_tab_can_go_forward (MIDORI_TAB (view)));
    _action_set_visible (browser, "Forward", midori_tab_can_go_forward (MIDORI_TAB (view)));    //zgh
    _action_set_sensitive (browser, "Previous",
        midori_view_get_previous_page (view) != NULL);
    _action_set_sensitive (browser, "Next",
        midori_view_get_next_page (view) != NULL);

    _action_set_sensitive (browser, "AddSpeedDial", !midori_view_is_blank (view));
    _action_set_sensitive (browser, "BookmarkAdd", !midori_view_is_blank (view));
    _action_set_sensitive (browser, "MailTo", !midori_view_is_blank (view));
    _action_set_sensitive (browser, "SaveAs", midori_tab_can_save (MIDORI_TAB (view)));
    _action_set_sensitive (browser, "ZoomIn", midori_view_can_zoom_in (view));
    _action_set_sensitive (browser, "ZoomOut", midori_view_can_zoom_out (view));
    _action_set_sensitive (browser, "ZoomNormal",
        midori_view_get_zoom_level (view) != 1.0f);
    _action_set_sensitive (browser, "Encoding",
        midori_tab_can_view_source (MIDORI_TAB (view)));
    _action_set_sensitive (browser, "SourceView",
        midori_tab_can_view_source (MIDORI_TAB (view)));
    _action_set_sensitive (browser, "SourceViewDom",
        midori_tab_can_view_source (MIDORI_TAB (view)));

    action = _action_by_name (browser, "NextForward");
    if (midori_tab_can_go_forward (MIDORI_TAB (view)))
    {
        g_object_set (action,
                      "stock-id", GTK_STOCK_GO_FORWARD,
                      "tooltip", _("Go forward to the next page"),
                      "sensitive", TRUE, NULL);
    }
    else
    {
        g_object_set (action,
                      "stock-id", GTK_STOCK_MEDIA_NEXT,
                      "tooltip", _("Go to the next sub-page"),
                      "sensitive", midori_view_get_next_page (view) != NULL, NULL);
    }

    action = _action_by_name (browser, "Location");
#if 1 //zgh
    midori_location_action_set_secondary_icon (
            MIDORI_LOCATION_ACTION (action), STOCK_BOOKMARK_ADD);
#endif
    if (midori_tab_is_blank (MIDORI_TAB (view)))
    {
        gchar* icon_names[] = { "edit-find-symbolic", "edit-find", NULL };
        GIcon* icon = g_themed_icon_new_from_names (icon_names, -1);
        midori_location_action_set_primary_icon (
            MIDORI_LOCATION_ACTION (action), icon, _("Web Search…"));
        g_object_unref (icon);
    }
    else
        midori_location_action_set_security_hint (
            MIDORI_LOCATION_ACTION (action), midori_tab_get_security (MIDORI_TAB (view)));
//    midori_browser_update_secondary_icon (browser, view, action);   //zgh 1203
}

static void
_midori_browser_set_statusbar_text (MidoriBrowser* browser,
                                    MidoriView*    view,
                                    const gchar*   text)
{
    #if GTK_CHECK_VERSION (3, 2, 0)
    gboolean is_location = FALSE;
    #else
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    gboolean is_location = widget && GTK_IS_ENTRY (widget)
        && GTK_IS_ALIGNMENT (gtk_widget_get_parent (widget));
    #endif

    katze_assign (browser->statusbar_text, midori_uri_format_for_display (text));
    if (view == NULL)
        return;

    if (!gtk_widget_get_visible (browser->statusbar) && !is_location
     && text && *text)
    {
        #if GTK_CHECK_VERSION (3, 2, 0)
        midori_view_set_overlay_text (view, browser->statusbar_text);
        #else
        GtkAction* action = _action_by_name (browser, "Location");
        MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (action);
        midori_location_action_set_text (location_action, browser->statusbar_text);
//        midori_location_action_set_secondary_icon (location_action, NULL);  //zgh   1203
        #endif
    }
    else if (!gtk_widget_get_visible (browser->statusbar) && !is_location)
    {
        #if GTK_CHECK_VERSION (3, 2, 0)
        midori_view_set_overlay_text (view, NULL);
        #else
        GtkAction* action = _action_by_name (browser, "Location");
        MidoriLocationAction* location_action = MIDORI_LOCATION_ACTION (action);
//        midori_browser_update_secondary_icon (browser, view, action);   //zgh 1203
        midori_location_action_set_text (location_action,
            midori_view_get_display_uri (view));
        #endif
    }
    else
    {
        gtk_statusbar_pop (GTK_STATUSBAR (browser->statusbar), 1);
        gtk_statusbar_push (GTK_STATUSBAR (browser->statusbar), 1,
                            katze_str_non_null (browser->statusbar_text));
    }
}

void
midori_browser_set_current_page_smartly (MidoriBrowser* browser,
                                         gint           n)
{
    if (!katze_object_get_boolean (browser->settings,
        "open-tabs-in-the-background"))
        midori_browser_set_current_page (browser, n);
}

/**
 * midori_browser_set_current_tab_smartly:
 * @browser: a #MidoriBrowser
 * @view: a #GtkWidget
 *
 * Switches to the tab containing @view iff open-tabs-in-the-background is %FALSE.
 *
 * Since: 0.4.9
 **/
void
midori_browser_set_current_tab_smartly (MidoriBrowser* browser,
                                        GtkWidget*     view)
{
    if (!katze_object_get_boolean (browser->settings,
        "open-tabs-in-the-background"))
        midori_browser_set_current_tab (browser, view);
}

static void
_midori_browser_update_progress (MidoriBrowser* browser,
                                 MidoriView*    view)
{
    GtkAction* action;
    gdouble progress = midori_view_get_progress (view);
    gboolean loading = progress > 0.0;

    action = _action_by_name (browser, "Location");
    midori_location_action_set_progress (MIDORI_LOCATION_ACTION (action), progress);

    _action_set_sensitive (browser, "Reload", !loading);
    _action_set_sensitive (browser, "Stop", loading);

    action = _action_by_name (browser, "ReloadStop");
    if (!loading)
    {
        g_object_set (action,
                      "stock-id", GTK_STOCK_REFRESH,
                      "tooltip", _("Reload the current page"), NULL);
    }
    else
    {
        g_object_set (action,
                      "stock-id", GTK_STOCK_STOP,
                      "tooltip", _("Stop loading the current page"), NULL);
    }

    g_object_set (browser->throbber, "active", loading, "visible", loading, NULL);
}

/**
 * midori_browser_update_history:
 * @item: a #KatzeItem
 * @type: "website", "bookmark" or "download"
 * @event: "access", "leave", "modify", "delete"
 *
 * Since: 0.4.7
 **/
void
midori_browser_update_history (KatzeItem*   item,
                               const gchar* type,
                               const gchar* event)
{
    g_return_if_fail (!KATZE_ITEM_IS_SEPARATOR (item));

    #ifdef HAVE_ZEITGEIST
    const gchar* inter;
    if (strstr (event, "access"))
        inter = ZEITGEIST_ZG_ACCESS_EVENT;
    else if (strstr (event, "leave"))
        inter = ZEITGEIST_ZG_LEAVE_EVENT;
    else if (strstr (event, "modify"))
        inter = ZEITGEIST_ZG_MODIFY_EVENT;
    else if (strstr (event, "create"))
        inter = ZEITGEIST_ZG_CREATE_EVENT;
    else if (strstr (event, "delete"))
        inter = ZEITGEIST_ZG_DELETE_EVENT;
    else
        g_assert_not_reached ();

    /* FIXME: Should insert folders into the log (ZEITGEIST_NFO_BOOKMARK_FOLDER) */
    if (KATZE_ITEM_IS_FOLDER (item))
        return;

    zeitgeist_log_insert_events_no_reply (zeitgeist_log_get_default (),
        zeitgeist_event_new_full (inter, ZEITGEIST_ZG_USER_ACTIVITY,
                                  "application://midori.desktop",
                                  zeitgeist_subject_new_full (
            katze_item_get_uri (item),
            strstr (type, "bookmark") ? ZEITGEIST_NFO_BOOKMARK : ZEITGEIST_NFO_WEBSITE,
            zeitgeist_manifestation_for_uri (katze_item_get_uri (item)),
            katze_item_get_meta_string (item, "mime-type"), NULL, katze_item_get_name (item), NULL),
                                  NULL),
        NULL);
    #endif
}

static void
midori_browser_update_history_title (MidoriBrowser* browser,
                                     KatzeItem*     item)
{
    sqlite3* db;
    static sqlite3_stmt* stmt = NULL;

    g_return_if_fail (katze_item_get_uri (item) != NULL);

    db = g_object_get_data (G_OBJECT (browser->history), "db");
    g_return_if_fail (db != NULL);
    if (!stmt)
    {
        const gchar* sqlcmd;

        sqlcmd = "UPDATE history SET title=? WHERE uri = ? and date=?";
        sqlite3_prepare_v2 (db, sqlcmd, -1, &stmt, NULL);
    }
    sqlite3_bind_text (stmt, 1, katze_item_get_name (item), -1, 0);
    sqlite3_bind_text (stmt, 2, katze_item_get_uri (item), -1, 0);
    sqlite3_bind_int64 (stmt, 3, katze_item_get_added (item));

    if (sqlite3_step (stmt) != SQLITE_DONE)
        g_printerr (_("Failed to update title: %s\n"), sqlite3_errmsg (db));
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);

    midori_browser_update_history (item, "website", "access");
}

/**
 * midori_browser_assert_action:
 * @browser: a #MidoriBrowser
 * @name: action, setting=value expression or extension=true|false
 *
 * Assert that @name is a valid action or setting expression,
 * if it fails the program will terminate with an error.
 * To be used with command line interfaces.
 *
 * Since: 0.5.0
 **/
void
midori_browser_assert_action (MidoriBrowser* browser,
                              const gchar*   name)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (name != NULL);

    if (strchr (name, '='))
    {
        gchar** parts = g_strsplit (name, "=", 0);
        GObjectClass* class = G_OBJECT_GET_CLASS (browser->settings);
        GParamSpec* pspec = g_object_class_find_property (class, parts[0]);
        if (pspec != NULL)
        {
            GType type = G_PARAM_SPEC_TYPE (pspec);
            if (!(
                (type == G_TYPE_PARAM_BOOLEAN && (!strcmp (parts[1], "true") || !strcmp (parts[1], "false")))
             || type == G_TYPE_PARAM_STRING
             || type == G_TYPE_PARAM_INT
             || type == G_TYPE_PARAM_FLOAT
             || type == G_TYPE_PARAM_DOUBLE
             || type == G_TYPE_PARAM_ENUM))
                midori_error (_("Value '%s' is invalid for %s"), parts[1], parts[0]);
        }
        else
        {
            gchar* extension_path = midori_paths_get_lib_path (PACKAGE_NAME);
            GObject* extension = midori_extension_load_from_file (extension_path, parts[0], FALSE, FALSE);
            g_free (extension_path);
            if (!extension || (strcmp (parts[1], "true") && strcmp (parts[1], "false")))
                midori_error (_("Unexpected setting '%s'"), name);
        }
        g_strfreev (parts);
    }
    else
    {
        GtkAction* action = _action_by_name (browser, name);
        if (!action)
            midori_error (_("Unexpected action '%s'."), name);
    }
}

void
midori_app_set_browsers (MidoriApp*     app,
                         KatzeArray*    browsers,
                         MidoriBrowser* browser);

static void
_midori_browser_activate_action (MidoriBrowser* browser,
                                 const gchar*   name)
{
    g_return_if_fail (name != NULL);

    if (strchr (name, '='))
    {
        gchar** parts = g_strsplit (name, "=", 0);
        GObjectClass* class = G_OBJECT_GET_CLASS (browser->settings);
        GParamSpec* pspec = g_object_class_find_property (class, parts[0]);
        if (pspec != NULL)
        {
            GType type = G_PARAM_SPEC_TYPE (pspec);
            if (type == G_TYPE_PARAM_BOOLEAN && !strcmp ("true", parts[1]))
                g_object_set (browser->settings, parts[0], TRUE, NULL);
            else if (type == G_TYPE_PARAM_BOOLEAN && !strcmp ("false", parts[1]))
                g_object_set (browser->settings, parts[0], FALSE, NULL);
            else if (type == G_TYPE_PARAM_STRING)
                g_object_set (browser->settings, parts[0], parts[1], NULL);
            else if (type == G_TYPE_PARAM_INT || type == G_TYPE_PARAM_UINT)
                g_object_set (browser->settings, parts[0], atoi (parts[1]), NULL);
            else if (type == G_TYPE_PARAM_FLOAT || type == G_TYPE_PARAM_DOUBLE)
                g_object_set (browser->settings, parts[0], g_ascii_strtod (parts[1], NULL), NULL);
            else if (type == G_TYPE_PARAM_ENUM)
            {
                GEnumClass* enum_class = G_ENUM_CLASS (g_type_class_peek (pspec->value_type));
                GEnumValue* enum_value = g_enum_get_value_by_name (enum_class, parts[1]);
                if (enum_value != NULL)
                    g_object_set (browser->settings, parts[0], enum_value->value, NULL);
                else
                    g_warning (_("Value '%s' is invalid for %s"), parts[1], parts[0]);
            }
            else
                g_warning (_("Value '%s' is invalid for %s"), parts[1], parts[0]);
        }
        else
        {
            gchar* extension_path = midori_paths_get_lib_path (PACKAGE_NAME);
            GObject* extension = midori_extension_load_from_file (extension_path, parts[0], TRUE, FALSE);
            MidoriApp* app = midori_app_new_proxy (NULL);
            g_object_set (app,
                "settings", browser->settings,
                NULL);
            /* FIXME: tabs of multiple windows */
            KatzeArray* browsers = katze_array_new (MIDORI_TYPE_BROWSER);
            katze_array_add_item (browsers, browser);
            midori_app_set_browsers (app, browsers, browser);
            g_free (extension_path);
            if (extension && !strcmp (parts[1], "true"))
                midori_extension_activate (extension, parts[0], TRUE, app);
            else if (extension && !strcmp (parts[1], "false"))
                midori_extension_deactivate (MIDORI_EXTENSION (extension));
            else
                g_warning (_("Unexpected setting '%s'"), name);
        }
        g_strfreev (parts);
    }
    else
    {
        GtkAction* action = _action_by_name (browser, name);
        if (action)
            gtk_action_activate (action);
        else
            g_warning (_("Unexpected action '%s'."), name);
    }
}

static void
midori_view_notify_icon_cb (MidoriView*    view,
                            GParamSpec*    pspec,
                            MidoriBrowser* browser)
{
    if (midori_browser_get_current_tab (browser) != (GtkWidget*)view)
        return;

    if (midori_paths_get_runtime_mode () == MIDORI_RUNTIME_MODE_APP)
        gtk_window_set_icon (GTK_WINDOW (browser), midori_view_get_icon (view));
}

static void
midori_view_notify_load_status_cb (GtkWidget*      widget,
                                   GParamSpec*     pspec,
                                   MidoriBrowser*  browser)
{
    MidoriView* view = MIDORI_VIEW (widget);
    MidoriLoadStatus load_status = midori_view_get_load_status (view);

    if (widget == midori_browser_get_current_tab (browser))
    {
        if (load_status == MIDORI_LOAD_COMMITTED)
        {
            const gchar* uri = midori_view_get_display_uri (view);
            GtkAction* action = _action_by_name (browser, "Location");
            midori_location_action_set_text (
                MIDORI_LOCATION_ACTION (action), uri);

            /* Focus the urlbar on blank pages */
            if (midori_view_is_blank (view))
                midori_browser_activate_action (browser, "Location");
        }

        _midori_browser_update_interface (browser, view);
        _midori_browser_set_statusbar_text (browser, view, NULL);
    }

    if (load_status == MIDORI_LOAD_FINISHED)
        katze_item_set_meta_string (midori_view_get_proxy_item (view),
                                    "history-step", NULL);

    g_object_notify (G_OBJECT (browser), "load-status");
}

static void
midori_view_notify_progress_cb (GtkWidget*     view,
                                GParamSpec*    pspec,
                                MidoriBrowser* browser)
{
    if (view == midori_browser_get_current_tab (browser))
        _midori_browser_update_progress (browser, MIDORI_VIEW (view));
}

static void
midori_view_notify_uri_cb (GtkWidget*     widget,
                           GParamSpec*    pspec,
                           MidoriBrowser* browser)
{
    if (widget == midori_browser_get_current_tab (browser))
    {
        MidoriView* view = MIDORI_VIEW (widget);
        const gchar* uri = midori_view_get_display_uri (view);
        GtkAction* action = _action_by_name (browser, "Location");
        midori_location_action_set_text (MIDORI_LOCATION_ACTION (action), uri);
        _action_set_sensitive (browser, "Back", midori_view_can_go_back (view));
        _action_set_sensitive (browser, "Forward", midori_tab_can_go_forward (MIDORI_TAB (view)));
        g_object_notify (G_OBJECT (browser), "uri");
    }
}

static void
midori_browser_set_title (MidoriBrowser* browser,
                          const gchar*   title)
{
    const gchar* custom_title = midori_settings_get_custom_title (MIDORI_SETTINGS (browser->settings));
    if (custom_title && *custom_title)
        gtk_window_set_title (GTK_WINDOW (browser), custom_title);
    else if (katze_object_get_boolean (browser->settings, "enable-private-browsing"))
    {
        gchar* window_title = g_strdup_printf (_("%s (Private Browsing)"), title);
        gtk_window_set_title (GTK_WINDOW (browser), window_title);
        g_free (window_title);
    }
    else
        gtk_window_set_title (GTK_WINDOW (browser), title);
}

static void
midori_view_notify_title_cb (GtkWidget*     widget,
                             GParamSpec*    pspec,
                             MidoriBrowser* browser)
{
    MidoriView* view = MIDORI_VIEW (widget);
    if (widget == midori_browser_get_current_tab (browser))
    {
        midori_browser_set_title (browser, midori_view_get_display_title (view));
        g_object_notify (G_OBJECT (browser), "title");
    }
    midori_browser_step_history (browser, view);
}

static void
midori_browser_step_history (MidoriBrowser* browser,
                             MidoriView*    view)
{
    if (midori_view_get_load_status (view) != MIDORI_LOAD_COMMITTED)
        return;
    if (!browser->history_database || !browser->maximum_history_age)
        return;

    KatzeItem* proxy = midori_view_get_proxy_item (view);
    const gchar* proxy_uri = katze_item_get_uri (proxy);
    if (midori_uri_is_blank (proxy_uri))
        return;

    const gchar* history_step = katze_item_get_meta_string (proxy, "history-step");
    if (history_step == NULL)
    {
        GError* error = NULL;
        time_t now = time (NULL);
        katze_item_set_added (proxy, now);
        gint64 day = sokoke_time_t_to_julian (&now);
        midori_history_database_insert (browser->history_database,
            katze_item_get_uri (proxy),
            katze_item_get_name (proxy),
            katze_item_get_added (proxy), day, &error);
        if (error != NULL)
        {
            g_printerr (_("Failed to insert new history item: %s\n"), error->message);
            g_error_free (error);
            return;
        }
        katze_item_set_meta_string (proxy, "history-step", "update");
        /* FIXME: No signal for adding/ removing */
        katze_array_add_item (browser->history, proxy);
        katze_array_remove_item (browser->history, proxy);
    }
    else if (!strcmp (history_step, "update"))
    {
        if (proxy->name != NULL)
            midori_browser_update_history_title (browser, proxy);
    }
    else if (!strcmp (history_step, "ignore"))
    {
        /* This is set when restoring sessions */
    }
    else
        g_warning ("Unexpected history-step: %s", history_step);
}

static void
midori_view_notify_zoom_level_cb (GtkWidget*     view,
                                  GParamSpec*    pspec,
                                  MidoriBrowser* browser)
{
    if (view == midori_browser_get_current_tab (browser))
        _action_set_sensitive (browser, "ZoomNormal",
            midori_view_get_zoom_level (MIDORI_VIEW (view)) != 1.0f);
}

static void
midori_view_notify_statusbar_text_cb (GtkWidget*     view,
                                      GParamSpec*    pspec,
                                      MidoriBrowser* browser)
{
    gchar* text;

    if (view == midori_browser_get_current_tab (browser))
    {
        g_object_get (view, "statusbar-text", &text, NULL);
        _midori_browser_set_statusbar_text (browser, MIDORI_VIEW (view), text);
        g_free (text);
    }
}

static gboolean
midori_bookmark_folder_button_reach_parent (GtkTreeModel* model, GtkTreeIter *iter, gint64 parentid)
{
    do
    {
        gint64 id;

        gtk_tree_model_get (model, iter, 1, &id, -1);

        if (parentid == id)
            return TRUE;

        if (gtk_tree_model_iter_has_child (model, iter))
        {
            GtkTreeIter child;
            gtk_tree_model_iter_children (model, &child, iter);
            if (midori_bookmark_folder_button_reach_parent (model, &child, parentid))
            {
                *iter = child;
                return TRUE;
            }
        }
    }
    while (gtk_tree_model_iter_next (model, iter));

    return FALSE;
}

typedef struct _FolderEntry
{
    const gchar *title;
    gint64 id;
    gint64 parentid;
} FolderEntry;

static void
midori_bookmark_folder_free_folder_entry (FolderEntry* folder)
{
    g_free ((gpointer)folder->title);
}

static GtkWidget*
midori_bookmark_folder_button_new (MidoriBookmarksDb* array,
                                   gint64      selected_parentid)
{
    GtkTreeStore* model;
    GtkWidget* combo;
    GtkCellRenderer* renderer;
    guint n;
    sqlite3* db;
    sqlite3_stmt* statement;
    gint result;
    const gchar* sqlcmd = "SELECT title, id, parentid FROM bookmarks WHERE uri='' ORDER BY parentid, title ASC";
    gint64 current_parentid;
    GtkTreeIter tree_iter;
    GtkTreeIter stock_parent_iter;
    GtkTreeIter* parent_iter;
    GList *folders = NULL;

    db = g_object_get_data (G_OBJECT (array), "db");
    g_return_val_if_fail (db != NULL, NULL);

    /* folder combo box model content:
    ** 0: title
    ** 1: id
    */
    model = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_INT64);
    combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (model));

    /* setup combo layout
    ** 0: a folder icon
    ** 1: the folder name
     */

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (G_OBJECT (renderer),
        "stock-id", GTK_STOCK_DIRECTORY,
        "stock-size", GTK_ICON_SIZE_MENU,
        NULL);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);

    renderer = katze_cell_renderer_combobox_text_new ();
    g_object_set (G_OBJECT (renderer),
        "width-chars", 40,    /* FIXME: figure out a way to define an acceptable string length */
        "ellipsize", PANGO_ELLIPSIZE_END,
        "unfolded-text", _("Select [text]"),
        NULL);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "text", 0);

    /* read the folders list from the database */
    /* FIXME: this should be a service of midori/midori-bookmarks-db */

    if ((result = sqlite3_prepare_v2 (db, sqlcmd, -1, &statement, NULL)) == SQLITE_OK)
    {
        while ((result = sqlite3_step (statement)) == SQLITE_ROW)
        {
            FolderEntry* folder = g_new (FolderEntry, 1);

            folder->title = g_strdup ((const gchar*)sqlite3_column_text (statement, 0));
            folder->id = sqlite3_column_int64 (statement, 1);
            folder->parentid = sqlite3_column_int64 (statement, 2);

            folders = g_list_append (folders, folder);
        }

        sqlite3_clear_bindings (statement);
        sqlite3_reset (statement);
    }

    /* populate the combo box */
    /* FIXME: here we should have the root bookmark array's name and id, not hard encoded values */

    gtk_tree_store_insert_with_values (model, &tree_iter, NULL, G_MAXINT,
        0, _("Bookmarks"), 1, (gint64)-1, -1);
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &tree_iter);

    current_parentid = -1;
    parent_iter = NULL;
    n = 1;
    while (g_list_first (folders))
    {
        gboolean something_done = FALSE;
        GList* list_iter = g_list_first (folders);

        do
        {
            FolderEntry* folder = list_iter->data;
            const gchar* title = folder->title;
            gint64 id = folder->id;
            gint64 parentid = folder->parentid;

            if (parentid != current_parentid)  /* optimize case of sub-folders of the same parent */
            {
                if (!parentid)
                {
                    /* folder's parent is the stree store root */

                    current_parentid = -1;
                    parent_iter = NULL;
                }
                else if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &tree_iter))
                {
                    if (midori_bookmark_folder_button_reach_parent (
                            GTK_TREE_MODEL (model), &tree_iter, parentid))
                    {
                        /* folder's parent found in the tree store */

                        current_parentid = parentid;
                        stock_parent_iter = tree_iter;
                        parent_iter = &stock_parent_iter;
                    }
                    else
                    {
                        /* folder's parent not found, skip it */

                        list_iter = g_list_next (list_iter);
                        continue;
                    }
                }
                else
                    g_assert_not_reached ();
            }

            /* insert folder in the tree store and remove it from the folders list */

            gtk_tree_store_insert_with_values (model, &tree_iter, parent_iter, G_MAXINT,
                0, title, 1, id, -1);

            if (id == selected_parentid)
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &tree_iter);

            n++;

            something_done = TRUE;

            g_free ((gpointer)title);
            folders = g_list_delete_link (folders, list_iter);

            list_iter = g_list_first (folders);
        }
        while (list_iter);

        if (!something_done)  /* avoid infinite loop in case of orphan folders */
            break;
    }

    if (g_list_first (folders))
    {
        GList* iter;
        g_printerr ("midori_bookmark_folder_button_new: orphan folder(s) detected in bookmarks db\n");

        for (iter = g_list_first (folders) ; iter ; iter = g_list_next (iter))
        {
            FolderEntry* folder = iter->data;
            const gchar* title = folder->title;
            gint64 id = folder->id;
            gint64 parentid = folder->parentid;

            g_printerr ("  id=%" G_GINT64_FORMAT ", parentid=%" G_GINT64_FORMAT ", title=%s\n",
                id, parentid, title);
        }

        g_list_free_full (folders, (GDestroyNotify)midori_bookmark_folder_free_folder_entry);
    }

    if (n < 2)
        gtk_widget_set_sensitive (combo, FALSE);

    return combo;
}

static gint64
midori_bookmark_folder_button_get_active (GtkWidget* combo)
{
    gint64 id = -1;
    GtkTreeIter iter;

    g_return_val_if_fail (GTK_IS_COMBO_BOX (combo), 0);

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
    {
        GtkTreeModel* model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
        gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 1, &id, -1);
    }

    return id;
}

static void
midori_browser_edit_bookmark_title_changed_cb (GtkEntry*      entry,
                                               GtkDialog*     dialog)
{
    const gchar* title = gtk_entry_get_text (entry);
    gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_ACCEPT,
        title != NULL && title[0] != '\0');
}

static void
midori_browser_edit_bookmark_add_speed_dial_cb (GtkWidget* button,
                                                KatzeItem* bookmark)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (button);
    midori_browser_add_speed_dial (browser);
    GtkWidget* dialog = gtk_widget_get_toplevel (button);
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_DELETE_EVENT);
}

static void
midori_browser_edit_bookmark_create_launcher_cb (GtkWidget* button,
                                                 KatzeItem* bookmark)
{
    GtkAction* action = g_object_get_data (G_OBJECT (button), "midori-action");
    gtk_action_activate (action);
    GtkWidget* dialog = gtk_widget_get_toplevel (button);
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_DELETE_EVENT);
}

/* Private function, used by MidoriBookmarks and MidoriHistory */
/* static */ gboolean
midori_browser_edit_bookmark_dialog_new (MidoriBrowser* browser,
                                         KatzeItem*     bookmark_or_parent,
                                         gboolean       new_bookmark,
                                         gboolean       is_folder,
                                         GtkWidget*     proxy)
{
    KatzeItem*   bookmark = bookmark_or_parent;
    const gchar* title;
    GtkWidget* dialog;
    GtkWidget* content_area;
    GtkWidget* view;
    GtkWidget* vbox;
    GtkWidget* hbox;
    GtkWidget* label;
    const gchar* value;
    GtkWidget* entry_title;
    GtkWidget* entry_uri;
    GtkWidget* combo_folder;
    GtkWidget* check_toolbar;
    gboolean return_status = FALSE;

    if (is_folder)
        title = new_bookmark ? _("New Folder") : _("Edit Folder");
    else
        title = new_bookmark ? _("New Bookmark") : _("Edit Bookmark");
    #ifdef HAVE_GRANITE
    if (proxy != NULL)
    {
        /* FIXME: granite: should return GtkWidget* like GTK+ */
        dialog = (GtkWidget*)granite_widgets_pop_over_new ();
        granite_widgets_pop_over_move_to_widget (
            GRANITE_WIDGETS_POP_OVER (dialog), proxy, TRUE);
    }
    else
    #endif
    {
        dialog = gtk_dialog_new_with_buttons (title, GTK_WINDOW (browser),
            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR, NULL, NULL);
    }
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        new_bookmark ? GTK_STOCK_ADD : GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

    if (!is_folder)
        label = gtk_label_new (_("Type a name for this bookmark, and choose where to keep it."));
    else
        label = gtk_label_new (_("Type a name for this folder, and choose where to keep it."));

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 6);
    gtk_box_pack_start (GTK_BOX (content_area), vbox, FALSE, FALSE, 0);
    gtk_window_set_icon_name (GTK_WINDOW (dialog),
        new_bookmark ? GTK_STOCK_ADD : GTK_STOCK_REMOVE);

    if (new_bookmark)
    {
        view = midori_browser_get_current_tab (browser);
        if (is_folder)
        {
            bookmark = (KatzeItem*)katze_array_new (KATZE_TYPE_ARRAY);
            katze_item_set_name (bookmark,
                midori_view_get_display_title (MIDORI_VIEW (view)));
        }
        else
            bookmark = g_object_new (KATZE_TYPE_ITEM,
                "uri", midori_view_get_display_uri (MIDORI_VIEW (view)),
                "name", midori_view_get_display_title (MIDORI_VIEW (view)), NULL);
        katze_item_set_meta_integer (
            bookmark, "parentid",
            (!bookmark_or_parent
                ? 0
                : katze_item_get_meta_integer (bookmark_or_parent, "id")));
    }

    entry_title = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry_title), TRUE);
    value = katze_item_get_name (bookmark);
    gtk_entry_set_text (GTK_ENTRY (entry_title), katze_str_non_null (value));
    midori_browser_edit_bookmark_title_changed_cb (GTK_ENTRY (entry_title),
                                                   GTK_DIALOG (dialog));
    g_signal_connect (entry_title, "changed",
        G_CALLBACK (midori_browser_edit_bookmark_title_changed_cb), dialog);
    gtk_box_pack_start (GTK_BOX (vbox), entry_title, FALSE, FALSE, 0);

    entry_uri = NULL;
    if (!is_folder)
    {
        entry_uri = katze_uri_entry_new (
            gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT));
        gtk_entry_set_activates_default (GTK_ENTRY (entry_uri), TRUE);
        gtk_entry_set_text (GTK_ENTRY (entry_uri), katze_item_get_uri (bookmark));
        gtk_box_pack_start (GTK_BOX (vbox), entry_uri, FALSE, FALSE, 0);
    }

    combo_folder = midori_bookmark_folder_button_new (browser->bookmarks,
        katze_item_get_meta_integer (bookmark, "parentid"));
    gtk_box_pack_start (GTK_BOX (vbox), combo_folder, FALSE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    check_toolbar = gtk_check_button_new_with_mnemonic (_("Show in Bookmarks _Bar"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_toolbar),
        katze_item_get_meta_boolean (bookmark, "toolbar"));
    gtk_box_pack_start (GTK_BOX (hbox), check_toolbar, FALSE, FALSE, 0);

    if (new_bookmark && !is_folder)
    {
        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

        label = gtk_button_new_with_mnemonic (_("Add to _Speed Dial"));
        g_signal_connect (label, "clicked",
            G_CALLBACK (midori_browser_edit_bookmark_add_speed_dial_cb), bookmark);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

        /* FIXME: There's no API for extending the bookmark dialog */
        GtkAction* action = _action_by_name (browser, "CreateLauncher");
        if (action != NULL)
        {
            label = gtk_button_new_with_mnemonic (gtk_action_get_label (action));
            g_object_set_data (G_OBJECT (label), "midori-action", action);
            g_signal_connect (label, "clicked",
                G_CALLBACK (midori_browser_edit_bookmark_create_launcher_cb), bookmark);
            gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        }
    }

    gtk_widget_show_all (content_area);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    if (midori_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gint64 selected;

        katze_item_set_name (bookmark,
            gtk_entry_get_text (GTK_ENTRY (entry_title)));
        katze_item_set_meta_integer (bookmark, "toolbar",
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_toolbar)));
        if (!is_folder)
            katze_item_set_uri (bookmark,
                gtk_entry_get_text (GTK_ENTRY (entry_uri)));

        selected = midori_bookmark_folder_button_get_active (combo_folder);
        katze_item_set_meta_integer (bookmark, "parentid", selected);

        if (new_bookmark)
            midori_bookmarks_db_add_item (browser->bookmarks, bookmark);
        else
            midori_bookmarks_db_update_item (browser->bookmarks, bookmark);

        return_status = TRUE;
    }

    gtk_widget_destroy (dialog);
    return return_status;
}

static gboolean
midori_browser_prepare_download (MidoriBrowser*  browser,
                                 WebKitDownload* download,
                                 const gchar*    uri)

{
    if (!midori_download_has_enough_space (download, uri, FALSE))
        return FALSE;

#ifdef HAVE_WEBKIT2
    webkit_download_set_destination (download, uri);
#else
    webkit_download_set_destination_uri (download, uri);
#endif
    g_signal_emit (browser, signals[ADD_DOWNLOAD], 0, download);
    return TRUE;
}

#ifndef HAVE_WEBKIT2
static void
midori_browser_save_resources (GList*       resources,
                               const gchar* folder)
{
    GList* list;
    katze_mkdir_with_parents (folder, 0700);

    for (list = resources; list; list = g_list_next (list))
    {
        WebKitWebResource* resource = WEBKIT_WEB_RESOURCE (list->data);
        GString* data = webkit_web_resource_get_data (resource);

        /* Resource could be adblocked, skip it in that case */
        if (!g_strcmp0 (webkit_web_resource_get_uri (resource), "about:blank"))
            continue;

        gchar* sub_filename = midori_download_get_filename_suggestion_for_uri (
            webkit_web_resource_get_mime_type (resource),
            webkit_web_resource_get_uri (resource));
        gchar* sub_path = g_build_filename (folder, sub_filename, NULL);
        sub_path = midori_download_get_unique_filename (sub_path);
        if (data)
        {
            GError* error = NULL;
            if (!g_file_set_contents (sub_path, data->str, data->len, &error))
            {
                g_warning ("Failed to save %s: %s", sub_filename, error->message);
                g_error_free (error);
            }
        }
        else
            g_warning ("Skipping empty resource %s", sub_filename);
        g_free (sub_filename);
        g_free (sub_path);
    }
}
#endif

void
midori_browser_save_uri (MidoriBrowser* browser,
                         MidoriView*    view,
                         const gchar*   uri)
{
    static gchar* last_dir = NULL;
    GtkWidget* dialog;
    const gchar* title = midori_view_get_display_title (view);
    gchar* filename;

    dialog = (GtkWidget*)midori_file_chooser_dialog_new (_("Save file as"),
        GTK_WINDOW (browser), GTK_FILE_CHOOSER_ACTION_SAVE);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

    if (uri == NULL)
        uri = midori_view_get_display_uri (view);

    if (last_dir && *last_dir)
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), last_dir);
    else
    {
        gchar* dirname = midori_uri_get_folder (uri);
        if (dirname == NULL)
            dirname = katze_object_get_string (browser->settings, "download-folder");
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), dirname);
        g_free (dirname);
    }

#ifndef HAVE_WEBKIT2
    GList* resources = midori_view_get_resources (view);
    gboolean file_only = TRUE;
    GtkWidget* checkbox = NULL;

    if (resources != NULL && g_list_nth_data (resources, 1) != NULL)
    {
        file_only = FALSE;
        checkbox = gtk_check_button_new_with_mnemonic (_("Save associated _resources"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
        gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), checkbox);
    }

    if (!file_only && !g_str_equal (title, uri))
        filename = midori_download_clean_filename (title);
    else
    {
        gchar* mime_type = katze_object_get_object (view, "mime-type");
        filename = midori_download_get_filename_suggestion_for_uri (mime_type, uri);
        g_free (mime_type);
    }
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);
    g_free (filename);

    if (midori_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        if (checkbox != NULL)
            file_only = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));
        if (!file_only)
        {
            gchar* fullname = g_strconcat (filename, ".html", NULL);
            midori_view_save_source (view, uri, fullname, FALSE);
            g_free (fullname);
            midori_browser_save_resources (resources, filename);
        }
        else
            midori_view_save_source (view, uri, filename, FALSE);
        katze_assign (last_dir,
            gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog)));
    }
    g_list_foreach (resources, (GFunc)g_object_unref, NULL);
    g_list_free (resources);
#else
    filename = midori_download_clean_filename (title);
    gchar* suggested_filename = g_strconcat (filename, ".mht", NULL);
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), suggested_filename);

    g_free (filename);
    g_free (suggested_filename);
    if (midori_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        char *uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
        if (uri != NULL)
        {
            midori_view_save_source (view, uri, NULL, false);
            g_free (uri);
        }
        katze_assign (last_dir,
            gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog)));
    }
#endif
    gtk_widget_destroy (dialog);
}

static void
midori_browser_speed_dial_refresh_cb (MidoriSpeedDial* dial,
                                      MidoriBrowser*   browser)
{
    GList* tabs = midori_browser_get_tabs (browser);
    for (; tabs != NULL; tabs = g_list_next (tabs))
        if (!strcmp (midori_tab_get_uri (tabs->data), "about:dial"))
            midori_view_reload (tabs->data, FALSE);
    g_list_free (tabs);
}

static void
midori_browser_add_speed_dial (MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    midori_speed_dial_add (browser->dial,
        midori_view_get_display_uri (MIDORI_VIEW (view)),
        midori_view_get_display_title (MIDORI_VIEW (view)), NULL);
}

static gboolean
midori_browser_tab_leave_notify_event_cb (GtkWidget*        widget,
                                          GdkEventCrossing* event,
                                          MidoriBrowser*    browser)
{
    _midori_browser_set_statusbar_text (browser, MIDORI_VIEW (widget), NULL);
    return TRUE;
}

static void
midori_browser_view_copy_to_item_history (GtkWidget* view,
                                          KatzeItem* item)
{
#ifndef HAVE_WEBKIT2
    WebKitWebView* copy_from;
    WebKitWebBackForwardList* list_from;
    guint length_from;
    gint i;
    GPtrArray* history;

    copy_from = WEBKIT_WEB_VIEW (midori_view_get_web_view (MIDORI_VIEW (view)));
    list_from = webkit_web_view_get_back_forward_list (copy_from);
    length_from = webkit_web_back_forward_list_get_back_length (list_from);
    history = g_ptr_array_new ();

    for (i = -length_from; i <=  -1 ; i++)
    {
        WebKitWebHistoryItem* hist_item = webkit_web_back_forward_list_get_nth_item (list_from, i);
        if (hist_item == NULL)
            break;
        g_object_ref ((gpointer) hist_item);
        g_ptr_array_add (history, (gpointer) hist_item);
    }
    g_object_set_data (G_OBJECT (item), "tab-history", (gpointer) history);
#endif
}

static void
midori_view_destroy_cb (GtkWidget*     view,
                        MidoriBrowser* browser)
{
    if (browser->proxy_array)
    {
        KatzeItem* item = midori_view_get_proxy_item (MIDORI_VIEW (view));
        if (katze_array_get_item_index (browser->proxy_array, item) != -1
         && !midori_tab_is_blank (MIDORI_TAB (view)))
        {
            if (browser->trash)
            {
                midori_browser_view_copy_to_item_history (view,item);
                katze_array_add_item (browser->trash, item);
            }
            midori_browser_update_history (item, "website", "leave");
        }
        midori_browser_disconnect_tab (browser, MIDORI_VIEW (view));
        g_signal_emit (browser, signals[REMOVE_TAB], 0, view);
    }
}

static void
midori_view_attach_inspector_cb (GtkWidget*     view,
                                 GtkWidget*     inspector_view,
                                 MidoriBrowser* browser)
{
    GtkWidget* toplevel = gtk_widget_get_toplevel (inspector_view);
    GtkWidget* scrolled = gtk_widget_get_parent (browser->inspector_view);
    if (browser->inspector_view == inspector_view)
        return;

    gtk_widget_hide (toplevel);
    gtk_widget_destroy (browser->inspector_view);
    gtk_widget_reparent (inspector_view, scrolled);
    gtk_widget_show_all (browser->inspector);
    browser->inspector_view = inspector_view;
    gtk_widget_destroy (toplevel);
    if (!katze_object_get_boolean (browser->settings, "last-inspector-attached"))
        g_object_set (browser->settings, "last-inspector-attached", TRUE, NULL);
}

static void
midori_view_detach_inspector_cb (GtkWidget*     view,
                                 GtkWidget*     inspector_view,
                                 MidoriBrowser* browser)
{
    GtkWidget* scrolled = gtk_widget_get_parent (GTK_WIDGET (inspector_view));
    GtkWidget* paned = gtk_widget_get_parent (scrolled);
    browser->inspector_view = gtk_viewport_new (NULL, NULL);
    gtk_container_remove (GTK_CONTAINER (scrolled), GTK_WIDGET (inspector_view));
    gtk_container_add (GTK_CONTAINER (scrolled), browser->inspector_view);
    gtk_widget_hide (paned);
    if (katze_object_get_boolean (browser->settings, "last-inspector-attached"))
        g_object_set (browser->settings, "last-inspector-attached", FALSE, NULL);
}

static void
midori_browser_view_copy_history (GtkWidget* view_to,
                                  GtkWidget* view_from,
                                  gboolean   omit_last)
{
#ifndef HAVE_WEBKIT2
    WebKitWebView* copy_from;
    WebKitWebBackForwardList* list_from;
    WebKitWebView* copy_to;
    WebKitWebBackForwardList* list_to;
    guint length_from;
    gint i;

    copy_from = WEBKIT_WEB_VIEW (midori_view_get_web_view (MIDORI_VIEW (view_from)));
    list_from = webkit_web_view_get_back_forward_list (copy_from);
    copy_to = WEBKIT_WEB_VIEW (midori_view_get_web_view (MIDORI_VIEW (view_to)));
    list_to = webkit_web_view_get_back_forward_list (copy_to);
    length_from = webkit_web_back_forward_list_get_back_length (list_from);

    for (i = -length_from; i <= (omit_last ? -1 : 0); i++)
    {
        WebKitWebHistoryItem* item = webkit_web_back_forward_list_get_nth_item (list_from, i);
        if (item == NULL)
            break;
        webkit_web_back_forward_list_add_item (list_to, item);
    }
#endif
}


static void
midori_browser_view_copy_from_item_history (GtkWidget* view,
                                            KatzeItem* item)
{
#ifndef HAVE_WEBKIT2
    WebKitWebView* copy_to;
    WebKitWebBackForwardList* list_to;
    gint i;
    GPtrArray* list_from;

    copy_to = WEBKIT_WEB_VIEW (midori_view_get_web_view (MIDORI_VIEW (view)));
    list_to = webkit_web_view_get_back_forward_list (copy_to);

    if (item == NULL)
        return;
    list_from = g_object_get_data (G_OBJECT (item), "tab-history");
    if (list_from == NULL)
        return;

    for (i = 0; i < list_from->len; i++)
    {
        WebKitWebHistoryItem* hist_item = (WebKitWebHistoryItem*) g_ptr_array_index (list_from, i);
        if (hist_item == NULL)
            break;
        webkit_web_back_forward_list_add_item (list_to, hist_item);
    }
    g_ptr_array_unref (list_from);
#endif
}



static gboolean
midori_browser_notify_new_tab_timeout_cb (MidoriBrowser *browser)
{
    #ifndef G_OS_WIN32
    gtk_window_set_opacity (GTK_WINDOW (browser), 1);
    #endif
    return G_SOURCE_REMOVE;
}

static void
midori_browser_notify_new_tab (MidoriBrowser* browser)
{
    if (katze_object_get_boolean (browser->settings, "flash-window-on-new-bg-tabs"))
    {
        #ifndef G_OS_WIN32
        gtk_window_set_opacity (GTK_WINDOW (browser), 0.8);
        #endif
        midori_timeout_add (100,
            (GSourceFunc) midori_browser_notify_new_tab_timeout_cb, browser, NULL);
    }
}

static bool
midori_view_forward_external (GtkWidget*    view,
                              const gchar*  uri,
                              MidoriNewView where)
{
    if (midori_paths_get_runtime_mode () == MIDORI_RUNTIME_MODE_APP)
    {
        gboolean handled = FALSE;
        g_signal_emit_by_name (view, "open-uri", uri, &handled);
        return handled;
    }
    else if (midori_paths_get_runtime_mode () == MIDORI_RUNTIME_MODE_PRIVATE)
    {
        if (where == MIDORI_NEW_VIEW_WINDOW)
        {
            sokoke_spawn_app (uri, TRUE);
            return TRUE;
        }
    }
    return FALSE;
}

static void
midori_view_new_tab_cb (GtkWidget*     view,
                        const gchar*   uri,
                        gboolean       background,
                        MidoriBrowser* browser)
{
    if (midori_view_forward_external (view, uri, MIDORI_NEW_VIEW_TAB))
        return;

    GtkWidget* new_view = midori_browser_add_uri (browser, uri);
    if (view != NULL)
        midori_browser_view_copy_history (new_view, view, FALSE);

    if (!background)
        midori_browser_set_current_tab (browser, new_view);
    else
        midori_browser_notify_new_tab (browser);
}

static void
midori_view_new_window_cb (GtkWidget*     view,
                           const gchar*   uri,
                           MidoriBrowser* browser)
{
    if (midori_view_forward_external (
        view ? view : midori_browser_get_current_tab (browser),
        uri, MIDORI_NEW_VIEW_WINDOW))
        return;

    MidoriBrowser* new_browser;
    g_signal_emit (browser, signals[NEW_WINDOW], 0, NULL, &new_browser);
    g_assert (new_browser != NULL);
    midori_view_new_tab_cb (view, uri, FALSE, new_browser);
}

static void
_midori_browser_set_toolbar_items (MidoriBrowser* browser,
                                   const gchar*   items);

static void
midori_view_new_view_cb (GtkWidget*     view,
                         GtkWidget*     new_view,
                         MidoriNewView  where,
                         gboolean       user_initiated,
                         MidoriBrowser* browser)
{
    if (midori_tab_get_is_dialog (MIDORI_TAB (view)))
    {
        /* Dialog: URL, no toolbars, no tabs */
        MidoriBrowser* new_browser;
        g_signal_emit (browser, signals[NEW_WINDOW], 0, NULL, &new_browser);
        g_assert (new_browser != NULL);
        gtk_window_set_transient_for (GTK_WINDOW (new_browser), GTK_WINDOW (browser));
        gtk_window_set_destroy_with_parent (GTK_WINDOW (new_browser), TRUE);
        g_object_set (new_browser,
                      "show-tabs", FALSE,
                      NULL);
        sokoke_widget_set_visible (new_browser->menubar, FALSE);
        sokoke_widget_set_visible (new_browser->bookmarkbar, FALSE);
        sokoke_widget_set_visible (new_browser->statusbar, FALSE);
        _action_set_visible (new_browser, "CompactMenu", FALSE);
        _action_set_visible (new_browser, "Preferences", TRUE); //zgh
        _midori_browser_set_toolbar_items (new_browser, "Location");
        sokoke_widget_set_visible (new_browser->panel, FALSE);
        midori_browser_add_tab (new_browser, new_view);
        midori_browser_set_current_tab (new_browser, new_view);
        return;
    }

    if (midori_view_forward_external (new_view,
        katze_item_get_uri (midori_view_get_proxy_item (MIDORI_VIEW (new_view))),
        where))
        return;

    midori_browser_view_copy_history (new_view, view, TRUE);
    if (where == MIDORI_NEW_VIEW_WINDOW)
    {
        MidoriBrowser* new_browser;
        g_signal_emit (browser, signals[NEW_WINDOW], 0, NULL, &new_browser);
        g_assert (new_browser != NULL);
        midori_browser_add_tab (new_browser, new_view);
        midori_browser_set_current_tab (new_browser, new_view);

        // ZRL 以新窗口打开时主动设置新视图的uri，激活加载流程
        const gchar* dest_uri = g_object_get_data (G_OBJECT (new_view), "destination-uri");
        midori_view_set_uri(new_view, dest_uri);
    }
// ZRL TODO 暂时屏蔽,未来清楚其作用时再打开
#if 0
    else if (gtk_widget_get_parent (new_view) != browser->notebook)
    {
        midori_browser_add_tab (browser, new_view);
        if (where != MIDORI_NEW_VIEW_BACKGROUND)
            midori_browser_set_current_tab (browser, new_view);
    }
#endif
    else {
        midori_browser_notify_new_tab (browser);
        // ZRL 以新Tab打开时步骤：midori_browser_add_tab, midori_browser_set_current_tab, midori_view_set_uri
        const gchar* dest_uri = g_object_get_data (G_OBJECT (new_view), "destination-uri");
        midori_browser_add_tab (browser, new_view);
        if (where != MIDORI_NEW_VIEW_BACKGROUND)
            midori_browser_set_current_tab (browser, new_view);
        midori_view_set_uri(new_view, dest_uri);
    }

    if (!user_initiated)
    {
        GdkWindow* window = gtk_widget_get_window (GTK_WIDGET (browser));
        GdkWindowState state = gdk_window_get_state (window);
        if ((state | GDK_WINDOW_STATE_MAXIMIZED)
         || (state | GDK_WINDOW_STATE_FULLSCREEN))
        {
            if (where == MIDORI_NEW_VIEW_WINDOW)
                g_signal_emit (browser, signals[SEND_NOTIFICATION], 0,
                    _("New Window"), _("A new window has been opened"));
            else if (!browser->show_tabs)
                g_signal_emit (browser, signals[SEND_NOTIFICATION], 0,
                    _("New Tab"), _("A new tab has been opened"));
        }
    }
}

#ifdef HAVE_WEBKIT2
static void
midori_browser_close_tab_idle (GObject*      resource,
                               GAsyncResult* result,
                               gpointer      view)
{
    guchar* data = webkit_web_resource_get_data_finish (WEBKIT_WEB_RESOURCE (resource),
        result, NULL, NULL);
    if (data != NULL)
        return;
#else
static gboolean
midori_browser_close_tab_idle (gpointer view)
{
#endif
    MidoriBrowser* browser = midori_browser_get_for_widget (GTK_WIDGET (view));
    midori_browser_close_tab (browser, GTK_WIDGET (view));
#ifndef HAVE_WEBKIT2
    return G_SOURCE_REMOVE;
#endif
}

static gboolean
midori_view_download_requested_cb (GtkWidget*      view,
                                   WebKitDownload* download,
                                   MidoriBrowser*  browser)
{
    MidoriDownloadType type = midori_download_get_type (download);
    gboolean handled = TRUE;

    g_return_val_if_fail (MIDORI_IS_VIEW (view), FALSE);
    if (type == MIDORI_DOWNLOAD_CANCEL)
    {
        handled = FALSE;
    }
    #ifdef HAVE_WEBKIT2
    else if (!webkit_download_get_destination (download))
    #else
    else if (!webkit_download_get_destination_uri (download))
    #endif
    {
        if (type == MIDORI_DOWNLOAD_SAVE_AS)
        {
            static GtkWidget* dialog = NULL;
            gchar* filename;

            if (!dialog)
            {
                #ifdef HAVE_WEBKIT2
                const gchar* download_uri = webkit_uri_response_get_uri (
                    webkit_download_get_response (download));
                #else
                const gchar* download_uri = webkit_download_get_uri (download);
                #endif
                gchar* folder;
                dialog = (GtkWidget*)midori_file_chooser_dialog_new (_("Save file"),
                    GTK_WINDOW (browser), GTK_FILE_CHOOSER_ACTION_SAVE);
                gtk_file_chooser_set_do_overwrite_confirmation (
                    GTK_FILE_CHOOSER (dialog), TRUE);
                gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
                folder = midori_uri_get_folder (download_uri);
                if (folder == NULL)
                    folder = katze_object_get_string (browser->settings, "download-folder");
                gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), folder);
                g_free (folder);
                g_signal_connect (dialog, "destroy",
                                  G_CALLBACK (gtk_widget_destroyed), &dialog);
            }
            filename = midori_download_get_suggested_filename (download);
            gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);
            g_free (filename);

            if (midori_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
            {
                gtk_widget_hide (dialog);
                gchar* uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
                if (!midori_browser_prepare_download (browser, download, uri))
                {
                    g_free (uri);
                    return FALSE;
                }
                g_free (uri);
            }
            else
            {
                gtk_widget_hide (dialog);
                return FALSE;
            }
        }
        else
        {
            gchar* folder = type == MIDORI_DOWNLOAD_OPEN ? NULL
              : katze_object_get_string (browser->settings, "download-folder");
            gchar* destination_uri =
                midori_download_prepare_destination_uri (download, folder);
            midori_browser_prepare_download (browser, download, destination_uri);
            g_free (destination_uri);
        }
        #ifndef HAVE_WEBKIT2
        webkit_download_start (download);
        #endif
    }

    /* Close empty tabs due to download links with a target */
    if (midori_view_is_blank (MIDORI_VIEW (view)))
    {
        GtkWidget* web_view = midori_view_get_web_view (MIDORI_VIEW (view));
        #ifdef HAVE_WEBKIT2
        WebKitWebResource* resource = webkit_web_view_get_main_resource (WEBKIT_WEB_VIEW (web_view));
        webkit_web_resource_get_data (resource, NULL, midori_browser_close_tab_idle, view);
        #else
        WebKitWebFrame* web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (web_view));
        WebKitWebDataSource* datasource = webkit_web_frame_get_data_source (web_frame);
        if (webkit_web_data_source_get_data (datasource) == NULL)
            g_idle_add (midori_browser_close_tab_idle, view);
        #endif
    }
    return handled;
}

void download_created_destination_cb (WebKitDownload *download,
                                      gchar          *destination,
                                      gpointer        user_data){
    g_print("%s: destination %s",G_STRFUNC,destination);
}

static void
midori_view_search_text_cb (GtkWidget*     view,
                            gboolean       found,
                            gchar*         typing,
                            MidoriBrowser* browser)
{
    midori_findbar_search_text (MIDORI_FINDBAR (browser->find), view, found, typing);
}

gint
midori_browser_get_n_pages (MidoriBrowser* browser)
{
    return midori_notebook_get_count (MIDORI_NOTEBOOK (browser->notebook));
}

static void
_midori_browser_remove_tab (MidoriBrowser* browser,
                            GtkWidget*     widget)
{
    gtk_widget_destroy (widget);
}

static void
midori_browser_connect_tab (MidoriBrowser* browser,
                            GtkWidget*     view)
{
    KatzeItem* item = midori_view_get_proxy_item (MIDORI_VIEW (view));
    katze_array_add_item (browser->proxy_array, item);

    g_object_connect (view,
                      "signal::notify::icon",
                      midori_view_notify_icon_cb, browser,
                      "signal::notify::load-status",
                      midori_view_notify_load_status_cb, browser,
                      "signal::notify::progress",
                      midori_view_notify_progress_cb, browser,
                      "signal::notify::uri",
                      midori_view_notify_uri_cb, browser,
                      "signal::notify::title",
                      midori_view_notify_title_cb, browser,
                      "signal::notify::zoom-level",
                      midori_view_notify_zoom_level_cb, browser,
                      "signal::notify::statusbar-text",
                      midori_view_notify_statusbar_text_cb, browser,
                      "signal::attach-inspector",
                      midori_view_attach_inspector_cb, browser,
                      "signal::detach-inspector",
                      midori_view_detach_inspector_cb, browser,
                      "signal::new-tab",
                      midori_view_new_tab_cb, browser,
                      "signal::new-window",
                      midori_view_new_window_cb, browser,
                      "signal::new-view",
                      midori_view_new_view_cb, browser,
                      "signal-after::download-requested",
                      midori_view_download_requested_cb, browser,
                      "signal::search-text",
                      midori_view_search_text_cb, browser,
                      "signal::leave-notify-event",
                      midori_browser_tab_leave_notify_event_cb, browser,
                      "signal::destroy",
                      midori_view_destroy_cb, browser,
                      NULL);
}

static void
midori_browser_disconnect_tab (MidoriBrowser* browser,
                               MidoriView*    view)
{
    KatzeItem* item = midori_view_get_proxy_item (view);
    katze_array_remove_item (browser->proxy_array, item);

    /* We don't ever want to be in a situation with no tabs,
       so just create an empty one if the last one is closed.
       The only exception is when we are closing the window,
       which is indicated by the proxy array having been unset. */
    if (katze_array_is_empty (browser->proxy_array))
    {
        midori_browser_add_uri (browser, "about:new");
        midori_browser_set_current_page (browser, 0);
    }

    _midori_browser_update_actions (browser);

    g_object_disconnect (view,
                         "any_signal",
                         midori_view_notify_icon_cb, browser,
                         "any_signal",
                         midori_view_notify_load_status_cb, browser,
                         "any_signal",
                         midori_view_notify_progress_cb, browser,
                         "any_signal",
                         midori_view_notify_uri_cb, browser,
                         "any_signal",
                         midori_view_notify_title_cb, browser,
                         "any_signal",
                         midori_view_notify_zoom_level_cb, browser,
                         "any_signal",
                         midori_view_notify_statusbar_text_cb, browser,
                         "any_signal::attach-inspector",
                         midori_view_attach_inspector_cb, browser,
                         "any_signal::detach-inspector",
                         midori_view_detach_inspector_cb, browser,
                         "any_signal::new-tab",
                         midori_view_new_tab_cb, browser,
                         "any_signal::new-window",
                         midori_view_new_window_cb, browser,
                         "any_signal::new-view",
                         midori_view_new_view_cb, browser,
                         "any_signal::download-requested::after",
                         midori_view_download_requested_cb, browser,
                         "any_signal::search-text",
                         midori_view_search_text_cb, browser,
                         "any_signal::leave-notify-event",
                         midori_browser_tab_leave_notify_event_cb, browser,
                         NULL);
}

static void
_midori_browser_add_tab (MidoriBrowser* browser,
                         GtkWidget*     view)
{
    KatzeItem* item = midori_view_get_proxy_item (MIDORI_VIEW (view));
    guint n;

    midori_browser_connect_tab (browser, view);

    if (!katze_item_get_meta_boolean (item, "append") &&
        katze_object_get_boolean (browser->settings, "open-tabs-next-to-current"))
    {
        n = midori_browser_get_current_page (browser) + 1;
        katze_array_move_item (browser->proxy_array, item, n);
    }
    else
        n = -1;
    katze_item_set_meta_integer (item, "append", -1);

    midori_notebook_insert (MIDORI_NOTEBOOK (browser->notebook), MIDORI_TAB (view), n);

    _midori_browser_update_actions (browser);
}

static void
_midori_browser_quit (MidoriBrowser* browser)
{
    /* Nothing to do */
}

static void
_update_tooltip_if_changed (GtkAction* action,
                            const gchar* text)
{
    gchar *old;
    g_object_get (action, "tooltip", &old, NULL);
    if (g_strcmp0(old, text)) {
        g_object_set (action,
                      "tooltip", text, NULL);
    }
    g_free (old);
}

static void
_update_reload_tooltip (GtkWidget*   widget,
                        GdkEventKey* event,
                        gboolean released)
{
    MidoriBrowser* browser = MIDORI_BROWSER (widget);

    /* Update the reload/stop tooltip in case we are holding the hard refresh modifiers*/
    GtkAction *reload_stop = _action_by_name (browser, "ReloadStop");
    GtkAction *reload = _action_by_name (browser, "Reload");
    GdkModifierType mask;
    gdk_window_get_pointer (gtk_widget_get_window (widget), NULL, NULL, &mask);
    const gchar *target;

    if ( mask & GDK_SHIFT_MASK)
    {
        target = _("Reload page without caching");
    }
    else
    {
        target = _("Reload the current page");
    }
    _update_tooltip_if_changed (reload_stop, target);
    _update_tooltip_if_changed (reload, target);
}

static gboolean
midori_browser_key_release_event (GtkWidget*   widget,
                                  GdkEventKey* event)
{
    _update_reload_tooltip (widget, event, TRUE);
    return FALSE;
}

static gboolean
midori_browser_key_press_event (GtkWidget*   widget,
                                GdkEventKey* event)
{
    GtkWindow* window = GTK_WINDOW (widget);
    MidoriBrowser* browser = MIDORI_BROWSER (widget);
    GtkWidgetClass* widget_class;
    guint clean_state;

    _update_reload_tooltip(widget, event, FALSE);
    /* Interpret Ctrl(+Shift)+Tab as tab switching for compatibility */
    if (midori_browser_get_nth_tab (browser, 1) != NULL
     && event->keyval == GDK_KEY_Tab
     && (event->state & GDK_CONTROL_MASK))
    {
        midori_browser_activate_action (browser, "TabNext");
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_ISO_Left_Tab
     && (event->state & GDK_CONTROL_MASK)
     && (event->state & GDK_SHIFT_MASK))
    {
        midori_browser_activate_action (browser, "TabPrevious");
        return TRUE;
    }
    /* Interpret Ctrl+= as Zoom In for compatibility */
    else if ((event->keyval == GDK_KEY_KP_Equal || event->keyval == GDK_KEY_equal)
          && (event->state & GDK_CONTROL_MASK))
    {
        midori_browser_activate_action (browser, "ZoomIn");
        return TRUE;
    }
    /* Interpret F5 as reloading for compatibility */
    else if (event->keyval == GDK_KEY_F5)
    {
        midori_browser_activate_action (browser, "Reload");
        return TRUE;
    }

#ifndef HAVE_WEBKIT2
    GtkWidget* focus = gtk_window_get_focus (GTK_WINDOW (widget));
    if (focus == NULL)
        gtk_widget_grab_focus (midori_browser_get_current_tab (MIDORI_BROWSER (widget)));
    else if (G_OBJECT_TYPE (focus) == WEBKIT_TYPE_WEB_VIEW
          && event->keyval == GDK_KEY_space
          && (!(event->state & GDK_SHIFT_MASK))
          && !webkit_web_view_can_cut_clipboard (WEBKIT_WEB_VIEW (focus))
          && !webkit_web_view_can_paste_clipboard (WEBKIT_WEB_VIEW (focus)))
    {
        /* Space at the bottom of the page: Go to next page */
        MidoriView* view = midori_view_get_for_widget (focus);
        GtkScrolledWindow* scrolled = GTK_SCROLLED_WINDOW (gtk_widget_get_parent (focus));
        GtkAdjustment* vadjust = gtk_scrolled_window_get_vadjustment (scrolled);
        if (gtk_adjustment_get_value (vadjust)
         == (gtk_adjustment_get_upper (vadjust) - gtk_adjustment_get_page_size (vadjust)))
        {
            /* Duplicate here because the URI pointer might change */
            gchar* uri = g_strdup (midori_view_get_next_page (view));
            if (uri != NULL)
            {
                midori_view_set_uri (view, uri);
                g_free (uri);
                return TRUE;
            }
        }
    }
#endif

    if (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
        if (sokoke_window_activate_key (window, event))
            return TRUE;

    clean_state = event->state & gtk_accelerator_get_default_mod_mask();
    if (!clean_state && gtk_window_propagate_key_event (window, event))
        return TRUE;

    if (!(event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
        if (sokoke_window_activate_key (window, event))
            return TRUE;

    if (event->state && gtk_window_propagate_key_event (window, event))
        return TRUE;

    /* Interpret (Shift+)Backspace as going back (forward) for compatibility */
    if ((event->keyval == GDK_KEY_BackSpace)
     && (event->state & GDK_SHIFT_MASK))
    {
        midori_browser_activate_action (browser, "Forward");
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_BackSpace)
    {
        midori_browser_activate_action (browser, "Back");
        return TRUE;
    }

    widget_class = g_type_class_peek_static (g_type_parent (GTK_TYPE_WINDOW));
    return widget_class->key_press_event (widget, event);
}

static void
midori_browser_class_init (MidoriBrowserClass* class)
{
    GtkWidgetClass* gtkwidget_class;
    GObjectClass* gobject_class;
    GParamFlags flags;

    /**
     * MidoriBrowser::new-window:
     * @browser: the object on which the signal is emitted
     * @window: a new browser window, or %NULL
     *
     * Emitted when a new browser window was created.
     *
     * Note: Before 0.1.7 the second argument was an URI string.
     *
     * Note: Since 0.2.1 the return value is a #MidoriBrowser
     *
     * Return value: a new #MidoriBrowser
     */
    signals[NEW_WINDOW] = g_signal_new (
        "new-window",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        G_STRUCT_OFFSET (MidoriBrowserClass, new_window),
        0,
        NULL,
        midori_cclosure_marshal_OBJECT__OBJECT,
        MIDORI_TYPE_BROWSER, 1,
        MIDORI_TYPE_BROWSER);

    signals[ADD_TAB] = g_signal_new (
        "add-tab",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, add_tab),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GTK_TYPE_WIDGET);

    signals[REMOVE_TAB] = g_signal_new (
        "remove-tab",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, remove_tab),
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GTK_TYPE_WIDGET);

    /**
     * MidoriBrowser::move-tab:
     * @browser: the object on which the signal is emitted
     * @notebook: the notebook containing the tabs
     * @cur_pos: the current position of the tab
     * @new_pos: the new position of the tab
     *
     * Emitted when a tab is moved.
     *
     * Since: 0.3.3
     */
     signals[MOVE_TAB] = g_signal_new (
        "move-tab",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__OBJECT_INT_INT,
        G_TYPE_NONE, 3,
        GTK_TYPE_WIDGET, G_TYPE_INT, G_TYPE_INT);

    /**
     * MidoriBrowser::switch-tab:
     * @browser: the object on which the signal is emitted
     * @old_view: the previous tab
     * @new_view: the new tab
     *
     * Emitted when a tab is switched.
     * There's no guarantee what the current tab is.
     *
     * Since: 0.4.7
     */
     signals[SWITCH_TAB] = g_signal_new (
        "switch-tab",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__OBJECT_OBJECT,
        G_TYPE_NONE, 2,
        G_TYPE_OBJECT, G_TYPE_OBJECT);

    signals[ACTIVATE_ACTION] = g_signal_new (
        "activate-action",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, activate_action),
        0,
        NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    /**
     * MidoriBrowser::add-download:
     * @browser: the object on which the signal is emitted
     * @download: a new download
     *
     * Emitted when a new download was accepted and is
     * about to start. Download UI should hook up here.
     *
     * Emitting this signal manually is equal to a
     * user initiating and confirming a download
     *
     * Note: This requires WebKitGTK+ 1.1.3.
     *
     * Since: 0.1.5
     */
    signals[ADD_DOWNLOAD] = g_signal_new (
        "add-download",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        G_TYPE_OBJECT);

    /**
     * MidoriBrowser::send-notification:
     * @browser: the object on which the signal is emitted
     * @title: the title for the notification
     * @message: the message for the notification
     *
     * Emitted when a browser wants to display a notification message,
     * e.g. when a download has been completed or a new tab was opened.
     *
     * Since: 0.1.7
     */
    signals[SEND_NOTIFICATION] = g_signal_new (
        "send-notification",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        midori_cclosure_marshal_VOID__STRING_STRING,
        G_TYPE_NONE, 2,
        G_TYPE_STRING,
        G_TYPE_STRING);

    /**
     * MidoriBrowser::populate-tool-menu:
     * @browser: the object on which the signal is emitted
     * @menu: the #GtkMenu to populate
     *
     * Emitted when a Tool menu is displayed, such as the
     * toplevel Tools in the menubar or the compact menu.
     *
     * Since: 0.1.9
     */
    signals[POPULATE_TOOL_MENU] = g_signal_new (
        "populate-tool-menu",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GTK_TYPE_MENU);
    /**
     * MidoriBrowser::populate-toolbar-menu:
     * @browser: the object on which the signal is emitted
     * @menu: the #GtkMenu to populate
     *
     * Emitted when a toolbar menu is displayed on right-click.
     *
     * Since: 0.3.4
     */
    signals[POPULATE_TOOLBAR_MENU] = g_signal_new (
        "populate-toolbar-menu",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        GTK_TYPE_MENU);

    signals[QUIT] = g_signal_new (
        "quit",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET (MidoriBrowserClass, quit),
        0,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    /**
     * MidoriBrowser::show-preferences:
     * @browser: the object on which the signal is emitted
     * @preferences: the #KatzePreferences to populate
     *
     * Emitted when a preference dialogue displayed, to allow
     * adding of a new page, to be used sparingly.
     *
     * Since: 0.3.4
     */
    signals[SHOW_PREFERENCES] = g_signal_new (
        "show-preferences",
        G_TYPE_FROM_CLASS (class),
        (GSignalFlags)(G_SIGNAL_RUN_LAST),
        0,
        0,
        NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        KATZE_TYPE_PREFERENCES);

    class->add_tab = _midori_browser_add_tab;
    class->remove_tab = _midori_browser_remove_tab;
    class->activate_action = _midori_browser_activate_action;
    class->quit = _midori_browser_quit;

    gtkwidget_class = GTK_WIDGET_CLASS (class);
    gtkwidget_class->key_press_event = midori_browser_key_press_event;
    gtkwidget_class->key_release_event = midori_browser_key_release_event;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->dispose = midori_browser_dispose;
    gobject_class->finalize = midori_browser_finalize;
    gobject_class->set_property = midori_browser_set_property;
    gobject_class->get_property = midori_browser_get_property;

    flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS;

    g_object_class_install_property (gobject_class,
                                     PROP_MENUBAR,
                                     g_param_spec_object (
                                     "menubar",
                                     "Menubar",
                                     "The menubar",
                                     GTK_TYPE_MENU_BAR,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_NAVIGATIONBAR,
                                     g_param_spec_object (
                                     "navigationbar",
                                     "Navigationbar",
                                     "The navigationbar",
                                     GTK_TYPE_TOOLBAR,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_NOTEBOOK,
                                     g_param_spec_object (
                                     "notebook",
                                     "Notebook",
                                     "The notebook containing the views",
                                     GTK_TYPE_CONTAINER,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_PANEL,
                                     g_param_spec_object (
                                     "panel",
                                     "Panel",
                                     "The side panel embedded in the browser",
                                     MIDORI_TYPE_PANEL,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_URI,
                                     g_param_spec_string (
                                     "uri",
                                     "URI",
                                     "The current URI",
                                     "",
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_TAB,
                                     g_param_spec_object (
                                     "tab",
                                     "Tab",
                                     "The current tab",
                                     GTK_TYPE_WIDGET,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_LOAD_STATUS,
                                     g_param_spec_enum (
                                     "load-status",
                                     "Load Status",
                                     "The current load status",
                                     MIDORI_TYPE_LOAD_STATUS,
                                     MIDORI_LOAD_FINISHED,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriBrowser:statusbar:
    *
    * The widget representing the statusbar contents. This is
    * not an actual #GtkStatusbar but rather a #GtkBox.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_STATUSBAR,
                                     g_param_spec_object (
                                     "statusbar",
                                     "Statusbar",
                                     "The statusbar",
                                     GTK_TYPE_BOX,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriBrowser:statusbar-text:
    *
    * The text that is displayed in the statusbar.
    *
    * This value reflects changes to the text visible in the statusbar, such
    * as the uri of a hyperlink the mouse hovers over or the description of
    * a menuitem.
    *
    * Setting this value changes the displayed text until the next change.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_STATUSBAR_TEXT,
                                     g_param_spec_string (
                                     "statusbar-text",
                                     "Statusbar Text",
                                     "The text that is displayed in the statusbar",
                                     "",
                                     flags));

    /**
    * MidoriBrowser:settings:
    *
    * An associated settings instance that is shared among all web views.
    *
    * Setting this value is propagated to every present web view. Also
    * every newly created web view will use this instance automatically.
    *
    * If no settings are specified a default will be used.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SETTINGS,
                                     g_param_spec_object (
                                     "settings",
                                     "Settings",
                                     "The associated settings",
                                     MIDORI_TYPE_WEB_SETTINGS,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriBrowser:proxy-items:
    *
    * The open views, automatically updated, for session management.
    *
    * Since: 0.4.8
    */
    g_object_class_install_property (gobject_class,
                                     PROP_PROXY_ITEMS,
                                     g_param_spec_object (
                                     "proxy-items",
                                     "Proxy Items",
                                     "The open tabs as an array",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriBrowser:bookmarks:
    *
    * The bookmarks folder, containing all bookmarks.
    *
    * This is actually a reference to a bookmarks instance,
    * so if bookmarks should be used it must be initially set.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_BOOKMARKS,
                                     g_param_spec_object (
                                     "bookmarks",
                                     "Bookmarks",
                                     "The bookmarks folder, containing all bookmarks",
                                     TYPE_MIDORI_BOOKMARKS_DB,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriBrowser:trash:
    *
    * The trash, that collects all closed tabs and windows.
    *
    * This is actually a reference to a trash instance, so if a trash should
    * be used it must be initially set.
    *
    * Note: In the future the trash might collect other types of items.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_TRASH,
                                     g_param_spec_object (
                                     "trash",
                                     "Trash",
                                     "The trash, collecting recently closed tabs and windows",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriBrowser:search-engines:
    *
    * The list of search engines to be used for web search.
    *
    * This is actually a reference to a search engines instance,
    * so if search engines should be used it must be initially set.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_ENGINES,
                                     g_param_spec_object (
                                     "search-engines",
                                     "Search Engines",
                                     "The list of search engines to be used for web search",
                                     KATZE_TYPE_ARRAY,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
    * MidoriBrowser:history:
    *
    * The list of history items.
    *
    * This is actually a reference to a history instance,
    * so if history should be used it must be initially set.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_HISTORY,
                                     g_param_spec_object (
                                     "history",
                                     "History",
                                     "The list of history items",
                                     KATZE_TYPE_ARRAY,
                                     flags));

    /**
    * MidoriBrowser:speed-dial:
    *
    * The speed dial configuration file.
    *
    * Since: 0.3.4
    * Since 0.4.7 this is a Midori.SpeedDial instance.
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SPEED_DIAL,
                                     g_param_spec_object (
                                     "speed-dial",
                                     "Speeddial",
                                     "Speed dial",
                                     MIDORI_TYPE_SPEED_DIAL,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * MidoriBrowser:show-tabs:
     *
     * Whether or not to show tabs.
     *
     * If disabled, no tab labels are shown. This is intended for
     * extensions that want to provide alternative tab labels.
     *
     * Since 0.1.8
    */
    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_TABS,
                                     g_param_spec_boolean (
                                     "show-tabs",
                                     "Show Tabs",
                                     "Whether or not to show tabs",
                                     TRUE,
                                     flags));

    #if !GTK_CHECK_VERSION (3, 0, 0)
    /* Add 2px space between tool buttons */
    gtk_rc_parse_string (
        "style \"tool-button-style\"\n {\n"
        "GtkToolButton::icon-spacing = 2\n }\n"
        "widget \"MidoriBrowser.*.MidoriBookmarkbar.Gtk*ToolButton\" "
        "style \"tool-button-style\"\n"
        "widget \"MidoriBrowser.*.MidoriFindbar.Gtk*ToolButton\" "
        "style \"tool-button-style\"\n");
    #endif
}

static void
_action_window_new_activate (GtkAction*     action,
                             MidoriBrowser* browser)
{
    midori_view_new_window_cb (NULL, "about:home", browser);
}

static void
_action_tab_new_activate (GtkAction*     action,
                          MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_add_uri (browser, "about:new");
    midori_browser_set_current_tab (browser, view);
}

static void
_action_private_browsing_activate (GtkAction*     action,
                                   MidoriBrowser* browser)
{
    sokoke_spawn_app ("about:private", TRUE);
}

static void
_action_open_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    #if !GTK_CHECK_VERSION (3, 1, 10)
    static gchar* last_dir = NULL;
    gboolean folder_set = FALSE;
    #endif
    gchar* uri = NULL;
    GtkWidget* dialog;
    GtkWidget* view;

    dialog = (GtkWidget*)midori_file_chooser_dialog_new (_("Open file"),
        GTK_WINDOW (browser), GTK_FILE_CHOOSER_ACTION_OPEN);

     /* base the start folder on the current view's uri if it is local */
     view = midori_browser_get_current_tab (browser);
     if ((uri = (gchar*)midori_view_get_display_uri (MIDORI_VIEW (view))))
     {
         gchar* filename = g_filename_from_uri (uri, NULL, NULL);
         if (filename)
         {
             gchar* dirname = g_path_get_dirname (filename);
             if (dirname && g_file_test (dirname, G_FILE_TEST_IS_DIR))
             {
                 gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), dirname);
                 #if !GTK_CHECK_VERSION (3, 1, 10)
                 folder_set = TRUE;
                 #endif
             }

             g_free (dirname);
             g_free (filename);
         }
     }

     #if !GTK_CHECK_VERSION (3, 1, 10)
     if (!folder_set && last_dir && *last_dir)
         gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), last_dir);
     #endif

     if (midori_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
     {
         #if !GTK_CHECK_VERSION (3, 1, 10)
         gchar* folder;
         folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
         katze_assign (last_dir, folder);
         #endif
         uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
         midori_browser_set_current_uri (browser, uri);
         g_free (uri);

     }
    gtk_widget_destroy (dialog);
}

static void
_action_save_as_activate (GtkAction*     action,
                          MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    midori_browser_save_uri (browser, MIDORI_VIEW (view), NULL);
}

static void
_action_add_speed_dial_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    midori_browser_add_speed_dial (browser);
}

static void
midori_browser_subscribe_to_news_feed (MidoriBrowser* browser,
                                       const gchar*   uri)
{
    const gchar* news_aggregator = midori_settings_get_news_aggregator (MIDORI_SETTINGS (browser->settings));
    if (news_aggregator && *news_aggregator)
    {
        /* Thunderbird only accepts feed://, Liferea doesn't mind */
        gchar* feed = g_strdup (uri);
        if (g_str_has_prefix (feed, "http://"))
        {
            feed[0] = 'f';
            feed[1] = 'e';
            feed[2] = 'e';
            feed[3] = 'd';
        }
        /* Special-case Liferea because a helper script may be required */
        if (g_str_equal (news_aggregator, "liferea")
         && g_find_program_in_path ("liferea-add-feed"))
            sokoke_spawn_program ("liferea-add-feed", FALSE, feed, TRUE, FALSE);
        else
            sokoke_spawn_program (news_aggregator, TRUE, feed, TRUE, FALSE);
        g_free (feed);
    }
    else
    {
        gchar* description = g_strdup_printf ("%s\n\n%s", uri,
            _("To use the above URI open a news aggregator. "
            "There is usually a menu or button \"New Subscription\", "
            "\"New News Feed\" or similar.\n"
            "Alternatively go to Preferences, Applications in Midori, "
            "and select a News Aggregator. Next time you click the "
            "news feed icon, it will be added automatically."));
        sokoke_message_dialog (GTK_MESSAGE_INFO, _("New feed"), description, FALSE);
        g_free (description);
    }
}

static void
_action_add_news_feed_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    GtkWidget* view;
    const gchar* uri;

    if (!(view = midori_browser_get_current_tab (browser)))
        return;
    if (!(uri = g_object_get_data (G_OBJECT (view), "news-feeds")))
        return;

    midori_browser_subscribe_to_news_feed (browser, uri);
}

static void
_action_compact_add_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    GtkWidget* dialog;
    GtkBox* box;
    const gchar* actions[] = { "BookmarkAdd", "AddSpeedDial", "AddNewsFeed" };
    guint i;

    dialog = g_object_new (GTK_TYPE_DIALOG,
        "transient-for", browser,
        "title", _("Add a new bookmark"), NULL);
    box = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));

    for (i = 0; i < G_N_ELEMENTS (actions); i++)
    {
        gchar* label;
        GtkWidget* button;

        action = _action_by_name (browser, actions[i]);
        label = katze_object_get_string (action, "label");
        button = gtk_button_new_with_mnemonic (label);
        g_free (label);
        gtk_widget_set_name (button, "GtkButton-thumb");
        gtk_box_pack_start (box, button, TRUE, TRUE, 4);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action);
        g_signal_connect_swapped (button, "clicked",
                                  G_CALLBACK (gtk_widget_destroy), dialog);
    }

    gtk_widget_show (dialog);
    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy), dialog);
}

static void
_action_tab_close_activate (GtkAction*     action,
                            MidoriBrowser* browser)
{
    GtkWidget* widget = midori_browser_get_current_tab (browser);
    midori_browser_close_tab (browser, widget);
}

static void
_action_window_close_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    gboolean val = FALSE;
    GdkEvent* event = gtk_get_current_event();
    g_signal_emit_by_name (G_OBJECT (browser), "delete-event", event, &val);
    gdk_event_free (event);

    if (!val)
        gtk_widget_destroy (GTK_WIDGET (browser));
}

static void
_action_mail_to_activate (GtkAction*     action,
                          MidoriBrowser* browser)
{
    MidoriView* view = MIDORI_VIEW (midori_browser_get_current_tab (browser));
    gchar* uri = g_uri_escape_string (midori_view_get_display_uri (view), NULL, TRUE);
    gchar* title = g_uri_escape_string (midori_view_get_display_title (view), NULL, TRUE);
    gchar* mailto = g_strconcat ("mailto:?cc=&bcc=&subject=", title, "&body=", uri, NULL);
    gboolean handled = FALSE;
    g_signal_emit_by_name (view, "open-uri", mailto, &handled);
    g_free (mailto);
    g_free (title);
    g_free (uri);
}

static void
_action_print_activate (GtkAction*     action,
                        MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);

    midori_view_print (MIDORI_VIEW (view));
}

static void
_action_quit_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    g_signal_emit (browser, signals[QUIT], 0);
}

static void
_action_edit_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    gboolean can_undo = FALSE, can_redo = FALSE;
    gboolean can_cut = FALSE, can_copy = FALSE, can_paste = FALSE;
    gboolean has_selection, can_select_all = FALSE;

    if (WEBKIT_IS_WEB_VIEW (widget))
    {
        midori_tab_update_actions (MIDORI_TAB (midori_browser_get_current_tab (browser)), browser->action_group, NULL, NULL);
        return;
    }
    else if (GTK_IS_EDITABLE (widget))
    {
        GtkEditable* editable = GTK_EDITABLE (widget);
        has_selection = gtk_editable_get_selection_bounds (editable, NULL, NULL);
        can_cut = has_selection && gtk_editable_get_editable (editable);
        can_copy = has_selection;
        can_paste = gtk_editable_get_editable (editable);
        can_select_all = TRUE;
    }
    else if (GTK_IS_TEXT_VIEW (widget))
    {
        GtkTextView* text_view = GTK_TEXT_VIEW (widget);
        GtkTextBuffer* buffer = gtk_text_view_get_buffer (text_view);
        has_selection = gtk_text_buffer_get_has_selection (buffer);
        can_cut = gtk_text_view_get_editable (text_view);
        can_copy = has_selection;
        can_paste = gtk_text_view_get_editable (text_view) && has_selection;
        can_select_all = TRUE;
    }
#if 0 //zgh
    _action_set_sensitive (browser, "Undo", can_undo);
    _action_set_sensitive (browser, "Redo", can_redo);
#endif
    _action_set_sensitive (browser, "Cut", can_cut);
    _action_set_sensitive (browser, "Copy", can_copy);
    _action_set_sensitive (browser, "Paste", can_paste);
    _action_set_sensitive (browser, "Delete", can_cut);
    _action_set_sensitive (browser, "SelectAll", can_select_all);
}

static void
_action_undo_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (WEBKIT_IS_WEB_VIEW (widget))
#ifdef HAVE_WEBKIT2
        webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (widget), WEBKIT_EDITING_COMMAND_UNDO);
#else
        webkit_web_view_undo (WEBKIT_WEB_VIEW (widget));
#endif
}

static void
_action_redo_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (WEBKIT_IS_WEB_VIEW (widget))
#ifdef HAVE_WEBKIT2
        webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (widget), WEBKIT_EDITING_COMMAND_REDO);
#else
        webkit_web_view_redo (WEBKIT_WEB_VIEW (widget));
#endif
}

static void
_action_cut_activate (GtkAction*     action,
                      MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget) && g_signal_lookup ("cut-clipboard", G_OBJECT_TYPE (widget)))
        g_signal_emit_by_name (widget, "cut-clipboard");
#ifdef HAVE_WEBKIT2
    else if (WEBKIT_IS_WEB_VIEW (widget))
        webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (widget), WEBKIT_EDITING_COMMAND_CUT);
#endif
}

static void
_action_copy_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget) && g_signal_lookup ("copy-clipboard", G_OBJECT_TYPE (widget)))
        g_signal_emit_by_name (widget, "copy-clipboard");
#ifdef HAVE_WEBKIT2
    else if (WEBKIT_IS_WEB_VIEW (widget))
        webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (widget), WEBKIT_EDITING_COMMAND_COPY);
#endif
}

static void
_action_paste_activate (GtkAction*     action,
                        MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget) && g_signal_lookup ("paste-clipboard", G_OBJECT_TYPE (widget)))
        g_signal_emit_by_name (widget, "paste-clipboard");
#ifdef HAVE_WEBKIT2
    else if (WEBKIT_IS_WEB_VIEW (widget))
        webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (widget), WEBKIT_EDITING_COMMAND_PASTE);
#endif
}

static void
_action_delete_activate (GtkAction*     action,
                         MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget))
    {
        if (GTK_IS_EDITABLE (widget))
            gtk_editable_delete_selection (GTK_EDITABLE (widget));
#ifndef HAVE_WEBKIT2
        else if (WEBKIT_IS_WEB_VIEW (widget))
            webkit_web_view_delete_selection (WEBKIT_WEB_VIEW (widget));
#endif
        else if (GTK_IS_TEXT_VIEW (widget))
            gtk_text_buffer_delete_selection (
                gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget)), TRUE, FALSE);
    }
}

static void
_action_select_all_activate (GtkAction*     action,
                             MidoriBrowser* browser)
{
    GtkWidget* widget = gtk_window_get_focus (GTK_WINDOW (browser));
    if (G_LIKELY (widget))
    {
        if (GTK_IS_EDITABLE (widget))
            gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
#ifdef HAVE_WEBKIT2
        else if (WEBKIT_IS_WEB_VIEW (widget))
            webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (widget), WEBKIT_EDITING_COMMAND_SELECT_ALL);
#endif
        else if (g_signal_lookup ("select-all", G_OBJECT_TYPE (widget)))
        {
            if (GTK_IS_TEXT_VIEW (widget))
                g_signal_emit_by_name (widget, "select-all", TRUE);
            else if (GTK_IS_TREE_VIEW (widget))
            {
                gboolean dummy;
                g_signal_emit_by_name (widget, "select-all", &dummy);
            }
            else
                g_signal_emit_by_name (widget, "select-all");
        }
    }
}

static void
_action_find_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    midori_findbar_invoke (MIDORI_FINDBAR (browser->find),
        midori_view_get_selected_text (MIDORI_VIEW (view)));
}

static void
_action_find_next_activate (GtkAction*     action,
                            MidoriBrowser* browser)
{
    midori_findbar_continue (MIDORI_FINDBAR (browser->find), TRUE);
}

static void
_action_find_previous_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    midori_findbar_continue (MIDORI_FINDBAR (browser->find), FALSE);
}

static void
midori_browser_navigationbar_notify_style_cb (GObject*       object,
                                              GParamSpec*    arg1,
                                              MidoriBrowser* browser)
{
    MidoriToolbarStyle toolbar_style;

    g_object_get (browser->settings, "toolbar-style", &toolbar_style, NULL);
    _midori_browser_set_toolbar_style (browser, toolbar_style);
}

static gboolean
midori_browser_toolbar_item_button_press_event_cb (GtkWidget*      toolitem,
                                                   GdkEventButton* event,
                                                   MidoriBrowser*  browser);

/**
 * midori_browser_get_toolbar_actions:
 *
 * Retrieves a list of actions which are suitable for use in a toolbar.
 *
 * Return value: a NULL-terminated array of strings with actions
 *
 * Since: 0.1.8
 **/
const gchar**
midori_browser_get_toolbar_actions (MidoriBrowser* browser)
{
    static const gchar* actions[] = {
            "WindowNew", "TabNew", "Open", "SaveAs", "Print", "Find",
            "Fullscreen", "Preferences", "Window", "Bookmarks",
            "ReloadStop", "ZoomIn", "TabClose", "NextForward", "Location",
            "ZoomOut", "Separator", "Back", "Forward", "Homepage",
            "Panel", "Trash", "Search", "BookmarkAdd", "Previous", "Next", NULL };

    return actions;
}

/**
 * midori_browser_get_settings:
 *
 * Retrieves the settings instance of the browser.
 *
 * Return value: a #MidoriWebSettings instance
 *
 * Since: 0.2.5
 **/
MidoriWebSettings*
midori_browser_get_settings (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    return browser->settings;
}

static gboolean
midori_browser_toolbar_popup_context_menu_cb (GtkWidget*     widget,
                                              gint           x,
                                              gint           y,
                                              gint           button,
                                              MidoriBrowser* browser)
{
    MidoriContextAction* menu = midori_context_action_new ("ToolbarContextMenu", NULL, NULL, NULL);
    midori_context_action_add_action_group (menu, browser->action_group);
    midori_context_action_add_by_name (menu, "Menubar");
//zgh    midori_context_action_add_by_name (menu, "Navigationbar");
    midori_context_action_add_by_name (menu, "Bookmarkbar");
    midori_context_action_add_by_name (menu, "Statusbar");

    GtkMenu* context_menu = midori_context_action_create_menu (menu, NULL, FALSE);
    g_signal_emit (browser, signals[POPULATE_TOOLBAR_MENU], 0, context_menu);
    katze_widget_popup (widget, GTK_MENU (context_menu), NULL,
        button == -1 ? KATZE_MENU_POSITION_LEFT : KATZE_MENU_POSITION_CURSOR);
    return TRUE;
}

static void
midori_browser_bookmark_popup (GtkWidget*      proxy,
                               GdkEventButton* event,
                               KatzeItem*      item,
                               MidoriBrowser*  browser);

static gboolean
midori_bookmarkbar_activate_item (GtkAction* action,
                                  KatzeItem* item,
                                  MidoriBrowser* browser)
{
    return midori_browser_open_bookmark (browser, item);;
}

static gboolean
midori_bookmarkbar_activate_item_alt (GtkAction*      action,
                                      KatzeItem*      item,
                                      GtkWidget*      proxy,
                                      GdkEventButton* event,
                                      MidoriBrowser*  browser)
{
    g_assert (event);

    if (MIDORI_EVENT_NEW_TAB (event))
    {
        GtkWidget* view = midori_browser_add_item (browser, item);
        midori_browser_set_current_tab_smartly (browser, view);
    }
    else if (MIDORI_EVENT_CONTEXT_MENU (event))
    {
        midori_browser_bookmark_popup (proxy, NULL, item, browser);
    }
    else if (event->button == 1)
    {
        midori_bookmarkbar_activate_item (action, item, browser);
    }

    return TRUE;
}

static void
_action_trash_populate_popup (GtkAction*     action,
                              GtkMenu*       menu,
                              MidoriBrowser* browser)
{
    GtkWidget* menuitem;

    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    menuitem = gtk_action_create_menu_item (
        _action_by_name (browser, "TrashEmpty"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
}

static GtkWidget*
midori_browser_restore_tab (MidoriBrowser* browser,
                            KatzeItem*     item)
{
    GtkWidget* view;
    g_object_ref (item);
    katze_array_remove_item (browser->trash, item);
    view = midori_browser_add_item (browser, item);
    midori_browser_view_copy_from_item_history (view,item);

    g_object_unref (item);
    return view;
}

static gboolean
_action_trash_activate_item_alt (GtkAction*      action,
                                 KatzeItem*      item,
                                 GtkWidget*      proxy,
                                 GdkEventButton* event,
                                 MidoriBrowser*  browser)
{
    g_assert (event);

    if (MIDORI_EVENT_NEW_TAB (event))
    {
        midori_browser_set_current_tab_smartly (browser,
            midori_browser_restore_tab (browser, item));
    }
    else if (event->button == 1)
    {
        midori_browser_set_current_tab (browser,
            midori_browser_restore_tab (browser, item));
    }

    return TRUE;
}

/* static */ gboolean
midori_browser_open_bookmark (MidoriBrowser* browser,
                              KatzeItem*     item)
{
    const gchar* uri = katze_item_get_uri (item);
    gchar* uri_fixed;

    if (!(uri && *uri))
        return FALSE;

    /* Imported bookmarks may lack a protocol */
    uri_fixed = sokoke_magic_uri (uri, TRUE, FALSE);
    if (!uri_fixed)
        uri_fixed = g_strdup (uri);

    if (katze_item_get_meta_boolean (item, "app"))
        sokoke_spawn_app (uri_fixed, FALSE);
    else
    {
        midori_browser_set_current_uri (browser, uri_fixed);
        gtk_widget_grab_focus (midori_browser_get_current_tab (browser));
    }
    g_free (uri_fixed);
    return TRUE;
}

static void
_action_tools_populate_popup (GtkAction*     action,
                              GtkMenu*       default_menu,
                              MidoriBrowser* browser)
{
    MidoriContextAction* menu = midori_context_action_new ("ToolsMenu", NULL, NULL, NULL);
    midori_context_action_add_action_group (menu, browser->action_group);
//    midori_context_action_add_by_name (menu, "ManageSearchEngines");  //zgh
    midori_context_action_add_by_name (menu, "ClearPrivateData");
    midori_context_action_add_by_name (menu, "InspectPage");
    g_signal_emit (browser, signals[POPULATE_TOOL_MENU], 0, default_menu);
//    midori_context_action_add (menu, NULL);   //zgh
#if 0 //zgh
    gsize j = 0;
    GtkWidget* widget;
    while ((widget = midori_panel_get_nth_page (MIDORI_PANEL (browser->panel), j++)))
        midori_context_action_add (menu, g_object_get_data (G_OBJECT (widget), "midori-panel-action"));
#endif
    #ifdef G_OS_WIN32
    midori_context_action_add (menu, NULL);
    midori_context_action_add_by_name (menu, "Preferences");
    #endif
    midori_context_action_create_menu (menu, default_menu, TRUE);
}

static gboolean
_action_bookmarks_populate_folder (GtkAction*     action,
                                   GtkMenuShell*  menu,
                                   KatzeArray*    folder,
                                   MidoriBrowser* browser)
{
    if (browser->bookmarks == NULL)
        return FALSE;

    midori_bookmarks_db_populate_folder (browser->bookmarks, folder);

    /* Clear items from dummy array here */
    gtk_container_foreach (GTK_CONTAINER (menu),
        (GtkCallback)(gtk_widget_destroy), NULL);

    /* "Add Bookmark", "Import Bookmarks", and "Export Bookmarks" at the top */
    if (folder == KATZE_ARRAY (browser->bookmarks))
    {
        GtkWidget* menuitem;
        menuitem = gtk_action_create_menu_item (_action_by_name (browser, "BookmarkAdd"));
        gtk_menu_shell_append (menu, menuitem);
        gtk_widget_show (menuitem);
        menuitem = gtk_action_create_menu_item (_action_by_name (browser, "BookmarksImport"));
        gtk_menu_shell_append (menu, menuitem);
        gtk_widget_show (menuitem);
        menuitem = gtk_action_create_menu_item (_action_by_name (browser, "BookmarksExport"));
        gtk_menu_shell_append (menu, menuitem);
        gtk_widget_show (menuitem);
        #if 0 //zgh
        menuitem = gtk_separator_menu_item_new ();
        gtk_menu_shell_append (menu, menuitem);
        gtk_widget_show (menuitem);
        #endif
    }
    
    if (!katze_array_is_empty (folder))  //zgh
    {
        GtkWidget* menuitem = gtk_separator_menu_item_new ();
        gtk_menu_shell_append (menu, menuitem);
        gtk_widget_show (menuitem);
        
        katze_array_action_generate_menu (KATZE_ARRAY_ACTION (action), folder,
                                      menu, GTK_WIDGET (browser));
    }

#if 0 //zgh
    if (katze_array_is_empty (folder))
    {
        GtkWidget* menuitem = gtk_image_menu_item_new_with_label (_("Empty"));
        gtk_widget_set_sensitive (menuitem, FALSE);
        gtk_menu_shell_append (menu, menuitem);
        gtk_widget_show (menuitem);
        return TRUE;
    }

    katze_array_action_generate_menu (KATZE_ARRAY_ACTION (action), folder,
                                      menu, GTK_WIDGET (browser));
#endif
    return TRUE;
}

static void
_action_window_populate_popup (GtkAction*     action,
                               GtkMenu*       default_menu,
                               MidoriBrowser* browser)
{
    MidoriContextAction* menu = midori_context_action_new ("WindowMenu", NULL, NULL, NULL);
    midori_context_action_add_action_group (menu, browser->action_group);
    midori_context_action_add (menu, NULL);
    midori_context_action_add_by_name (menu, "LastSession");
    midori_context_action_add_by_name (menu, "TabCurrent");
    midori_context_action_add_by_name (menu, "NextView");
    midori_context_action_add_by_name (menu, "TabNext");
    midori_context_action_add_by_name (menu, "TabPrevious");
    midori_context_action_create_menu (menu, default_menu, TRUE);
}

static void
_action_window_activate_item_alt (GtkAction*      action,
                                  KatzeItem*      item,
                                  GtkWidget*      proxy,
                                  GdkEventButton* event,
                                  MidoriBrowser*  browser)
{
    midori_browser_set_current_item (browser, item);
}

static void
_action_compact_menu_populate_popup (GtkAction*     action,
                                     GtkMenu*       default_menu,
                                     MidoriBrowser* browser)
{
    MidoriContextAction* menu = midori_context_action_new ("CompactMenu", NULL, NULL, NULL);
    midori_context_action_add_action_group (menu, browser->action_group);
    midori_context_action_add_by_name (menu, "WindowNew");
    midori_context_action_add_by_name (menu, "PrivateBrowsing");
    midori_context_action_add (menu, NULL);
    midori_context_action_add_by_name (menu, "Find");
    midori_context_action_add_by_name (menu, "Print");
    midori_context_action_add_by_name (menu, "Fullscreen");
    midori_context_action_add_by_name (menu, "MailTo");
    midori_context_action_add (menu, NULL);
    midori_context_action_add_by_name (menu, "Homepage");   //zgh
    gsize j = 0;
    GtkWidget* widget;
    while ((widget = midori_panel_get_nth_page (MIDORI_PANEL (browser->panel), j++)))
        midori_context_action_add (menu, g_object_get_data (G_OBJECT (widget), "midori-panel-action"));
    midori_context_action_add (menu, NULL);
    midori_context_action_add_by_name (menu, "BookmarkAdd");
    midori_context_action_add_by_name (menu, "BookmarksImport");
    midori_context_action_add_by_name (menu, "BookmarksExport");
    midori_context_action_add_by_name (menu, "ClearPrivateData");
    g_signal_emit (browser, signals[POPULATE_TOOL_MENU], 0, default_menu);
    midori_context_action_add (menu, NULL);
    #ifndef HAVE_GRANITE
    midori_context_action_add_by_name (menu, "HelpFAQ");
    midori_context_action_add_by_name (menu, "HelpBugs");
    #endif
    midori_context_action_add_by_name (menu, "Preferences");
    midori_context_action_add_by_name (menu, "About");
    midori_context_action_create_menu (menu, default_menu, FALSE);
}

static void
midori_preferences_response_help_cb (GtkWidget*     preferences,
                                     gint           response,
                                     MidoriBrowser* browser)
{
    if (response == GTK_RESPONSE_HELP)
        midori_browser_activate_action (browser, "HelpFAQ");
}

static void
_action_preferences_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
#if 0
    static GtkWidget* dialog = NULL;

    if (!dialog)
    {
        dialog = midori_preferences_new (GTK_WINDOW (browser), browser->settings);
        g_signal_emit (browser, signals[SHOW_PREFERENCES], 0, dialog);
        g_signal_connect (dialog, "response",
            G_CALLBACK (midori_preferences_response_help_cb), browser);
        /**/   g_signal_connect (dialog, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &dialog);
   
        gtk_widget_show (dialog);
    }
    else
        gtk_window_present (GTK_WINDOW (dialog));
#else

static GtkWidget* dialog = NULL;

if (!dialog)
{
	dialog = browser_settings_window_new(browser->settings); 
	g_signal_connect (dialog, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &dialog);

	gtk_widget_show (dialog);
}
else
	gtk_window_present (GTK_WINDOW (dialog));


#endif
g_print("Hello world preferences\n");
}

static gboolean
midori_browser_has_native_menubar (void)
{
    static const gchar* ubuntu_menuproxy = NULL;
    if (ubuntu_menuproxy == NULL)
        ubuntu_menuproxy = g_getenv ("UBUNTU_MENUPROXY");
    /*
     * Values when the global menu is enabled
     * UBUNTU_MENUPROXY=libappmenu.so
     * UBUNTU_MENUPROXY=1
     * The official way to disable the menu is
     * UBUNTU_MENUPROXY=
     */
    return ubuntu_menuproxy && (strstr (ubuntu_menuproxy, ".so") || !strcmp (ubuntu_menuproxy, "1"));
}

static void
_action_menubar_activate (GtkToggleAction* menubar_action,
                          MidoriBrowser*   browser)
{
    gboolean active = gtk_toggle_action_get_active (menubar_action);
    GtkAction* menu_action = _action_by_name (browser, "CompactMenu");
    GString* toolbar_items;
    GList* children;
    gchar* items;

    if (midori_browser_has_native_menubar ())
        active = FALSE;

    toolbar_items = g_string_new (NULL);
    children = gtk_container_get_children (GTK_CONTAINER (browser->navigationbar));
    for (; children != NULL; children = g_list_next (children))
    {
        GtkAction* action = gtk_activatable_get_related_action (
            GTK_ACTIVATABLE (children->data));
        if (!action)
            continue;
        if (action == ((GtkAction*)menu_action))
        {
            if (active)
            {
                gtk_container_remove (GTK_CONTAINER (browser->navigationbar),
                                      GTK_WIDGET (children->data));
            }
            continue;
        }
        else if (MIDORI_IS_PANED_ACTION (action))
        {
            MidoriPanedAction* paned_action = MIDORI_PANED_ACTION (action);
            g_string_append_printf (toolbar_items, "%s,%s",
                midori_paned_action_get_child1_name (paned_action),
                midori_paned_action_get_child2_name (paned_action));
        }
        else
            g_string_append (toolbar_items, gtk_action_get_name (action));
        g_string_append_c (toolbar_items, ',');
    }
    g_list_free (children);

    if (katze_object_get_boolean (browser->settings, "show-menubar") != active)
        g_object_set (browser->settings, "show-menubar", active, NULL);

    items = g_string_free (toolbar_items, FALSE);
    g_object_set (browser->settings, "toolbar-items", items, NULL);
    g_free (items);

    sokoke_widget_set_visible (browser->menubar, active);
    _action_set_visible(browser, "Preferences", active);    //zgh
    g_object_set_data (G_OBJECT (browser), "midori-toolbars-visible",
        gtk_widget_get_visible (browser->menubar)
        || gtk_widget_get_visible (browser->navigationbar)
        ? (void*)0xdeadbeef : NULL);
}

static void
_action_navigationbar_activate (GtkToggleAction* action,
                                MidoriBrowser*   browser)
{
    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (browser->settings, "show-navigationbar", active, NULL);
    sokoke_widget_set_visible (browser->navigationbar, active);

    g_object_set_data (G_OBJECT (browser), "midori-toolbars-visible",
        gtk_widget_get_visible (browser->menubar)
        || gtk_widget_get_visible (browser->navigationbar)
        ? (void*)0xdeadbeef : NULL);
}

static void
_action_bookmarkbar_activate (GtkToggleAction* action,
                              MidoriBrowser*   browser)
{
    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (browser->settings, "show-bookmarkbar", active, NULL);
    sokoke_widget_set_visible (browser->bookmarkbar, active);
}

static void
_action_statusbar_activate (GtkToggleAction* action,
                            MidoriBrowser*   browser)
{
    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (browser->settings, "show-statusbar", active, NULL);
    sokoke_widget_set_visible (browser->statusbar, active);
}

static void
_action_reload_stop_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    gchar* stock_id;
    g_object_get (action, "stock-id", &stock_id, NULL);

    /* Refresh or stop, depending on the stock id */
    if (!strcmp (stock_id, GTK_STOCK_REFRESH))
    {
        GdkModifierType state = (GdkModifierType)0;
        gint x, y;
        GdkWindow* window;
        gboolean from_cache = TRUE;

        if (!strcmp (gtk_action_get_name (action), "ReloadUncached"))
            from_cache = FALSE;
        else if ((window = gtk_widget_get_window (GTK_WIDGET (browser))))
        {
            gdk_window_get_pointer (window, &x, &y, &state);
            if (state & GDK_SHIFT_MASK)
                from_cache = FALSE;
        }
        midori_view_reload (MIDORI_VIEW (view), from_cache);
    }
    else
        midori_tab_stop_loading (MIDORI_TAB (view));
    g_free (stock_id);
}

static void
_action_zoom_activate (GtkAction*     action,
                       MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);

    if (g_str_equal (gtk_action_get_name (action), "ZoomIn"))
        midori_view_set_zoom_level (MIDORI_VIEW (view),
            midori_view_get_zoom_level (MIDORI_VIEW (view)) + 0.10f);
    else if (g_str_equal (gtk_action_get_name (action), "ZoomOut"))
        midori_view_set_zoom_level (MIDORI_VIEW (view),
            midori_view_get_zoom_level (MIDORI_VIEW (view)) - 0.10f);
    else
        midori_view_set_zoom_level (MIDORI_VIEW (view), 1.0f);
}

static void
_action_view_encoding_activate (GtkAction*     action,
                                GtkAction*     current,
                                MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    const gchar* name = gtk_action_get_name (current);
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (MIDORI_VIEW (view)));

    const gchar* encoding;
    if (!strcmp (name, "EncodingAutomatic"))
        encoding = NULL;
    else if (!strcmp (name, "EncodingChinese"))
        encoding = "BIG5";
    else if (!strcmp (name, "EncodingChineseSimplified"))
        encoding = "GB18030";
    else if (!strcmp (name, "EncodingJapanese"))
        encoding = "SHIFT_JIS";
    else if (!strcmp (name, "EncodingKorean"))
        encoding = "EUC-KR";
    else if (!strcmp (name, "EncodingRussian"))
        encoding = "KOI8-R";
    else if (!strcmp (name, "EncodingUnicode"))
        encoding = "UTF-8";
    else if (!strcmp (name, "EncodingWestern"))
        encoding = "ISO-8859-1";
    else
        g_assert_not_reached ();
    #ifdef HAVE_WEBKIT2
    webkit_web_view_set_custom_charset (web_view, encoding);
    #else
    webkit_web_view_set_custom_encoding (web_view, encoding);
    #endif
}

static void
_action_source_view (GtkAction*     action,
                     MidoriBrowser* browser,
                     gboolean       use_dom)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    #ifdef HAVE_WEBKIT2
    /* TODO: midori_view_save_source isn't async and not WebKit2-friendly */
    GtkWidget* source = midori_view_new_with_item (NULL, browser->settings);
    GtkWidget* source_view = midori_view_get_web_view (MIDORI_VIEW (source));
    //ZRL disable this.
#if 0
    midori_tab_set_view_source (MIDORI_TAB (source), TRUE);
#endif
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (source_view), midori_tab_get_uri (MIDORI_TAB (view)));
    midori_browser_add_tab (browser, source);
    #else
    gchar* text_editor;
    gchar* filename = NULL;

    filename = midori_view_save_source (MIDORI_VIEW (view), NULL, NULL, use_dom);
    g_object_get (browser->settings, "text-editor", &text_editor, NULL);
    if (!(text_editor && *text_editor))
    {
        GtkWidget* source;
        GtkWidget* source_view;
        gchar* source_uri;

        source_uri = g_filename_to_uri (filename, NULL, NULL);
        g_free (filename);

        source = midori_view_new_with_item (NULL, browser->settings);
        source_view = midori_view_get_web_view (MIDORI_VIEW (source));
        // ZRL disable this.
#if 
        //midori_tab_set_view_source (MIDORI_TAB (source), TRUE);
#endif
        webkit_web_view_load_uri (WEBKIT_WEB_VIEW (source_view), source_uri);
        midori_browser_add_tab (browser, source);
    }
    else
    {
        sokoke_spawn_program (text_editor, TRUE, filename, TRUE, FALSE);
        g_free (filename);
    }
    g_free (text_editor);
    #endif
}

static void
_action_source_view_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    _action_source_view (action, browser, FALSE);
}

static void
_action_source_view_dom_activate (GtkAction*     action,
                                  MidoriBrowser* browser)
{
    _action_source_view (action, browser, TRUE);
}


static void
_action_caret_browsing_activate (GtkAction*     action,
                                 MidoriBrowser* browser)
{
    gint response;
    GtkWidget* dialog;

    if (!katze_object_get_boolean (browser->settings, "enable-caret-browsing"))
    {
        dialog = gtk_message_dialog_new (GTK_WINDOW (browser),
            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
            GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
            _("Toggle text cursor navigation"));
        gtk_window_set_title (GTK_WINDOW (dialog), _("Toggle text cursor navigation"));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
            _("Pressing F7 toggles Caret Browsing. When active, a text cursor appears in all websites."));
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            _("_Enable Caret Browsing"), GTK_RESPONSE_ACCEPT,
            NULL);

        response = midori_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        if (response != GTK_RESPONSE_ACCEPT)
            return;
    }

    g_object_set (browser->settings, "enable-caret-browsing",
        !katze_object_get_boolean (browser->settings, "enable-caret-browsing"), NULL);
}

static void
_action_fullscreen_activate (GtkAction*     action,
                             MidoriBrowser* browser)
{
    GdkWindowState state;

    if (!gtk_widget_get_window (GTK_WIDGET (browser)))
        return;

    state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (browser)));
    if (state & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if (katze_object_get_boolean (G_OBJECT (browser->settings), "show-menubar"))
            {
            gtk_widget_show (browser->menubar);
            _action_set_visible(browser, "Preferences", TRUE);  //zgh
            }

        if (katze_object_get_boolean (G_OBJECT (browser->settings), "show-panel"))
            gtk_widget_show (browser->panel);

        if (katze_object_get_boolean (G_OBJECT (browser->settings), "show-bookmarkbar"))
            gtk_widget_show (browser->bookmarkbar);

        if (browser->show_navigationbar)
            gtk_widget_show (browser->navigationbar);

        if (browser->show_statusbar)
            gtk_widget_show (browser->statusbar);
        _toggle_tabbar_smartly (browser, TRUE);

        gtk_window_unfullscreen (GTK_WINDOW (browser));
    }
    else
    {
        gtk_widget_hide (browser->menubar);
        gtk_widget_hide (browser->panel);
        gtk_widget_hide (browser->bookmarkbar);
        gtk_widget_hide (browser->navigationbar);
        gtk_widget_hide (browser->statusbar);
        midori_notebook_set_labels_visible (MIDORI_NOTEBOOK (browser->notebook), FALSE);

        gtk_window_fullscreen (GTK_WINDOW (browser));
    }
}

static void
_action_scroll_somewhere_activate (GtkAction*     action,
                                   MidoriBrowser* browser)
{
#ifndef HAVE_WEBKIT2
    GtkWidget* view = midori_browser_get_current_tab (browser);
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (MIDORI_VIEW (view)));
    const gchar* name = gtk_action_get_name (action);

    if (g_str_equal (name, "ScrollLeft"))
        webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_VISUAL_POSITIONS, -1);
    else if (g_str_equal (name, "ScrollDown"))
        webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_DISPLAY_LINES, 1);
    else if (g_str_equal (name, "ScrollUp"))
        webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_DISPLAY_LINES, -1);
    else if (g_str_equal (name, "ScrollRight"))
        webkit_web_view_move_cursor (web_view, GTK_MOVEMENT_VISUAL_POSITIONS, 1);
#endif
}

static void
_action_readable_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    gchar* filename;
    gchar* stylesheet;
    gint i;

    filename = midori_paths_get_res_filename ("faq.css");
    stylesheet = NULL;
    if (!g_file_get_contents (filename, &stylesheet, NULL, NULL))
    {
        #ifdef G_OS_WIN32
        katze_assign (filename, midori_paths_get_data_filename ("doc/midori/faq.css", FALSE));
        #else
        katze_assign (filename, g_build_filename (DOCDIR, "faq.css", NULL));
        #endif
        g_file_get_contents (filename, &stylesheet, NULL, NULL);
    }
    if (!(stylesheet && *stylesheet))
    {
        g_free (filename);
        g_free (stylesheet);
        midori_view_add_info_bar (MIDORI_VIEW (view), GTK_MESSAGE_ERROR,
            "Stylesheet faq.css not found", NULL, view,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
        return;
    }

    i = 0;
    while (stylesheet[i])
    {
        /* Replace line breaks with spaces */
        if (stylesheet[i] == '\n' || stylesheet[i] == '\r')
            stylesheet[i] = ' ';
        /* Change all single quotes to double quotes */
        else if (stylesheet[i] == '\'')
            stylesheet[i] = '\"';
        i++;
    }

    midori_tab_inject_stylesheet (MIDORI_TAB (view), stylesheet);
    g_free (stylesheet);
}

static gboolean
_action_navigation_activate (GtkAction*     action,
                             MidoriBrowser* browser)
{
    MidoriView* view;
    GtkWidget* tab;
    gchar* uri;
    const gchar* name;
    gboolean middle_click;

    g_assert (GTK_IS_ACTION (action));

    if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action),
                                            "midori-middle-click")))
    {
        middle_click = TRUE;
        g_object_set_data (G_OBJECT (action),
                           "midori-middle-click",
                           GINT_TO_POINTER(0));
    }
    else
        middle_click = FALSE;

    tab = midori_browser_get_current_tab (browser);
    view = MIDORI_VIEW (tab);
    name = gtk_action_get_name (action);

    if (!strcmp (name, "NextForward"))
        name = midori_tab_can_go_forward (MIDORI_TAB (view)) ? "Forward" : "Next";

    if (g_str_equal (name, "Back"))
    {
        if (middle_click)
        {
            WebKitWebView* web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (view));
            #ifdef HAVE_WEBKIT2
            WebKitBackForwardList* list = webkit_web_view_get_back_forward_list (web_view);
            WebKitBackForwardListItem* item = webkit_back_forward_list_get_back_item (list);
            const gchar* back_uri = webkit_back_forward_list_item_get_uri (item);
            #else
            WebKitWebBackForwardList* list = webkit_web_view_get_back_forward_list (web_view);
            WebKitWebHistoryItem* item = webkit_web_back_forward_list_get_forward_item (list);
            const gchar* back_uri = webkit_web_history_item_get_uri (item);
            #endif

            GtkWidget* new_view = midori_browser_add_uri (browser, back_uri);
            midori_browser_set_current_tab_smartly (browser, new_view);
        }
        else
            {
            midori_view_go_back (view);
            _action_set_visible(browser, "Forward", TRUE);  //zgh
            }

        return TRUE;
    }
    else if (g_str_equal (name, "Forward"))
    {
        if (middle_click)
        {
            WebKitWebView* web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (view));
            #ifdef HAVE_WEBKIT2
            WebKitBackForwardList* list = webkit_web_view_get_back_forward_list (web_view);
            WebKitBackForwardListItem* item = webkit_back_forward_list_get_forward_item (list);
            const gchar* forward_uri = webkit_back_forward_list_item_get_uri (item);
            #else
            WebKitWebBackForwardList* list = webkit_web_view_get_back_forward_list (web_view);
            WebKitWebHistoryItem* item = webkit_web_back_forward_list_get_forward_item (list);
            const gchar* forward_uri = webkit_web_history_item_get_uri (item);
            #endif

            GtkWidget* new_view = midori_browser_add_uri (browser, forward_uri);
            midori_browser_set_current_tab_smartly (browser, new_view);
        }
        else
          midori_tab_go_forward (MIDORI_TAB (view));

        return TRUE;
    }
    else if (g_str_equal (name, "Previous"))
    {
        /* Duplicate here because the URI pointer might change */
        uri = g_strdup (midori_view_get_previous_page (view));

        if (middle_click)
        {
            GtkWidget* new_view = midori_browser_add_uri (browser, uri);
            midori_browser_set_current_tab_smartly (browser, new_view);
        }
        else
            midori_view_set_uri (view, uri);

        g_free (uri);
        return TRUE;
    }
    else if (g_str_equal (name, "Next"))
    {
        /* Duplicate here because the URI pointer might change */
        uri = g_strdup (midori_view_get_next_page (view));

        if (middle_click)
        {
            GtkWidget* new_view = midori_browser_add_uri (browser, uri);
            midori_browser_set_current_tab_smartly (browser, new_view);
        }
        else
            midori_view_set_uri (view, uri);

        g_free (uri);
        return TRUE;
    }
    else if (g_str_equal (name, "Homepage"))
    {
        if (middle_click)
        {
            GtkWidget* new_view = midori_browser_add_uri (browser, "about:home");
            midori_browser_set_current_tab_smartly (browser, new_view);
        }
        else
//zgh            midori_view_set_uri (view, "about:home");
            midori_view_set_uri (view, "http://www.baidu.com/");

        return TRUE;
    }
    return FALSE;
}

static void
_action_location_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    if (!gtk_widget_get_visible (browser->navigationbar))
        gtk_widget_show (browser->navigationbar);
}

static void
_action_location_focus_in (GtkAction*     action,
                           MidoriBrowser* browser)
{
    GdkScreen* screen = gtk_widget_get_screen (browser->notebook);
    GtkIconTheme* icon_theme = gtk_icon_theme_get_for_screen (screen);
    if (gtk_icon_theme_has_icon (icon_theme, "go-jump-symbolic"))
        midori_location_action_set_secondary_icon (
            MIDORI_LOCATION_ACTION (action), "go-jump-symbolic");
    else
        midori_location_action_set_secondary_icon (
            MIDORI_LOCATION_ACTION (action), GTK_STOCK_JUMP_TO);
}


static void
_action_location_focus_out (GtkAction*     action,
                            MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);

    if (!browser->show_navigationbar || midori_browser_is_fullscreen (browser))
        gtk_widget_hide (browser->navigationbar);

//    midori_browser_update_secondary_icon (browser, MIDORI_VIEW (view), action); //zgh 1203
}

static void
_action_location_reset_uri (GtkAction*     action,
                            MidoriBrowser* browser)
{
    midori_location_action_set_text (MIDORI_LOCATION_ACTION (action),
        midori_browser_get_current_uri (browser));
}

#ifndef HAVE_WEBKIT2
static void
midori_browser_item_icon_loaded_cb (WebKitFaviconDatabase* database,
                                    const gchar*           frame_uri,
                                    KatzeItem*             item)
{
    gchar* uri = g_object_get_data (G_OBJECT (item), "browser-queue-icon");
    if (strcmp (frame_uri, uri))
        return;

    gchar* icon_uri = webkit_favicon_database_get_favicon_uri (
        webkit_get_favicon_database (), frame_uri);
    if (icon_uri != NULL)
    {
        g_free (icon_uri);
        katze_item_set_icon (item, frame_uri);
        /* This signal fires extremely often (WebKit bug?)
           we must throttle it (disconnect) once we have an icon */
        g_signal_handlers_disconnect_by_func (webkit_get_favicon_database (),
            midori_browser_item_icon_loaded_cb, item);
    }
}
#endif

static void
midori_browser_queue_item_for_icon (KatzeItem*     item,
                                    const gchar*   uri)
{
#ifndef HAVE_WEBKIT2
    if (katze_item_get_icon (item) != NULL)
        return;
    g_object_set_data_full (G_OBJECT (item), "browser-queue-icon", g_strdup (uri), g_free);
    g_signal_connect (webkit_get_favicon_database (), "icon-loaded",
        G_CALLBACK (midori_browser_item_icon_loaded_cb), item);
#endif
}

static void
_action_location_submit_uri (GtkAction*     action,
                             const gchar*   uri,
                             gboolean       new_tab,
                             MidoriBrowser* browser)
{
    gchar* new_uri;

    /* Switch to already open tab if possible */
    KatzeItem* found = katze_array_find_uri (browser->proxy_array, uri);
    if (found != NULL && !new_tab
     && !g_str_equal (midori_browser_get_current_uri (browser), uri))
    {
        GtkWidget* view = midori_browser_get_current_tab (browser);
        midori_browser_set_current_item (browser, found);
        if (midori_view_is_blank (MIDORI_VIEW (view)))
            midori_browser_close_tab (browser, view);
        return;
    }

    uri = katze_skip_whitespace (uri);
    new_uri = sokoke_magic_uri (uri, TRUE, FALSE);
    if (!new_uri)
    {
        const gchar* keywords = NULL;
        const gchar* search_uri = NULL;
        KatzeItem* item;

        /* Do we have a keyword and a string? */
        if (browser->search_engines
         && (item = katze_array_find_token (browser->search_engines, uri)))
        {
            keywords = strchr (uri, ' ');
            if (keywords != NULL)
                keywords++;
            else
                keywords = "";
            search_uri = katze_item_get_uri (item);
        }

        if (keywords == NULL)
        {
            keywords = uri;
            search_uri = midori_settings_get_location_entry_search (
                MIDORI_SETTINGS (browser->settings));
        }
        new_uri = midori_uri_for_search (search_uri, keywords);

        if (browser->search_engines && item != NULL)
            midori_browser_queue_item_for_icon (item, new_uri);

        if (browser->history != NULL)
        {
            time_t now = time (NULL);
            gint64 day = sokoke_time_t_to_julian (&now);
            sqlite3* db = g_object_get_data (G_OBJECT (browser->history), "db");
            static sqlite3_stmt* statement = NULL;

            if (!statement)
            {
                const gchar* sqlcmd;
                sqlcmd = "INSERT INTO search (keywords, uri, day) VALUES (?,?,?)";
                sqlite3_prepare_v2 (db, sqlcmd, strlen (sqlcmd) + 1, &statement, NULL);
            }
            sqlite3_bind_text (statement, 1, keywords, -1, 0);
            sqlite3_bind_text (statement, 2, search_uri, -1, 0);
            sqlite3_bind_int64 (statement, 3, day);

            if (sqlite3_step (statement) != SQLITE_DONE)
                g_printerr (_("Failed to insert new history item: %s\n"),
                        sqlite3_errmsg (db));
            sqlite3_reset (statement);
            if (sqlite3_step (statement) == SQLITE_DONE)
                sqlite3_clear_bindings (statement);
        }
    }

    if (new_tab)
    {
        GtkWidget* view = midori_browser_add_uri (browser, new_uri);
        midori_browser_set_current_tab (browser, view);
    }
    else
        midori_browser_set_current_uri (browser, new_uri);
    g_free (new_uri);
    gtk_widget_grab_focus (midori_browser_get_current_tab (browser));
}

static void
midori_browser_news_feed_clicked_cb (GtkWidget*     menuitem,
                                     MidoriBrowser* browser)
{
    gchar* uri = g_object_get_data (G_OBJECT (menuitem), "uri");
    midori_browser_subscribe_to_news_feed (browser, uri);
}

static gboolean
_action_location_secondary_icon_released (GtkAction*     action,
                                          GtkWidget*     widget,
                                          MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    KatzeItem* bookmark = (KatzeItem*)katze_array_new (KATZE_TYPE_ARRAY);
    katze_item_set_name (bookmark,
        midori_view_get_display_title (MIDORI_VIEW (view)));
    katze_item_set_uri (bookmark, 
        midori_view_get_display_uri (MIDORI_VIEW (view)));
    katze_item_set_meta_integer (bookmark, "toolbar", TRUE);
    katze_item_set_meta_integer (bookmark, "parentid", 0);

    midori_bookmarks_db_add_item (browser->bookmarks, bookmark);
//            midori_bookmarks_db_update_item (browser->bookmarks, bookmark);

#if 0   //zgh   功能改为添加书栏
    GtkWidget* view = midori_browser_get_current_tab (browser);
        const gchar* uri = midori_view_get_display_uri (MIDORI_VIEW (view));
        const gchar* feed;
        /* Clicking icon on blank is equal to Paste and Proceed */
        if (midori_view_is_blank (MIDORI_VIEW (view)))
        {
            GtkClipboard* clipboard = gtk_clipboard_get_for_display (
                gtk_widget_get_display (view), GDK_SELECTION_CLIPBOARD);
            gchar* text = gtk_clipboard_wait_for_text (clipboard);
            if (text != NULL)
            {
                _action_location_submit_uri (action, text, FALSE, browser);
                g_free (text);
            }
        }
        else if (gtk_window_get_focus (GTK_WINDOW (browser)) == widget)
        {
            const gchar* text = gtk_entry_get_text (GTK_ENTRY (widget));
            _action_location_submit_uri (action, text, FALSE, browser);
        }
        else if ((feed = g_object_get_data (G_OBJECT (view), "news-feeds")))
        {
            KatzeArray* news_feeds;
            KatzeItem* item;
            KatzeItem* itemm;

            news_feeds = katze_object_get_object (G_OBJECT (view), "news-feeds");
            item = katze_array_get_nth_item (news_feeds, 0);
            if ((itemm = katze_array_get_nth_item (news_feeds, 1)))
            {
                guint i;
                GtkWidget* menu;
                GtkWidget* menuitem;

                menu = gtk_menu_new ();
                menuitem = gtk_menu_item_new_with_label (katze_item_get_name (item));
                g_object_set_data_full (G_OBJECT (menuitem), "uri",
                    g_strdup (katze_item_get_uri (item)), (GDestroyNotify)g_free);
                g_signal_connect (menuitem, "activate",
                    G_CALLBACK (midori_browser_news_feed_clicked_cb), browser);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
                menuitem = gtk_menu_item_new_with_label (katze_item_get_name (itemm));
                g_object_set_data_full (G_OBJECT (menuitem), "uri",
                    g_strdup (katze_item_get_uri (itemm)), (GDestroyNotify)g_free);
                g_signal_connect (menuitem, "activate",
                    G_CALLBACK (midori_browser_news_feed_clicked_cb), browser);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
                i = 2;
                while ((itemm = katze_array_get_nth_item (news_feeds, i++)))
                {
                    menuitem = gtk_menu_item_new_with_label (
                        katze_item_get_name (itemm));
                    g_object_set_data_full (G_OBJECT (menuitem), "uri",
                        g_strdup (katze_item_get_uri (itemm)), (GDestroyNotify)g_free);
                    g_signal_connect (menuitem, "activate",
                        G_CALLBACK (midori_browser_news_feed_clicked_cb), browser);
                    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
                }
                gtk_container_foreach (GTK_CONTAINER (menu),
                                       (GtkCallback)(gtk_widget_show_all), NULL);
                katze_widget_popup (widget, GTK_MENU (menu), NULL,
                                    KATZE_MENU_POSITION_RIGHT);
            }
            else
                midori_browser_subscribe_to_news_feed (browser, feed);
            g_object_unref (news_feeds);
        }
        else
            _action_location_submit_uri (action, uri, FALSE, browser);
#endif
        return TRUE;
}

static void
_action_search_submit (GtkAction*     action,
                       const gchar*   keywords,
                       gboolean       new_tab,
                       MidoriBrowser* browser)
{
    KatzeItem* item;
    const gchar* url;
    gchar* search;

    item = katze_array_get_nth_item (browser->search_engines, browser->last_web_search);
    if (item)
        url = katze_item_get_uri (item);
    else /* The location entry search is our fallback */
        url = midori_settings_get_location_entry_search (MIDORI_SETTINGS (browser->settings));

    search = midori_uri_for_search (url, keywords);
    if (item != NULL)
        midori_browser_queue_item_for_icon (item, search);

    if (new_tab)
    {
        GtkWidget* view = midori_browser_add_uri (browser, search);
        midori_browser_set_current_tab_smartly (browser, view);
    }
    else
        midori_browser_set_current_uri (browser, search);

    g_free (search);
}

static void
_action_search_activate (GtkAction*     action,
                         MidoriBrowser* browser)
{
    GSList* proxies = gtk_action_get_proxies (action);
    for (; proxies != NULL; proxies = g_slist_next (proxies))
        if (GTK_IS_TOOL_ITEM (proxies->data))
        {
            if (!gtk_widget_get_visible (browser->navigationbar))
                gtk_widget_show (browser->navigationbar);
            return;
        }

    midori_browser_set_current_uri (browser, "about:search");
    gtk_widget_grab_focus (midori_browser_get_current_tab (browser));
}

static void
_action_search_notify_current_item (GtkAction*     action,
                                    GParamSpec*    pspec,
                                    MidoriBrowser* browser)
{
    MidoriSearchAction* search_action;
    KatzeItem* item;
    guint idx;

    search_action = MIDORI_SEARCH_ACTION (action);
    item = midori_search_action_get_current_item (search_action);
    if (item)
        idx = katze_array_get_item_index (browser->search_engines, item);
    else
        idx = 0;

    g_object_set (browser->settings, "last-web-search", idx, NULL);
    browser->last_web_search = idx;
}

static void
_action_search_notify_default_item (GtkAction*     action,
                                    GParamSpec*    pspec,
                                    MidoriBrowser* browser)
{
    MidoriSearchAction* search_action;
    KatzeItem* item;

    search_action = MIDORI_SEARCH_ACTION (action);
    item = midori_search_action_get_default_item (search_action);
    if (item)
        g_object_set (browser->settings, "location-entry-search",
                      katze_item_get_uri (item), NULL);
}

static void
_action_search_focus_out (GtkAction*     action,
                          MidoriBrowser* browser)
{
    if ((gtk_widget_get_visible (browser->statusbar)
            && !browser->show_navigationbar)
            || midori_browser_is_fullscreen (browser))
    {
        gtk_widget_hide (browser->navigationbar);
    }
}

static void
midori_browser_bookmark_open_activate_cb (GtkWidget*     menuitem,
                                          MidoriBrowser* browser)
{
    KatzeItem* item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    midori_browser_open_bookmark (browser, item);
}

static void
midori_browser_bookmark_open_in_tab_activate_cb (GtkWidget*     menuitem,
                                                 MidoriBrowser* browser)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    if (KATZE_IS_ARRAY (item))
    {
        KatzeItem* child;
        KatzeArray* array;

        array = midori_bookmarks_db_query_recursive (browser->bookmarks,
            "*", "parentid = %q", katze_item_get_meta_string (item, "id"), FALSE);

        KATZE_ARRAY_FOREACH_ITEM (child, KATZE_ARRAY (array))
        {
            if ((uri = katze_item_get_uri (child)) && *uri)
            {
                GtkWidget* view = midori_browser_add_item (browser, child);
                midori_browser_set_current_tab_smartly (browser, view);
            }
        }
        g_object_unref (G_OBJECT (array));
    }
    else
    {
        if ((uri = katze_item_get_uri (item)) && *uri)
        {
            GtkWidget* view = midori_browser_add_item (browser, item);
            midori_browser_set_current_tab_smartly (browser, view);
        }
    }
}

static void
midori_browser_bookmark_open_in_window_activate_cb (GtkWidget*     menuitem,
                                                    MidoriBrowser* browser)
{
    KatzeItem* item;
    const gchar* uri;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    uri = katze_item_get_uri (item);
    midori_view_new_window_cb (NULL, uri, browser);
}

static void
midori_browser_bookmark_edit_activate_cb (GtkWidget* menuitem,
                                          GtkWidget* widget)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (widget);
    KatzeItem* item = g_object_get_data (G_OBJECT (menuitem), "KatzeItem");

    if (KATZE_ITEM_IS_BOOKMARK (item))
        midori_browser_edit_bookmark_dialog_new (browser, item, FALSE, FALSE, widget);
    else
        midori_browser_edit_bookmark_dialog_new (browser, item, FALSE, TRUE, widget);
}

static void
midori_browser_bookmark_delete_activate_cb (GtkWidget*     menuitem,
                                            MidoriBrowser* browser)
{
    KatzeItem* item;

    item = (KatzeItem*)g_object_get_data (G_OBJECT (menuitem), "KatzeItem");
    midori_bookmarks_db_remove_item (browser->bookmarks, item);
}

static void
midori_browser_bookmark_popup (GtkWidget*      widget,
                               GdkEventButton* event,
                               KatzeItem*      item,
                               MidoriBrowser*  browser)
{
    MidoriContextAction* menu = midori_context_action_new ("BookmarkContextMenu", NULL, NULL, NULL);
    if (KATZE_ITEM_IS_FOLDER (item))
    {
        gint child_bookmarks_count = midori_bookmarks_db_count_recursive (browser->bookmarks,
            "uri <> ''", NULL, item, FALSE);

        GtkAction* action = gtk_action_new ("BookmarkOpenAllTabs", _("Open all in _Tabs"), NULL, STOCK_TAB_NEW);
        gtk_action_set_sensitive (action, child_bookmarks_count > 0);
        g_object_set_data (G_OBJECT (action), "KatzeItem", item);
        g_signal_connect (action, "activate",
            G_CALLBACK (midori_browser_bookmark_open_in_tab_activate_cb), browser);
        midori_context_action_add (menu, action);
    }
    else
    {
        GtkAction* action = gtk_action_new ("BookmarkOpen", NULL, NULL, GTK_STOCK_OPEN);
        gtk_action_set_sensitive (action, katze_item_get_uri (item) != NULL);
        g_object_set_data (G_OBJECT (action), "KatzeItem", item);
        g_signal_connect (action, "activate",
            G_CALLBACK (midori_browser_bookmark_open_activate_cb), browser);
        midori_context_action_add (menu, action);
        action = gtk_action_new ("BookmarkOpenTab", NULL, NULL, STOCK_TAB_NEW);
        gtk_action_set_sensitive (action, katze_item_get_uri (item) != NULL);
        g_object_set_data (G_OBJECT (action), "KatzeItem", item);
        g_signal_connect (action, "activate",
            G_CALLBACK (midori_browser_bookmark_open_in_tab_activate_cb), browser);
        midori_context_action_add (menu, action);
        action = gtk_action_new ("BookmarkOpenWindow", _("Open in New _Window"), NULL, STOCK_WINDOW_NEW);
        gtk_action_set_sensitive (action, katze_item_get_uri (item) != NULL);
        g_object_set_data (G_OBJECT (action), "KatzeItem", item);
        g_signal_connect (action, "activate",
            G_CALLBACK (midori_browser_bookmark_open_in_window_activate_cb), browser);
        midori_context_action_add (menu, action);
    }

    midori_context_action_add (menu, NULL);
    GtkAction* action = gtk_action_new ("BookmarkEdit", NULL, NULL, GTK_STOCK_EDIT);
    gtk_action_set_sensitive (action, !KATZE_ITEM_IS_SEPARATOR (item));
    g_object_set_data (G_OBJECT (action), "KatzeItem", item);
    g_signal_connect (action, "activate",
        G_CALLBACK (midori_browser_bookmark_edit_activate_cb), browser);
    midori_context_action_add (menu, action);
    action = gtk_action_new ("BookmarkDelete", NULL, NULL, GTK_STOCK_DELETE);
    g_object_set_data (G_OBJECT (action), "KatzeItem", item);
    g_signal_connect (action, "activate",
        G_CALLBACK (midori_browser_bookmark_delete_activate_cb), browser);
    midori_context_action_add (menu, action);

    GtkMenu* context_menu = midori_context_action_create_menu (menu, NULL, FALSE);
    katze_widget_popup (widget, context_menu, event, KATZE_MENU_POSITION_CURSOR);
}

static gboolean
midori_browser_menu_button_press_event_cb (GtkWidget*      toolitem,
                                           GdkEventButton* event,
                                           MidoriBrowser*  browser)
{
    if (event->button != 3)
        return FALSE;

    /* GtkMenuBar catches button events on children with submenus,
       so we need to see if the actual widget is the menubar, and if
       it is an item, we forward it to the actual widget. */
    if ((GTK_IS_BOX (toolitem) || GTK_IS_MENU_BAR (toolitem)))
    {
        if (gtk_widget_get_window (toolitem) != event->window)
            return FALSE;

        midori_browser_toolbar_popup_context_menu_cb (
            GTK_IS_BIN (toolitem) && gtk_bin_get_child (GTK_BIN (toolitem)) ?
                gtk_widget_get_parent (toolitem) : toolitem,
            event->x, event->y, event->button, browser);
        return TRUE;
    }
    else if (GTK_IS_MENU_ITEM (toolitem))
    {
        gboolean handled;
        g_signal_emit_by_name (toolitem, "button-press-event", event, &handled);
        return handled;
    }
    return FALSE;
}

static gboolean
midori_browser_menu_item_middle_click_event_cb (GtkWidget*      toolitem,
                                                GdkEventButton* event,
                                                MidoriBrowser*  browser)
{
    if (MIDORI_EVENT_NEW_TAB (event))
    {
        GtkAction* action;

        action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (toolitem));
        g_object_set_data (G_OBJECT (action), "midori-middle-click", GINT_TO_POINTER (1));

        return _action_navigation_activate (action, browser);
    }
    return FALSE;
}

static void
_action_bookmark_add_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    GtkWidget* proxy = NULL;
    GSList* proxies = gtk_action_get_proxies (action);
    for (; proxies != NULL; proxies = g_slist_next (proxies))
    if (GTK_IS_TOOL_ITEM (proxies->data))
    {
        proxy = proxies->data;
        break;
    }

    if (g_str_equal (gtk_action_get_name (action), "BookmarkFolderAdd"))
        midori_browser_edit_bookmark_dialog_new (browser, NULL, TRUE, TRUE, proxy);
    else
        midori_browser_edit_bookmark_dialog_new (browser, NULL, TRUE, FALSE, proxy);
}

static void
_action_bookmarks_import_activate (GtkAction*     action,
                                   MidoriBrowser* browser)
{
    typedef struct
    {
        const gchar* path;
        const gchar* name;
        const gchar* icon;
    } BookmarkClient;
    static const BookmarkClient bookmark_clients[] = {
        { ".local/share/data/Arora/bookmarks.xbel", N_("Arora"), "arora" },
        { ".kazehakase/bookmarks.xml", N_("Kazehakase"), "kazehakase-icon" },
        { ".opera/bookmarks.adr", N_("Opera"), "opera" },
        { ".kde/share/apps/konqueror/bookmarks.xml", N_("Konqueror"), "konqueror" },
        { ".gnome2/epiphany/bookmarks.rdf", N_("Epiphany"), "epiphany" },
        { ".mozilla/firefox/*/bookmarks.html", N_("Firefox (%s)"), "firefox" },
        { ".config/midori/bookmarks.xbel", N_("Midori 0.2.6"), "midori" },
    };

    GtkWidget* dialog;
    GtkWidget* content_area;
    GtkSizeGroup* sizegroup;
    GtkWidget* hbox;
    GtkWidget* label;
    GtkWidget* combo;
    GtkComboBox* combobox;
    GtkListStore* model;
    GtkCellRenderer* renderer;
    GtkWidget* combobox_folder;
    gint icon_width = 16;
    guint i;
    KatzeArray* bookmarks;

    dialog = gtk_dialog_new_with_buttons (
        _("Import bookmarks…"), GTK_WINDOW (browser),
        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        _("_Import bookmarks"), GTK_RESPONSE_ACCEPT,
        NULL);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_window_set_icon_name (GTK_WINDOW (dialog), STOCK_BOOKMARKS);

    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    sizegroup =  gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    label = gtk_label_new_with_mnemonic (_("_Application:"));
    gtk_size_group_add_widget (sizegroup, label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    model = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING,
                                   G_TYPE_STRING, G_TYPE_INT);
    combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (model));
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "icon-name", 1);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "width", 3);
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "text", 0);
    combobox = GTK_COMBO_BOX (combo);
    gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (GTK_WIDGET (browser)),
                                       GTK_ICON_SIZE_MENU, &icon_width, NULL);
    for (i = 0; i < G_N_ELEMENTS (bookmark_clients); i++)
    {
        const gchar* location = bookmark_clients[i].path;
        const gchar* client = bookmark_clients[i].name;
        gchar* path = NULL;

        /* Interpret * as 'any folder' */
        if (strchr (location, '*') != NULL)
        {
            gchar** parts = g_strsplit (location, "*", 2);
            GDir* dir;
            path = g_build_filename (g_get_home_dir (), parts[0], NULL);
            if ((dir = g_dir_open (path, 0, NULL)))
            {
                const gchar* name;
                while ((name = g_dir_read_name (dir)))
                {
                    gchar* file = g_build_filename (path, name, parts[1], NULL);
                    if (g_access (file, F_OK) == 0)
                    {
                        /* If name is XYZ.Name, we use Name only */
                        gchar* real_name = strchr (name, '.');
                        gchar* display = strstr (_(client), "%s")
                            ? g_strdup_printf (_(client),
                                  real_name ? real_name + 1 : name)
                            : g_strdup (_(client));
                        gtk_list_store_insert_with_values (model, NULL, G_MAXINT,
                            0, display, 1, bookmark_clients[i].icon,
                            2, file, 3, icon_width, -1);
                        g_free (display);
                    }
                    g_free (file);
                }
                g_dir_close (dir);
            }
            g_free (path);
            g_strfreev (parts);
            continue;
        }

        path = g_build_filename (g_get_home_dir (), location, NULL);
        if (g_access (path, F_OK) == 0)
            gtk_list_store_insert_with_values (model, NULL, G_MAXINT,
                0, _(client), 1, bookmark_clients[i].icon,
                2, path, 3, icon_width, -1);
        g_free (path);
    }

    gtk_list_store_insert_with_values (model, NULL, G_MAXINT,
        0, _("Import from XBEL or HTML file"), 1, NULL, 2, NULL, 3, icon_width, -1);
    gtk_combo_box_set_active (combobox, 0);
    gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, TRUE, 0);

    combobox_folder = midori_bookmark_folder_button_new (browser->bookmarks, 0);
    gtk_box_pack_start (GTK_BOX (content_area), combobox_folder, FALSE, TRUE, 0);
    gtk_widget_show_all (content_area);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    if (midori_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        GtkTreeIter iter;
        gchar* path = NULL;
        gint64 selected;
        GError* error;

        if (gtk_combo_box_get_active_iter (combobox, &iter))
            gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 2, &path, -1);
        selected = midori_bookmark_folder_button_get_active (combobox_folder);

        gtk_widget_destroy (dialog);
        if (!path)
        {
            GtkWidget* file_dialog;

            file_dialog = (GtkWidget*)midori_file_chooser_dialog_new (_("Import from a file"),
                GTK_WINDOW (browser), GTK_FILE_CHOOSER_ACTION_OPEN);
            if (midori_dialog_run (GTK_DIALOG (file_dialog)) == GTK_RESPONSE_OK)
                path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_dialog));
            gtk_widget_destroy (file_dialog);
        }

        error = NULL;
        bookmarks = katze_array_new (KATZE_TYPE_ARRAY);
        if (path && !midori_array_from_file (bookmarks, path, NULL, &error))
        {
            sokoke_message_dialog (GTK_MESSAGE_ERROR,
                _("Failed to import bookmarks"),
                error ? error->message : "", FALSE);
            if (error)
                g_error_free (error);
        }
        midori_bookmarks_db_import_array (browser->bookmarks, bookmarks, selected);
        g_object_unref (bookmarks);
        g_free (path);
    }
    else
        gtk_widget_destroy (dialog);
}

static void
_action_bookmarks_export_activate (GtkAction*     action,
                                   MidoriBrowser* browser)
{
    GtkWidget* file_dialog;
    GtkFileFilter* filter;
    const gchar* format;
    gchar* path = NULL;
    GError* error;
    KatzeArray* bookmarks;

wrong_format:
    file_dialog = (GtkWidget*)midori_file_chooser_dialog_new (_("Save file as"),
        GTK_WINDOW (browser), GTK_FILE_CHOOSER_ACTION_SAVE);
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_dialog),
                                       "bookmarks.xbel");
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("XBEL Bookmarks"));
    gtk_file_filter_add_mime_type (filter, "application/xml");
    gtk_file_filter_add_pattern (filter, "*.xbel");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), filter);
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("Netscape Bookmarks"));
    gtk_file_filter_add_mime_type (filter, "text/html");
    gtk_file_filter_add_pattern (filter, "*.html");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), filter);
    if (midori_dialog_run (GTK_DIALOG (file_dialog)) == GTK_RESPONSE_OK)
        path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_dialog));
    gtk_widget_destroy (file_dialog);

    if (path == NULL)
        return;

    if (g_str_has_suffix (path, ".xbel"))
        format = "xbel";
    else if (g_str_has_suffix (path, ".html"))
        format = "netscape";
    else
    {
        sokoke_message_dialog (GTK_MESSAGE_ERROR,
            _("Midori can only export to XBEL (*.xbel) and Netscape (*.html)"),
            "", TRUE);
        katze_assign (path, NULL);
        goto wrong_format;
    }

    error = NULL;
    bookmarks = midori_bookmarks_db_query_recursive (browser->bookmarks,
        "*", "parentid IS NULL", NULL, TRUE);
    if (!midori_array_to_file (bookmarks, path, format, &error))
    {
        sokoke_message_dialog (GTK_MESSAGE_ERROR,
            _("Failed to export bookmarks"), error ? error->message : "", FALSE);
        if (error)
            g_error_free (error);
    }
    g_object_unref (bookmarks);
    g_free (path);
}

static void
_action_manage_search_engines_activate (GtkAction*     action,
                                        MidoriBrowser* browser)
{
    static GtkWidget* dialog = NULL;

    if (!dialog)
    {
        dialog = midori_search_action_get_dialog (
            MIDORI_SEARCH_ACTION (_action_by_name (browser, "Search")));
        g_signal_connect (dialog, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &dialog);
        gtk_widget_show (dialog);
    }
    else
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
_action_clear_private_data_activate (GtkAction*     action,
                                     MidoriBrowser* browser)
{
    static GtkWidget* dialog = NULL;

    if (!dialog)
    {
        dialog = midori_private_data_get_dialog (browser);
        g_signal_connect (dialog, "destroy",
            G_CALLBACK (gtk_widget_destroyed), &dialog);
        gtk_widget_show (dialog);
    }
    else
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
_action_inspect_page_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    WebKitWebView* web_view = WEBKIT_WEB_VIEW (midori_view_get_web_view (MIDORI_VIEW (view)));
    WebKitWebInspector* inspector = webkit_web_view_get_inspector (web_view);
    webkit_web_inspector_show (inspector);
}

static void
_action_tab_move_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    const gchar* name = gtk_action_get_name (action);
    gint new_pos;
    gint cur_pos = midori_browser_get_current_page (browser);
    GtkWidget* widget = midori_browser_get_nth_tab (browser, cur_pos);

    if (!strcmp (name, "TabMoveFirst"))
        new_pos = 0;
    else if (!strcmp (name, "TabMoveBackward"))
    {
        if (cur_pos > 0)
            new_pos = cur_pos - 1;
        else
            new_pos = midori_browser_get_n_pages (browser) - 1;
    }
    else if (!strcmp (name, "TabMoveForward"))
    {
        if (cur_pos == (midori_browser_get_n_pages (browser) - 1))
            new_pos = 0;
        else
            new_pos = cur_pos + 1;
    }
    else if (!strcmp (name, "TabMoveLast"))
        new_pos = midori_browser_get_n_pages (browser) - 1;
    else
        g_assert_not_reached ();

    midori_notebook_move (MIDORI_NOTEBOOK (browser->notebook), MIDORI_TAB (widget), new_pos);
    g_signal_emit (browser, signals[MOVE_TAB], 0, browser->notebook, cur_pos, new_pos);
}

static void
_action_tab_previous_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    gint n = midori_browser_get_current_page (browser);
    midori_browser_set_current_page (browser, n - 1);
}

static void
_action_tab_next_activate (GtkAction*     action,
                           MidoriBrowser* browser)
{
    /* Advance one tab or jump to the first one if we are at the last one */
    gint n = midori_browser_get_current_page (browser);
    if (n == midori_browser_get_n_pages (browser) - 1)
        n = -1;
    midori_browser_set_current_page (browser, n + 1);
}

static void
_action_tab_current_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    gtk_widget_grab_focus (view);
}

static void
_action_next_view_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    gtk_widget_grab_focus (midori_browser_get_current_tab (browser));
}

static void
_action_tab_minimize_activate (GtkAction*     action,
                               MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    g_object_set (view, "minimized",
                  !katze_object_get_boolean (view, "minimized"), NULL);
}

static void
_action_tab_duplicate_activate (GtkAction*     action,
                                MidoriBrowser* browser)
{
    GtkWidget* view = g_object_get_data (G_OBJECT (action), "tab");
    if (view == NULL)
        view = midori_browser_get_current_tab (browser);
    midori_view_duplicate (MIDORI_VIEW (view));
}

static void
_action_tab_close_other_activate (GtkAction*     action,
                                  MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_get_current_tab (browser);
    GList* tabs = midori_browser_get_tabs (browser);
    for (; tabs; tabs = g_list_next (tabs))
    {
        if (tabs->data != view)
            midori_browser_close_tab (browser, tabs->data);
    }
    g_list_free (tabs);
}

static const gchar* credits_authors[] =
    { "Zhang RuiLi <zhangrl_os@sari.ac.cn>", NULL };
static const gchar* credits_documenters[] =
    { "Zhang RuiLi <zhangrl_os@sari.ac.cn>", NULL };
static const gchar* credits_artists[] =
    { "Zhang RuiLi <zhangrl_os@sari.ac.cn>", NULL };

static gchar*
midori_browser_get_docs (gboolean error)
{
    #ifdef G_OS_WIN32
    gchar* path = midori_paths_get_data_filename ("doc/midori/faq.html", FALSE);
    gchar* uri = g_filename_to_uri (path, NULL, NULL);
    g_free (path);
    return uri;
    #else
    gchar* path = midori_paths_get_res_filename ("faq.html");
    gboolean found = g_access (path, F_OK) == 0;
    if (found)
    {
        gchar* uri = g_filename_to_uri (path, NULL, NULL);
        g_free (path);
        return uri;
    }
    g_free (path);
    return g_strdup ("file://" DOCDIR "/faq.html");
    #endif
}

static void
_action_about_activate (GtkAction*     action,
                        MidoriBrowser* browser)
{
// ZRL customize about dialog
#if 0
    gchar* comments = g_strdup_printf ("%s\n%s",
        _("A lightweight web browser."),
        _("See about:version for version info."));
    const gchar* license =
    _("This library is free software; you can redistribute it and/or "
    "modify it under the terms of the GNU Lesser General Public "
    "License as published by the Free Software Foundation; either "
    "version 2.1 of the License, or (at your option) any later version.");

#ifdef HAVE_GRANITE
    gchar* docs = midori_browser_get_docs (FALSE);
    /* Avoid granite_widgets_show_about_dialog for invalid memory and crashes */
    /* FIXME: granite: should return GtkWidget* like GTK+ */
    GtkWidget* dialog = (GtkWidget*)granite_widgets_about_dialog_new ();
    g_object_set (dialog,
        "translate", "https://translations.xfce.org/projects/p/midori/",
        "bug", PACKAGE_BUGREPORT,
        "help", docs,
        "copyright", "2007-2013 Christian Dywan",
#else
    GtkWidget* dialog = gtk_about_dialog_new ();
    g_object_set (dialog,
        "wrap-license", TRUE,
        "copyright", "Copyright © 2007-2013 Christian Dywan",
#endif
        "transient-for", browser,
        "logo-icon-name", gtk_window_get_icon_name (GTK_WINDOW (browser)),
        "program-name", PACKAGE_NAME,
        "version", PACKAGE_VERSION,
        "comments", comments,
        "website", "http://www.midori-browser.org",
        "authors", credits_authors,
        "documenters", credits_documenters,
        "artists", credits_artists,
        "license", license,
        "translator-credits", _("translator-credits"),
        NULL);
    g_free (comments);
    #ifdef HAVE_GRANITE
    g_free (docs);
    #endif
    gtk_widget_show (dialog);
    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy), dialog);
#else
    gchar* comments = g_strdup_printf ("%s\n%s",
        _("A Security web browser."),
        _("See about:version for version info."));
    const gchar* license =
    _("This library is free software; you can redistribute it and/or "
    "modify it under the terms of the GNU Lesser General Public "
    "License as published by the Free Software Foundation; either "
    "version 2.1 of the License, or (at your option) any later version.");

#ifdef HAVE_GRANITE
    gchar* docs = midori_browser_get_docs (FALSE);
    /* Avoid granite_widgets_show_about_dialog for invalid memory and crashes */
    /* FIXME: granite: should return GtkWidget* like GTK+ */
    GtkWidget* dialog = (GtkWidget*)granite_widgets_about_dialog_new ();
    g_object_set (dialog,
        "translate", "https://translations.xfce.org/projects/p/midori/",
        "bug", PACKAGE_BUGREPORT,
        "help", docs,
        "copyright", "2007-2013 Christian Dywan",
#else
    GtkWidget* dialog = gtk_about_dialog_new ();
    g_object_set (dialog,
        "wrap-license", FALSE,
        "copyright", "Copyright © 2013-2014 cdosrepobuild <cdosrepobuild@iscas.ac.cn>",
#endif
        "transient-for", browser,
        "logo-icon-name", gtk_window_get_icon_name (GTK_WINDOW (browser)),
        "program-name", PACKAGE_NAME,
        "version", PACKAGE_VERSION,
        "comments", comments,
        "website", "http://www.baidu.com",
        NULL);
    g_free (comments);
    #ifdef HAVE_GRANITE
    g_free (docs);
    #endif
    printf("ZRL about icon name = %s. \n", gtk_window_get_icon_name (GTK_WINDOW (browser)));
    gtk_widget_show (dialog);
    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy), dialog);
#endif
}

static void
_action_help_link_activate (GtkAction*     action,
                            MidoriBrowser* browser)
{
    const gchar* action_name = gtk_action_get_name (action);
    gchar* uri = NULL;
    if (!strncmp ("HelpFAQ", action_name, 7))
        uri = midori_browser_get_docs (TRUE);
    else if  (!strncmp ("HelpBugs", action_name, 8))
    {
        if (!g_spawn_command_line_async ("ubuntu-bug " PACKAGE_NAME, NULL))
            uri = g_strdup (PACKAGE_BUGREPORT);
    }

    if (uri)
    {
        GtkWidget* view = midori_browser_add_uri (browser, uri);
        midori_browser_set_current_tab (browser, view);
        g_free (uri);
    }
}

static void
_action_panel_activate (GtkToggleAction* action,
                        MidoriBrowser*   browser)
{
    g_print("sidespanel out\n");
    gboolean active = gtk_toggle_action_get_active (action);
    g_object_set (browser->settings, "show-panel", active, NULL);
    sokoke_widget_set_visible (browser->panel, active);
}

static gboolean
midori_browser_panel_timeout (GtkWidget* hpaned)
{
    gint position = gtk_paned_get_position (GTK_PANED (hpaned));
    MidoriBrowser* browser = midori_browser_get_for_widget (hpaned);
    g_object_set (browser->settings, "last-panel-position", position, NULL);
    browser->panel_timeout = 0;
    return FALSE;
}

static void
midori_panel_notify_position_cb (GObject*       hpaned,
                                 GParamSpec*    pspec,
                                 MidoriBrowser* browser)
{
    if (!browser->panel_timeout)
        browser->panel_timeout = midori_timeout_add_seconds (5,
            (GSourceFunc)midori_browser_panel_timeout, hpaned, NULL);
}

static gboolean
midori_panel_cycle_child_focus_cb (GtkWidget*     hpaned,
                                   gboolean       reversed,
                                   MidoriBrowser* browser)
{
    /* Default cycle goes between all GtkPaned widgets.
       If focus is in the panel, focus the location as if it's a paned.
       If nothing is focussed, simply go to the location.
       Be sure to suppress the default because the signal can recurse. */
    GtkWidget* focus = gtk_window_get_focus (GTK_WINDOW (browser));
    if (gtk_widget_get_ancestor (focus, MIDORI_TYPE_PANEL)
     || !gtk_widget_get_ancestor (focus, GTK_TYPE_PANED))
    {
        g_signal_stop_emission_by_name (hpaned, "cycle-child-focus");
        midori_browser_activate_action (browser, "Location");
        return TRUE;
    }
    return FALSE;
}

static void
midori_panel_notify_page_cb (MidoriPanel*   panel,
                             GParamSpec*    pspec,
                             MidoriBrowser* browser)
{
    gint page = midori_panel_get_current_page (panel);
    if (page > -1)
        g_object_set (browser->settings, "last-panel-page", page, NULL);
}

static void
midori_panel_notify_show_titles_cb (MidoriPanel*   panel,
                                    GParamSpec*    pspec,
                                    MidoriBrowser* browser)
{
    gboolean show_titles = katze_object_get_boolean (panel, "show-titles");
    g_signal_handlers_block_by_func (browser->settings,
        midori_browser_settings_notify, browser);
    g_object_set (browser->settings, "compact-sidepanel", !show_titles, NULL);
    g_signal_handlers_unblock_by_func (browser->settings,
        midori_browser_settings_notify, browser);
}

static void
midori_panel_notify_right_aligned_cb (MidoriPanel*   panel,
                                      GParamSpec*    pspec,
                                      MidoriBrowser* browser)
{
    gboolean right_aligned = katze_object_get_boolean (panel, "right-aligned");
    GtkWidget* hpaned = gtk_widget_get_parent (browser->panel);
    GtkWidget* vpaned = gtk_widget_get_parent (browser->notebook);
    gint paned_position = gtk_paned_get_position (GTK_PANED (hpaned));
    GtkAllocation allocation;
    gint paned_size;

    gtk_widget_get_allocation (hpaned, &allocation);
    paned_size = allocation.width;

    g_object_set (browser->settings, "right-align-sidepanel", right_aligned, NULL);

    g_object_ref (browser->panel);
    g_object_ref (vpaned);
    gtk_container_remove (GTK_CONTAINER (hpaned), browser->panel);
    gtk_container_remove (GTK_CONTAINER (hpaned), vpaned);
    if (right_aligned)
    {
        gtk_paned_pack1 (GTK_PANED (hpaned), vpaned, TRUE, FALSE);
        gtk_paned_pack2 (GTK_PANED (hpaned), browser->panel, FALSE, FALSE);
    }
    else
    {
        gtk_paned_pack1 (GTK_PANED (hpaned), browser->panel, FALSE, FALSE);
        gtk_paned_pack2 (GTK_PANED (hpaned), vpaned, TRUE, FALSE);
    }
    gtk_paned_set_position (GTK_PANED (hpaned), paned_size - paned_position);
    g_object_unref (browser->panel);
    g_object_unref (vpaned);
}

static gboolean
midori_panel_close_cb (MidoriPanel*   panel,
                       MidoriBrowser* browser)
{
    _action_set_active (browser, "Panel", FALSE);
    return FALSE;
}

static void
midori_browser_switched_tab_cb (MidoriNotebook* notebook,
                                GtkWidget*      old_widget,
                                MidoriView*     new_view,
                                MidoriBrowser*  browser)
{
    GtkAction* action;
    const gchar* text;
    const gchar* uri;

    if (old_widget != NULL)
    {
        action = _action_by_name (browser, "Location");
        text = midori_location_action_get_text (MIDORI_LOCATION_ACTION (action));
        g_object_set_data_full (G_OBJECT (old_widget), "midori-browser-typed-text",
                                g_strdup (text), g_free);
    }

    g_return_if_fail (MIDORI_IS_VIEW (new_view));
    g_return_if_fail (new_view != MIDORI_VIEW (old_widget));

    uri = g_object_get_data (G_OBJECT (new_view), "midori-browser-typed-text");
    if (!uri)
        uri = midori_view_get_display_uri (new_view);
    midori_browser_set_title (browser, midori_view_get_display_title (new_view));
    action = _action_by_name (browser, "Location");
    midori_location_action_set_text (MIDORI_LOCATION_ACTION (action), uri);
    if (midori_paths_get_runtime_mode () == MIDORI_RUNTIME_MODE_APP)
        gtk_window_set_icon (GTK_WINDOW (browser), midori_view_get_icon (new_view));

    g_signal_emit (browser, signals[SWITCH_TAB], 0, old_widget, new_view);
    _midori_browser_set_statusbar_text (browser, new_view, NULL);
    _midori_browser_update_interface (browser, new_view);
    _midori_browser_update_progress (browser, new_view);
}

static void
midori_browser_notify_tab_cb (GtkWidget*     notebook,
                              GParamSpec*    pspec,
                              MidoriBrowser* browser)
{
    g_object_freeze_notify (G_OBJECT (browser));
    g_object_notify (G_OBJECT (browser), "uri");
    g_object_notify (G_OBJECT (browser), "title");
    g_object_notify (G_OBJECT (browser), "tab");
    g_object_thaw_notify (G_OBJECT (browser));
}

static void
midori_browser_tab_moved_cb (GtkWidget*     notebook,
                             MidoriView*    view,
                             guint          page_num,
                             MidoriBrowser* browser)
{
    KatzeItem* item = midori_view_get_proxy_item (view);
    katze_array_move_item (browser->proxy_array, item, page_num);
}

static void
midori_browser_notebook_create_window_cb (GtkWidget*     notebook,
                                          GtkWidget*     view,
                                          gint           x,
                                          gint           y,
                                          MidoriBrowser* browser)
{
    MidoriBrowser* new_browser;
    g_signal_emit (browser, signals[NEW_WINDOW], 0, NULL, &new_browser);
    if (new_browser)
    {
        gtk_window_move (GTK_WINDOW (new_browser), x, y);
        g_object_ref (view);
        midori_browser_disconnect_tab (browser, MIDORI_VIEW (view));
        midori_notebook_remove (MIDORI_NOTEBOOK (browser->notebook), MIDORI_TAB (view));
        midori_browser_add_tab (new_browser, view);
        g_object_unref (view);
    }
}

static void
midori_browser_notebook_new_tab_cb (GtkWidget*     notebook,
                                    MidoriBrowser* browser)
{
    GtkWidget* view = midori_browser_add_uri (browser, "about:new");
    midori_browser_set_current_tab (browser, view);
}

static void
midori_browser_notebook_context_menu_cb (MidoriNotebook*      notebook,
                                         MidoriContextAction* menu,
                                         MidoriBrowser*       browser)
{
    midori_context_action_add_action_group (menu, browser->action_group);
    midori_context_action_add (menu, NULL);
    midori_context_action_add_by_name (menu, "TabNew");
    midori_context_action_add_by_name (menu, "UndoTabClose");
}

static void
midori_browser_notebook_tab_context_menu_cb (MidoriNotebook*      notebook,
                                             MidoriTab*           tab,
                                             MidoriContextAction* menu,
                                             MidoriBrowser*       browser)
{
    midori_context_action_add_action_group (menu, browser->action_group);
    midori_context_action_add (menu, NULL);
    midori_context_action_add_by_name (menu, "TabNew");
    midori_context_action_add_by_name (menu, "UndoTabClose");
    if (MIDORI_IS_VIEW (tab))
    {
        GtkAction* action = gtk_action_new ("TabDuplicate", _("_Duplicate Current Tab"), NULL, NULL);
        g_object_set_data (G_OBJECT (action), "tab", tab);
        g_signal_connect (action, "activate",
            G_CALLBACK (_action_tab_duplicate_activate), browser);
        midori_context_action_add (menu, action);
    }
}

static void
_action_undo_tab_close_activate (GtkAction*     action,
                                 MidoriBrowser* browser)
{
    guint last;
    KatzeItem* item;

    if (!browser->trash)
        return;

    /* Reopen the most recent trash item */
    last = katze_array_get_length (browser->trash) - 1;
    item = katze_array_get_nth_item (browser->trash, last);
    midori_browser_set_current_tab (browser,
        midori_browser_restore_tab (browser, item));
}

static void
_action_trash_empty_activate (GtkAction*     action,
                              MidoriBrowser* browser)
{
    if (browser->trash)
        katze_array_clear (browser->trash);
}

static const GtkActionEntry entries[] =
{
    //by zgh
    { "File", NULL, N_("_File") },
    { "WindowNew", STOCK_WINDOW_NEW,
        N_("New _Window"), "<Ctrl>n",
        N_("Open a new window"), G_CALLBACK (_action_window_new_activate) },
    { "TabNew", STOCK_TAB_NEW,
        NULL, "<Ctrl>t",
        N_("Open a new tab"), G_CALLBACK (_action_tab_new_activate) },
    { "PrivateBrowsing", NULL,
        N_("New P_rivate Browsing Window"), "<Ctrl><Shift>n",
        NULL, G_CALLBACK (_action_private_browsing_activate), },
    { "Open", GTK_STOCK_OPEN,
        NULL, "<Ctrl>o",
        N_("Open a file"), G_CALLBACK (_action_open_activate) },
    { "SaveAs", GTK_STOCK_SAVE_AS,
        N_("_Save Page As…"), "<Ctrl>s",
        N_("Save to a file"), G_CALLBACK (_action_save_as_activate) },
        #if 0 //zgh
    { "AddSpeedDial", NULL,
        N_("Add to Speed _dial"), "<Ctrl>h",
        NULL, G_CALLBACK (_action_add_speed_dial_activate) },
        #endif
    { "AddNewsFeed", NULL,
        N_("Subscribe to News _feed"), NULL,
        NULL, G_CALLBACK (_action_add_news_feed_activate) },
    { "CompactAdd", GTK_STOCK_ADD,
        NULL, NULL,
        NULL, G_CALLBACK (_action_compact_add_activate) },
        #if 0 //zgh
    { "TabClose", GTK_STOCK_CLOSE,
        N_("_Close Tab"), "<Ctrl>w",
        N_("Close the current tab"), G_CALLBACK (_action_tab_close_activate) },
    { "WindowClose", NULL,
        N_("C_lose Window"), "<Ctrl><Shift>w",
        NULL, G_CALLBACK (_action_window_close_activate) },
        #endif
    { "Print", GTK_STOCK_PRINT,
        NULL, "<Ctrl>p",
        N_("Print the current page"), G_CALLBACK (_action_print_activate) },
    { "MailTo", NULL,
        N_("Send Page Link Via Email"), "<Ctrl>m",
        NULL, G_CALLBACK (_action_mail_to_activate) },
    { "Quit", GTK_STOCK_QUIT,
        N_("Close a_ll Windows"), "<Ctrl><Shift>q",
        NULL, G_CALLBACK (_action_quit_activate) },

    { "Edit", NULL, N_("_Edit"), NULL, NULL, G_CALLBACK (_action_edit_activate) },
    { "Undo", GTK_STOCK_UNDO,
        NULL, "<Ctrl>z",
        NULL, G_CALLBACK (_action_undo_activate) },
    { "Redo", GTK_STOCK_REDO,
        NULL, "<Ctrl><Shift>z",
        NULL, G_CALLBACK (_action_redo_activate) },
    { "Cut", GTK_STOCK_CUT,
        NULL, "<Ctrl>x",
        NULL, G_CALLBACK (_action_cut_activate) },
    { "Copy", GTK_STOCK_COPY,
        NULL, "<Ctrl>c",
        NULL, G_CALLBACK (_action_copy_activate) },
    { "Paste", GTK_STOCK_PASTE,
        NULL, "<Ctrl>v",
        NULL, G_CALLBACK (_action_paste_activate) },
    { "Delete", GTK_STOCK_DELETE,
        NULL, NULL,
        NULL, G_CALLBACK (_action_delete_activate) },
    { "SelectAll", GTK_STOCK_SELECT_ALL,
        NULL, "<Ctrl>a",
        NULL, G_CALLBACK (_action_select_all_activate) },
    { "Find", GTK_STOCK_FIND,
        N_("_Find…"), "<Ctrl>f",
        N_("Find a word or phrase in the page"), G_CALLBACK (_action_find_activate) },
    { "FindNext", GTK_STOCK_GO_FORWARD,
        N_("Find _Next"), "<Ctrl>g",
        NULL, G_CALLBACK (_action_find_next_activate) },
    { "FindPrevious", GTK_STOCK_GO_BACK,
        N_("Find _Previous"), "<Ctrl><Shift>g",
        NULL, G_CALLBACK (_action_find_previous_activate) },
    { "Preferences", GTK_STOCK_PREFERENCES,
        NULL, "<Ctrl><Alt>p",
        N_("Configure the application preferences"), G_CALLBACK (_action_preferences_activate) },

    { "View", NULL, N_("_View") },
    { "Toolbars", NULL, N_("_Toolbars") },
    { "Reload", GTK_STOCK_REFRESH,
        NULL, "<Ctrl>r",
        N_("Reload the current page"), G_CALLBACK (_action_reload_stop_activate) },
    { "ReloadUncached", GTK_STOCK_REFRESH,
        N_("Reload page without caching"), "<Ctrl><Shift>r",
        NULL, G_CALLBACK (_action_reload_stop_activate) },
    { "Stop", GTK_STOCK_STOP,
        NULL, "Escape",
        N_("Stop loading the current page"), G_CALLBACK (_action_reload_stop_activate) },
    { "ReloadStop", GTK_STOCK_STOP,
        NULL, "",
        N_("Reload the current page"), G_CALLBACK (_action_reload_stop_activate) },
    { "ZoomIn", GTK_STOCK_ZOOM_IN,
        NULL, "<Ctrl>plus",
        N_("Increase the zoom level"), G_CALLBACK (_action_zoom_activate) },
    { "ZoomOut", GTK_STOCK_ZOOM_OUT,
        NULL, "<Ctrl>minus",
        N_("Decrease the zoom level"), G_CALLBACK (_action_zoom_activate) },
    { "ZoomNormal", GTK_STOCK_ZOOM_100,
        NULL, "<Ctrl>0",
        NULL, G_CALLBACK (_action_zoom_activate) },
    { "Encoding", NULL, N_("_Encoding") },
    { "SourceView", NULL,
        N_("View So_urce"), "<Ctrl><Alt>U",
        NULL, G_CALLBACK (_action_source_view_activate) },
    { "SourceViewDom", NULL,
        N_("View _DOM Source"), "<Ctrl><Alt><Shift>U",
        NULL, G_CALLBACK (_action_source_view_dom_activate) },
    { "CaretBrowsing", NULL,
        N_("Ca_ret Browsing"), "F7",
        NULL, G_CALLBACK (_action_caret_browsing_activate) },
    { "Fullscreen", GTK_STOCK_FULLSCREEN,
        NULL, "F11",
        N_("Toggle fullscreen view"), G_CALLBACK (_action_fullscreen_activate) },
    { "ScrollLeft", NULL,
        N_("Scroll _Left"), "h",
        NULL, G_CALLBACK (_action_scroll_somewhere_activate) },
    { "ScrollDown", NULL,
        N_("Scroll _Down"), "j",
        NULL, G_CALLBACK (_action_scroll_somewhere_activate) },
    { "ScrollUp", NULL,
        N_("Scroll _Up"), "k",
        NULL, G_CALLBACK (_action_scroll_somewhere_activate) },
    { "ScrollRight", NULL,
        N_("Scroll _Right"), "l",
        NULL, G_CALLBACK (_action_scroll_somewhere_activate) },
    { "Readable", NULL,
       N_("_Readable"), "<Ctrl><Alt>R",
        NULL, G_CALLBACK (_action_readable_activate) },

    { "Go", NULL, N_("_Go") },
    { "Back", GTK_STOCK_GO_BACK,
        NULL, "<Alt>Left",
        N_("Go back to the previous page"), G_CALLBACK (_action_navigation_activate) },
    { "Forward", GTK_STOCK_GO_FORWARD,
        NULL, "<Alt>Right",
        N_("Go forward to the next page"), G_CALLBACK (_action_navigation_activate) },
#if 1   //zgh
    { "Previous", GTK_STOCK_MEDIA_PREVIOUS,
        NULL, "<Alt><Shift>Left",
        /* i18n: Visit the previous logical page, ie. in a forum or blog */
        N_("Go to the previous sub-page"), G_CALLBACK (_action_navigation_activate) },
    { "Next", GTK_STOCK_MEDIA_NEXT,
        NULL, "<Alt><Shift>Right",
        /* i18n: Visit the following logical page, ie. in a forum or blog */
        N_("Go to the next sub-page"), G_CALLBACK (_action_navigation_activate) },
    { "NextForward", GTK_STOCK_MEDIA_NEXT,
        N_("Next or Forward"), "",
        N_("Go to the next sub-page or next page in history"), G_CALLBACK (_action_navigation_activate) },
#endif
    { "Homepage", GTK_STOCK_HOME,
        N_("_Homepage"), "<Alt>Home",
        N_("Go to your homepage"), G_CALLBACK (_action_navigation_activate) },
    { "TrashEmpty", GTK_STOCK_CLEAR,
        N_("Empty Trash"), "",
        NULL, G_CALLBACK (_action_trash_empty_activate) },
    { "UndoTabClose", GTK_STOCK_UNDELETE,
        N_("Undo _Close Tab"), "<Ctrl><Shift>t",
        NULL, G_CALLBACK (_action_undo_tab_close_activate) },

    { "BookmarkAdd", STOCK_BOOKMARK_ADD,
        NULL, "<Ctrl>d",
        N_("Add a new bookmark"), G_CALLBACK (_action_bookmark_add_activate) },
    { "BookmarkFolderAdd", NULL,
        N_("Add a new _folder"), "",
        NULL, G_CALLBACK (_action_bookmark_add_activate) },
    { "BookmarksImport", NULL,
        N_("_Import bookmarks…"), "",
        NULL, G_CALLBACK (_action_bookmarks_import_activate) },
    { "BookmarksExport", NULL,
        N_("_Export bookmarks…"), "",
        NULL, G_CALLBACK (_action_bookmarks_export_activate) },
    { "ManageSearchEngines", GTK_STOCK_PROPERTIES,
        N_("_Manage Search Engines…"), "<Ctrl><Alt>s",
        NULL, G_CALLBACK (_action_manage_search_engines_activate) },
    { "ClearPrivateData", NULL,
        N_("_Clear Private Data…"), "<Ctrl><Shift>Delete",
        NULL, G_CALLBACK (_action_clear_private_data_activate) },
    { "InspectPage", NULL,
        N_("_Inspect Page"), "<Ctrl><Shift>i",
        NULL, G_CALLBACK (_action_inspect_page_activate) },

    { "TabPrevious", GTK_STOCK_GO_BACK,
        N_("_Previous Tab"), "<Ctrl>Page_Up",
        NULL, G_CALLBACK (_action_tab_previous_activate) },
    { "TabNext", GTK_STOCK_GO_FORWARD,
        N_("_Next Tab"), "<Ctrl>Page_Down",
        NULL, G_CALLBACK (_action_tab_next_activate) },
    { "TabMoveFirst", NULL, N_("Move Tab to _first position"), NULL,
       NULL, G_CALLBACK (_action_tab_move_activate) },
    { "TabMoveBackward", NULL, N_("Move Tab _Backward"), "<Ctrl><Shift>Page_Up",
       NULL, G_CALLBACK (_action_tab_move_activate) },
    { "TabMoveForward", NULL, N_("_Move Tab Forward"), "<Ctrl><Shift>Page_Down",
       NULL, G_CALLBACK (_action_tab_move_activate) },
    { "TabMoveLast", NULL, N_("Move Tab to _last position"), NULL,
       NULL, G_CALLBACK (_action_tab_move_activate) },
    { "TabCurrent", NULL,
        N_("Focus _Current Tab"), "<Ctrl><Alt>Home",
        NULL, G_CALLBACK (_action_tab_current_activate) },
    { "NextView", NULL,
        N_("Focus _Next view"), "F6",
        NULL, G_CALLBACK (_action_next_view_activate) },
    { "TabMinimize", NULL,
        N_("Only show the Icon of the _Current Tab"), "",
        NULL, G_CALLBACK (_action_tab_minimize_activate) },
    { "TabDuplicate", NULL,
        N_("_Duplicate Current Tab"), "",
        NULL, G_CALLBACK (_action_tab_duplicate_activate) },
    { "TabCloseOther", NULL,
        N_("Close Ot_her Tabs"), "",
        NULL, G_CALLBACK (_action_tab_close_other_activate) },
    { "LastSession", NULL,
        N_("Open last _session"), NULL,
        NULL, NULL },

    { "Help", NULL, N_("_Help") },
    { "HelpFAQ", GTK_STOCK_HELP,
        N_("_Frequent Questions"), "F1",
        NULL, G_CALLBACK (_action_help_link_activate) },
    { "HelpBugs", NULL,
        N_("_Report a Problem…"), NULL,
        NULL, G_CALLBACK (_action_help_link_activate) },
    { "About", GTK_STOCK_ABOUT,
        NULL, "",
        NULL, G_CALLBACK (_action_about_activate) },
    { "Dummy", NULL, N_("_Tools") },
};
static const guint entries_n = G_N_ELEMENTS (entries);

static const GtkToggleActionEntry toggle_entries[] =
{
    { "Menubar", NULL,
        N_("_Menubar"), "",
        NULL, G_CALLBACK (_action_menubar_activate),
        FALSE },
    { "Navigationbar", NULL,
        N_("_Navigationbar"), "",
        NULL, G_CALLBACK (_action_navigationbar_activate),
        FALSE },
    { "Panel", GTK_STOCK_INDENT,
        N_("Side_panel"), "F9",
        N_("Sidepanel"), G_CALLBACK (_action_panel_activate),
        FALSE },
    { "Bookmarkbar", NULL,
        N_("_Bookmarkbar"), "",
        NULL, G_CALLBACK (_action_bookmarkbar_activate),
        FALSE },
    { "Statusbar", NULL,
        N_("_Statusbar"), "<Ctrl>j",
        NULL, G_CALLBACK (_action_statusbar_activate),
        FALSE },
};
static const guint toggle_entries_n = G_N_ELEMENTS (toggle_entries);

static const GtkRadioActionEntry encoding_entries[] =
{
    { "EncodingAutomatic", NULL,
        N_("_Automatic"), "",
        NULL, 1 },
    { "EncodingChinese", NULL,
        N_("Chinese Traditional (BIG5)"), "",
        NULL, 1 },
    { "EncodingChineseSimplified", NULL,
        N_("Chinese Simplified (GB18030)"), "",
        NULL, 1 },
    { "EncodingJapanese", NULL,
        /* i18n: A double underscore "__" is used to prevent the mnemonic */
        N_("Japanese (SHIFT__JIS)"), "",
        NULL, 1 },
    { "EncodingKorean", NULL,
        N_("Korean (EUC-KR)"), "",
        NULL, 1 },
    { "EncodingRussian", NULL,
        N_("Russian (KOI8-R)"), "",
        NULL, 1 },
    { "EncodingUnicode", NULL,
        N_("Unicode (UTF-8)"), "",
        NULL, 1 },
    { "EncodingWestern", NULL,
        N_("Western (ISO-8859-1)"), "",
        NULL, 1 },
    { "EncodingCustom", NULL,
        N_("Custom…"), "",
        NULL, 1 },
};
static const guint encoding_entries_n = G_N_ELEMENTS (encoding_entries);

typedef struct {
     MidoriBrowser* browser;
     guint timeout;
} MidoriInactivityTimeout;

static gboolean
midori_inactivity_timeout (gpointer data)
{
    #ifdef HAVE_X11_EXTENSIONS_SCRNSAVER_H
    MidoriInactivityTimeout* mit = data;
    static Display* xdisplay = NULL;
    static XScreenSaverInfo* mit_info = NULL;
    static int has_extension = -1;
    int event_base, error_base;

    if (has_extension == -1)
    {
        GdkDisplay* display = gtk_widget_get_display (GTK_WIDGET (mit->browser));
        if (GDK_IS_X11_DISPLAY (display))
        {
            xdisplay = GDK_DISPLAY_XDISPLAY (display);
            has_extension = XScreenSaverQueryExtension (xdisplay, &event_base, &error_base);
        }
        else
            has_extension = 0;
    }

    if (has_extension)
    {
        if (!mit_info)
            mit_info = XScreenSaverAllocInfo ();
        XScreenSaverQueryInfo (xdisplay, RootWindow (xdisplay, 0), mit_info);
        if (mit_info->idle / 1000 > mit->timeout)
        {
            midori_private_data_clear_all (mit->browser);
            midori_browser_activate_action (mit->browser, "Homepage");
        }
    }
    #else
    /* TODO: Implement for other windowing systems */
    #endif

    return TRUE;
}

void
midori_browser_set_inactivity_reset (MidoriBrowser* browser,
                                     gint           inactivity_reset)
{
    if (inactivity_reset > 0)
    {
        MidoriInactivityTimeout* mit = g_new (MidoriInactivityTimeout, 1);
        mit->browser = browser;
        mit->timeout = inactivity_reset;
        midori_timeout_add_seconds (
            inactivity_reset, midori_inactivity_timeout, mit, NULL);
    }
}

static gboolean
midori_browser_window_state_event_cb (MidoriBrowser*       browser,
                                      GdkEventWindowState* event)
{
    MidoriWindowState window_state = MIDORI_WINDOW_NORMAL;
    if (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED)
        window_state = MIDORI_WINDOW_MINIMIZED;
    else if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
        window_state = MIDORI_WINDOW_MAXIMIZED;
    else if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
        window_state = MIDORI_WINDOW_FULLSCREEN;
    g_object_set (browser->settings, "last-window-state", window_state, NULL);

    return FALSE;
}

static gboolean
midori_browser_alloc_timeout (MidoriBrowser* browser)
{
    GtkWidget* widget = GTK_WIDGET (browser);
    GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (widget));

    if (!(state &
        (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)))
    {
        GtkAllocation allocation;
        gtk_widget_get_allocation (widget, &allocation);
        if (allocation.width != browser->last_window_width)
        {
            browser->last_window_width = allocation.width;
            g_object_set (browser->settings,
                "last-window-width", browser->last_window_width, NULL);
        }
        if (allocation.height != browser->last_window_height)
        {
            browser->last_window_height = allocation.height;
            g_object_set (browser->settings,
                "last-window-height", allocation.height, NULL);
        }
    }

    browser->alloc_timeout = 0;
    return FALSE;
}

static void
midori_browser_size_allocate_cb (MidoriBrowser* browser,
                                 GtkAllocation* allocation)
{
    GtkWidget* widget = GTK_WIDGET (browser);

    if (!browser->alloc_timeout && gtk_widget_get_realized (widget))
    {
        gpointer last_page;

        if ((last_page = g_object_get_data (G_OBJECT (browser), "last-page")))
        {
            midori_panel_set_current_page (MIDORI_PANEL (browser->panel),
                GPOINTER_TO_INT (last_page));
            g_object_set_data (G_OBJECT (browser), "last-page", NULL);
        }

        browser->alloc_timeout = midori_timeout_add_seconds (5,
            (GSourceFunc)midori_browser_alloc_timeout, browser, NULL);
    }
}

static void
midori_browser_destroy_cb (MidoriBrowser* browser)
{

    g_object_set_data (G_OBJECT (browser), "midori-browser-destroyed", (void*)1);

    if (G_UNLIKELY (browser->panel_timeout))
        g_source_remove (browser->panel_timeout);
    if (G_UNLIKELY (browser->alloc_timeout))
        g_source_remove (browser->alloc_timeout);

    /* Destroy panel first, so panels don't need special care */
    gtk_widget_destroy (browser->panel);
    /* Destroy tabs second, so child widgets don't need special care */
    gtk_container_foreach (GTK_CONTAINER (browser->notebook),
                           (GtkCallback) gtk_widget_destroy, NULL);
}

static const gchar* ui_markup =
    "<ui>"
        "<menubar>"
            "<menu action='File'>"
                "<menuitem action='WindowNew'/>"
                "<menuitem action='TabNew'/>"
                "<menuitem action='PrivateBrowsing'/>"
                "<separator/>"
                "<menuitem action='Open'/>"
                "<separator/>"
#if 0 //zgh
                "<menuitem action='SaveAs'/>"
                "<menuitem action='AddSpeedDial'/>"
                "<separator/>"
                "<menuitem action='TabClose'/>"
                "<menuitem action='WindowClose'/>"
                "<separator/>"
                "<menuitem action='MailTo'/>"
#endif
                "<menuitem action='Print'/>"
                "<separator/>"
                "<menuitem action='Quit'/>"
            "</menu>"
            "<menu action='Edit'>"
#if 0   //zgh
                "<menuitem action='Undo'/>"
                "<menuitem action='Redo'/>"
#endif
                "<separator/>"
                "<menuitem action='Cut'/>"
                "<menuitem action='Copy'/>"
                "<menuitem action='Paste'/>"
                "<menuitem action='Delete'/>"
                "<separator/>"
                "<menuitem action='SelectAll'/>"
                "<separator/>"
                "<menuitem action='Find'/>"
//                "<menuitem action='FindNext'/>"   //zgh
                #ifndef G_OS_WIN32
                "<separator/>"
                "<menuitem action='Preferences'/>"
                #endif
            "</menu>"
            "<menu action='View'>"
                "<menu action='Toolbars'>"
                    "<menuitem action='Menubar'/>"
                    "<menuitem action='Navigationbar'/>"
                    "<menuitem action='Bookmarkbar'/>"
                    "<menuitem action='Statusbar'/>"
                "</menu>"
#if 0 //zgh
                "<menuitem action='Panel'/>"
                "<separator/>"
                "<menuitem action='Stop'/>"
                "<menuitem action='Reload'/>"
                "<separator/>"
#endif
                "<menuitem action='ZoomIn'/>"
                "<menuitem action='ZoomOut'/>"
                "<menuitem action='ZoomNormal'/>"
                "<separator/>"
                "<menu action='Encoding'>"
                    "<menuitem action='EncodingAutomatic'/>"
                    "<menuitem action='EncodingChinese'/>"
                    "<menuitem action='EncodingChineseSimplified'/>"
                    "<menuitem action='EncodingJapanese'/>"
                    "<menuitem action='EncodingKorean'/>"
                    "<menuitem action='EncodingRussian'/>"
                    "<menuitem action='EncodingUnicode'/>"
                    "<menuitem action='EncodingWestern'/>"
                    "<menuitem action='EncodingCustom'/>"
                "</menu>"
                "<menuitem action='SourceView'/>"
//                "<menuitem action='SourceViewDom'/>"  //zgh
                "<menuitem action='Fullscreen'/>"
//                "<menuitem action='Readable'/>"   //zgh
            "</menu>"
#if 0   //zgh 
            "<menu action='Go'>"
                "<menuitem action='Back'/>"
                "<menuitem action='Forward'/>"
                "<menuitem action='Previous'/>"
                "<menuitem action='Next'/>"
                "<menuitem action='Homepage'/>"
                "<menuitem action='Location'/>"
                "<menuitem action='Search'/>"
                "<menuitem action='Trash'/>"
            "</menu>"
#endif
            //todo add 历史菜单
            "<menu action='Bookmarks'>"
                "<menuitem action='BookmarksAdd'/>"
                "<menuitem action='BookmarksImport'/>"
                "<menuitem action='BookmarksExport'/>"
            "</menu>"
            "<menuitem action='Tools'/>"
//            "<menuitem action='Window'/>" //zgh
            "<menu action='Help'>"
//                "<menuitem action='HelpFAQ'/>"    //zgh
                "<menuitem action='HelpBugs'/>"
                "<separator/>"
                "<menuitem action='About'/>"
        "</menu>"
        /* For accelerators to work all actions need to be used
           *somewhere* in the UI definition */
        /* These also show up in Unity's HUD */
        "<menu action='Dummy'>"
            "<menuitem action='TabMoveFirst'/>"
            "<menuitem action='TabMoveBackward'/>"
            "<menuitem action='TabMoveForward'/>"
            "<menuitem action='TabMoveLast'/>"
            "<menuitem action='ScrollLeft'/>"
            "<menuitem action='ScrollDown'/>"
            "<menuitem action='ScrollUp'/>"
            "<menuitem action='ScrollRight'/>"
            "<menuitem action='FindPrevious'/>"
            "<menuitem action='BookmarkAdd'/>"
            "<menuitem action='BookmarkFolderAdd'/>"
            "<menuitem action='ManageSearchEngines'/>"
            "<menuitem action='ClearPrivateData'/>"
            "<menuitem action='TabPrevious'/>"
            "<menuitem action='TabNext'/>"
            "<menuitem action='TabCurrent'/>"
            "<menuitem action='NextView'/>"
            "<menuitem action='TabMinimize'/>"
            "<menuitem action='TabDuplicate'/>"
            "<menuitem action='TabCloseOther'/>"
            "<menuitem action='LastSession'/>"
            "<menuitem action='UndoTabClose'/>"
            "<menuitem action='TrashEmpty'/>"
            "<menuitem action='Preferences'/>"
            "<menuitem action='InspectPage'/>"
            "<menuitem action='ReloadUncached'/>"
            "<menuitem action='CaretBrowsing'/>"
            "</menu>"
        "</menubar>"
        "<toolbar name='toolbar_navigation'>"
        "</toolbar>"
    "</ui>";

static void
midori_browser_realize_cb (GtkStyle*      style,
                           MidoriBrowser* browser)
{
    GdkScreen* screen = gtk_widget_get_screen (GTK_WIDGET (browser));
    if (screen)
    {
        GtkIconTheme* icon_theme = gtk_icon_theme_get_for_screen (screen);
        if (gtk_icon_theme_has_icon (icon_theme, "midori"))
            gtk_window_set_icon_name (GTK_WINDOW (browser), "midori");
        else
            gtk_window_set_icon_name (GTK_WINDOW (browser), MIDORI_STOCK_WEB_BROWSER);
    }
}

static void
midori_browser_set_history (MidoriBrowser* browser,
                            KatzeArray*    history)
{
    if (browser->history == history)
        return;

    if (history)
        g_object_ref (history);
    katze_object_assign (browser->history, history);
    katze_object_assign (browser->history_database, NULL);

    if (!history)
        return;

    GError* error = NULL;
    browser->history_database = midori_history_database_new (NULL, &error);
    if (error != NULL)
    {
        g_printerr (_("Failed to initialize history: %s"), error->message);
        g_printerr ("\n");
        g_error_free (error);
        return;
    }
    g_object_set (_action_by_name (browser, "Location"), "history",
                  browser->history, NULL);
}

static void
midori_browser_accel_switch_tab_activate_cb (GtkAccelGroup*  accel_group,
                                             GObject*        acceleratable,
                                             guint           keyval,
                                             GdkModifierType modifiers)
{
    GtkAccelGroupEntry* entry;

    if ((entry = gtk_accel_group_query (accel_group, keyval, modifiers, NULL)))
    {
        gint n;
        MidoriBrowser* browser;
        GtkWidget* view;

        /* Switch to n-th tab. 9 and 0 go to the last tab. */
        n = keyval - GDK_KEY_0;
        browser = g_object_get_data (G_OBJECT (accel_group), "midori-browser");
        if ((view = midori_browser_get_nth_tab (browser, n < 9 ? n - 1 : -1)))
            midori_browser_set_current_tab (browser, view);
    }
}

static void
midori_browser_add_actions (MidoriBrowser* browser)
{
    /* 0,053 versus 0,002 compared to gtk_action_group_add_ API */
    guint i;
    GSList* group = NULL;
    for (i = 0; i < G_N_ELEMENTS (entries); i++)
    {
        GtkActionEntry entry = entries[i];
        GtkAction* action = gtk_action_new (entry.name,
            _(entry.label), _(entry.tooltip), entry.stock_id);
        if (entry.callback)
            g_signal_connect (action, "activate", entry.callback, browser);
        gtk_action_group_add_action_with_accel (browser->action_group,
            GTK_ACTION (action), entry.accelerator);
    }
    for (i = 0; i < G_N_ELEMENTS (toggle_entries); i++)
    {
        GtkToggleActionEntry entry = toggle_entries[i];
        GtkToggleAction* action = gtk_toggle_action_new (entry.name,
            _(entry.label), _(entry.tooltip), entry.stock_id);
        if (entry.is_active)
            gtk_toggle_action_set_active (action, TRUE);
        if (entry.callback)
            g_signal_connect (action, "activate", entry.callback, browser);
        gtk_action_group_add_action_with_accel (browser->action_group,
            GTK_ACTION (action), entry.accelerator);
    }
    for (i = 0; i < G_N_ELEMENTS (encoding_entries); i++)
    {
        GtkRadioActionEntry entry = encoding_entries[i];
        GtkRadioAction* action = gtk_radio_action_new (entry.name,
            _(entry.label), _(entry.tooltip), entry.stock_id, entry.value);
        if (i == 0)
        {
            group = gtk_radio_action_get_group (action);
            gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
            g_signal_connect (action, "changed",
                G_CALLBACK (_action_view_encoding_activate), browser);
        }
        else
        {
            gtk_radio_action_set_group (action, group);
            group = gtk_radio_action_get_group (action);
        }
        gtk_action_group_add_action_with_accel (browser->action_group,
            GTK_ACTION (action), entry.accelerator);
    }
}

static gboolean
midori_browser_idle (gpointer data)
{
    MidoriBrowser* browser = MIDORI_BROWSER (data);

    if (browser->bookmarkbar_populate)
    {
        midori_bookmarkbar_populate_idle (browser);

        browser->bookmarkbar_populate = FALSE;
    }

    return FALSE;
}

static void
midori_browser_init (MidoriBrowser* browser)
{
    GtkWidget* vbox;
    GtkUIManager* ui_manager;
    GtkAccelGroup* accel_group;
    guint i;
    GClosure* accel_closure;
    GError* error;
    GtkAction* action;
    GtkWidget* menuitem;
    GtkWidget* homepage;
    GtkWidget* back;
    GtkWidget* forward;
    GtkSettings* gtk_settings;
    GtkWidget* hpaned;
    GtkWidget* vpaned;
    GtkWidget* scrolled;
    KatzeArray* dummy_array;

    browser->settings = midori_web_settings_new ();
    browser->proxy_array = katze_array_new (KATZE_TYPE_ARRAY);
    browser->bookmarks = NULL;
    browser->history = NULL;
    browser->history_database = NULL;
    browser->trash = NULL;
    browser->search_engines = NULL;
    browser->dial = NULL;

    /* Setup the window metrics */
    g_signal_connect (browser, "realize",
                      G_CALLBACK (midori_browser_realize_cb), browser);
    g_signal_connect (browser, "window-state-event",
                      G_CALLBACK (midori_browser_window_state_event_cb), NULL);
    g_signal_connect (browser, "size-allocate",
                      G_CALLBACK (midori_browser_size_allocate_cb), NULL);
    g_signal_connect (browser, "destroy",
                      G_CALLBACK (midori_browser_destroy_cb), NULL);
    gtk_window_set_role (GTK_WINDOW (browser), "browser");
    gtk_window_set_icon_name (GTK_WINDOW (browser), MIDORI_STOCK_WEB_BROWSER);
    #if GTK_CHECK_VERSION (3, 4, 0)
    #ifndef HAVE_GRANITE
    gtk_window_set_hide_titlebar_when_maximized (GTK_WINDOW (browser), TRUE);
    #endif
    #endif
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (browser), vbox);
    gtk_widget_show (vbox);

    /* Let us see some ui manager magic */
    browser->action_group = gtk_action_group_new ("Browser");
    gtk_action_group_set_translation_domain (browser->action_group, GETTEXT_PACKAGE);
    midori_browser_add_actions (browser);
    ui_manager = gtk_ui_manager_new ();
    accel_group = gtk_ui_manager_get_accel_group (ui_manager);
    gtk_window_add_accel_group (GTK_WINDOW (browser), accel_group);
    gtk_ui_manager_insert_action_group (ui_manager, browser->action_group, 0);

    g_object_set_data (G_OBJECT (accel_group), "midori-browser", browser);
    accel_closure = g_cclosure_new (G_CALLBACK (
        midori_browser_accel_switch_tab_activate_cb), browser, NULL);
    for (i = 0; i < 10; i++)
    {
        gchar* accel_path = g_strdup_printf ("<Manual>/Browser/SwitchTab%d", i);
        gtk_accel_map_add_entry (accel_path, GDK_KEY_0 + i, GDK_MOD1_MASK);
        gtk_accel_group_connect_by_path (accel_group, accel_path, accel_closure);
        g_free (accel_path);
    }
    g_closure_unref (accel_closure);

    error = NULL;
    if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_markup, -1, &error))
    {
        g_message ("User interface couldn't be created: %s", error->message);
        g_error_free (error);
    }

    /* Hide the 'Dummy' which only holds otherwise unused actions */
    _action_set_visible (browser, "Dummy", FALSE);

    action = g_object_new (KATZE_TYPE_SEPARATOR_ACTION,
        "name", "Separator",
        "label", _("_Separator"),
        NULL);
    gtk_action_group_add_action (browser->action_group, action);
    g_object_unref (action);

    action = g_object_new (MIDORI_TYPE_LOCATION_ACTION,
        "name", "Location",
        "label", _("_Location…"),
        "stock-id", GTK_STOCK_JUMP_TO,
        "tooltip", _("Open a particular location"),
        "placeholder-text", _("Search or enter an address"),
        NULL);

    g_object_connect (action,
                      "signal::activate",
                      _action_location_activate, browser,
                      #if 0 //zgh
                      "signal::focus-in",
                      _action_location_focus_in, browser,
                      #endif
                      "signal::focus-out",
                      _action_location_focus_out, browser,
                      "signal::reset-uri",
                      _action_location_reset_uri, browser,
                      "signal::submit-uri",
                      _action_location_submit_uri, browser,
                      "signal-after::secondary-icon-released",
                      _action_location_secondary_icon_released, browser,
                      NULL);

    gtk_action_group_add_action_with_accel (browser->action_group,
        action, "<Ctrl>L");
    g_object_unref (action);

#if 0 //zgh 本地搜索
    action = g_object_new (MIDORI_TYPE_SEARCH_ACTION,
        "name", "Search",
        "label", _("_Web Search…"),
        "stock-id", GTK_STOCK_FIND,
        "tooltip", _("Run a web search"),
        NULL);
    g_object_connect (action,
                      "signal::activate",
                      _action_search_activate, browser,
                      "signal::submit",
                      _action_search_submit, browser,
                      "signal::focus-out",
                      _action_search_focus_out, browser,
                      "signal::notify::current-item",
                      _action_search_notify_current_item, browser,
                      "signal::notify::default-item",
                      _action_search_notify_default_item, browser,
                      NULL);
    gtk_action_group_add_action_with_accel (browser->action_group,
        action, "<Ctrl>K");
    g_object_unref (action);
#endif
    action = g_object_new (MIDORI_TYPE_PANED_ACTION,
        "name", "LocationSearch",
        NULL);
    gtk_action_group_add_action (browser->action_group, action);
    g_object_unref (action);

#if 0 //zgh 垃圾桶
    action = g_object_new (KATZE_TYPE_ARRAY_ACTION,
        "name", "Trash",
        "stock-id", STOCK_USER_TRASH,
        "tooltip", _("Reopen a previously closed tab or window"),
        NULL);
    g_object_connect (action,
                      "signal::populate-popup",
                      _action_trash_populate_popup, browser,
                      "signal::activate-item-alt",
                      _action_trash_activate_item_alt, browser,
                      NULL);
    gtk_action_group_add_action_with_accel (browser->action_group, action, "");
    g_object_unref (action);
#endif
    dummy_array = katze_array_new (KATZE_TYPE_ARRAY);
    katze_array_update (dummy_array);
    action = g_object_new (KATZE_TYPE_ARRAY_ACTION,
        "name", "Bookmarks",
        "label", _("_Bookmarks"),
        "stock-id", STOCK_BOOKMARKS,
        "tooltip", _("Show the saved bookmarks"),
        "array", dummy_array /* updated, unique */,
        NULL);
    g_object_connect (action,
                      "signal::populate-folder",
                      _action_bookmarks_populate_folder, browser,
                      "signal::activate-item-alt",
                      midori_bookmarkbar_activate_item_alt, browser,
                      "signal::activate-item",
                      midori_bookmarkbar_activate_item, browser,
                      NULL);
    gtk_action_group_add_action_with_accel (browser->action_group, action, "");
    g_object_unref (action);
    g_object_unref (dummy_array);

    dummy_array = katze_array_new (KATZE_TYPE_ITEM);
    katze_array_update (dummy_array);
    action = g_object_new (KATZE_TYPE_ARRAY_ACTION,
        "name", "Tools",
        "label", _("_Tools"),
        "stock-id", GTK_STOCK_PREFERENCES,
        "array", dummy_array /* updated, unique */,
        NULL);
    g_object_connect (action,
                      "signal::populate-popup",
                      _action_tools_populate_popup, browser,
                      NULL);
    gtk_action_group_add_action (browser->action_group, action);
    g_object_unref (action);
    g_object_unref (dummy_array);

    action = g_object_new (KATZE_TYPE_ARRAY_ACTION,
        "name", "Window",
        "label", _("_Tabs"),
        "stock-id", GTK_STOCK_INDEX,
        "tooltip", _("Show a list of all open tabs"),
        "array", browser->proxy_array,
        NULL);
    g_object_connect (action,
                      "signal::populate-popup",
                      _action_window_populate_popup, browser,
                      "signal::activate-item-alt",
                      _action_window_activate_item_alt, browser,
                      NULL);
    gtk_action_group_add_action_with_accel (browser->action_group, action, "");
    g_object_unref (action);

    action = g_object_new (KATZE_TYPE_ARRAY_ACTION,
        "name", "CompactMenu",
        "label", _("_Menu"),
        "stock-id", GTK_STOCK_PROPERTIES,
        "tooltip", _("Menu"),
        "array", katze_array_new (KATZE_TYPE_ITEM),
        NULL);
    g_object_connect (action,
                      "signal::populate-popup",
                      _action_compact_menu_populate_popup, browser,
                      NULL);
    gtk_action_group_add_action (browser->action_group, action);
    g_object_unref (action);

    /* Create the menubar */
    browser->menubar = gtk_ui_manager_get_widget (ui_manager, "/menubar");
    gtk_box_pack_start (GTK_BOX (vbox), browser->menubar, FALSE, FALSE, 0);
    gtk_widget_hide (browser->menubar);
    _action_set_visible (browser, "Menubar", !midori_browser_has_native_menubar ());
    g_signal_connect (browser->menubar, "button-press-event",
        G_CALLBACK (midori_browser_menu_button_press_event_cb), browser);
#if 0  //by zgh
    menuitem = gtk_menu_item_new ();
    gtk_widget_show (menuitem);
    browser->throbber = gtk_spinner_new ();
    /* Wrap the spinner in an event box to retain its size when hidden */
    GtkWidget* throbber_box = gtk_event_box_new ();
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (throbber_box), FALSE);
    gint icon_size = 16;
    gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (GTK_WIDGET (browser)),
                                       GTK_ICON_SIZE_MENU, &icon_size, NULL);
    gtk_widget_set_size_request (throbber_box, icon_size, icon_size);
    gtk_container_add (GTK_CONTAINER (throbber_box), browser->throbber);
    gtk_widget_show (throbber_box);
    gtk_container_add (GTK_CONTAINER (menuitem), throbber_box);
    #if GTK_CHECK_VERSION (3, 2, 0)
    /* FIXME: Doesn't work */
    gtk_widget_set_hexpand (menuitem, TRUE);
    gtk_widget_set_halign (menuitem, GTK_ALIGN_END);
    #else
    gtk_menu_item_set_right_justified (GTK_MENU_ITEM (menuitem), TRUE);
    #endif
    gtk_menu_shell_append (GTK_MENU_SHELL (browser->menubar), menuitem);
#endif
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (
        gtk_ui_manager_get_widget (ui_manager, "/menubar/File/WindowNew")), NULL);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (
        gtk_ui_manager_get_widget (ui_manager, "/menubar/Go/Location")), NULL);
#if 0 //zgh
    homepage = gtk_ui_manager_get_widget (ui_manager, "/menubar/Go/Homepage");
    g_signal_connect (homepage, "button-press-event",
        G_CALLBACK (midori_browser_menu_item_middle_click_event_cb), browser);
    back = gtk_ui_manager_get_widget (ui_manager, "/menubar/Go/Back");
    g_signal_connect (back, "button-press-event",
        G_CALLBACK (midori_browser_menu_item_middle_click_event_cb), browser);
    forward = gtk_ui_manager_get_widget (ui_manager, "/menubar/Go/Forward");
    g_signal_connect (forward, "button-press-event",
        G_CALLBACK (midori_browser_menu_item_middle_click_event_cb), browser);
    forward = gtk_ui_manager_get_widget (ui_manager, "/menubar/Go/Previous");
    g_signal_connect (forward, "button-press-event",
        G_CALLBACK (midori_browser_menu_item_middle_click_event_cb), browser);
    forward = gtk_ui_manager_get_widget (ui_manager, "/menubar/Go/Next");
    g_signal_connect (forward, "button-press-event",
        G_CALLBACK (midori_browser_menu_item_middle_click_event_cb), browser);
#endif
    _action_set_sensitive (browser, "EncodingCustom", FALSE);
    _action_set_visible (browser, "LastSession", FALSE);

    _action_set_visible (browser, "Bookmarks", browser->bookmarks != NULL);
    _action_set_visible (browser, "BookmarkAdd", browser->bookmarks != NULL);
    _action_set_visible (browser, "BookmarksImport", browser->bookmarks != NULL);
    _action_set_visible (browser, "BookmarksExport", browser->bookmarks != NULL);
    _action_set_visible (browser, "Bookmarkbar", browser->bookmarks != NULL);
    _action_set_visible (browser, "Trash", browser->trash != NULL);
    _action_set_visible (browser, "UndoTabClose", browser->trash != NULL);
    _action_set_visible (browser, "Forward", FALSE);    //zgh

    /* Create the navigationbar */
    browser->navigationbar = gtk_ui_manager_get_widget (
        ui_manager, "/toolbar_navigation");
    katze_widget_add_class (browser->navigationbar, "primary-toolbar");
    /* FIXME: Settings should be connected with screen changes */
    gtk_settings = gtk_widget_get_settings (GTK_WIDGET (browser));
    if (gtk_settings)
        g_signal_connect (gtk_settings, "notify::gtk-toolbar-style",
            G_CALLBACK (midori_browser_navigationbar_notify_style_cb), browser);
    gtk_toolbar_set_show_arrow (GTK_TOOLBAR (browser->navigationbar), TRUE);
//zgh    g_object_set (_action_by_name (browser, "Back"), "is-important", TRUE, NULL);  //工具栏上显示字符
    gtk_widget_hide (browser->navigationbar);
    g_signal_connect (browser->navigationbar, "popup-context-menu",
        G_CALLBACK (midori_browser_toolbar_popup_context_menu_cb), browser);
    gtk_box_pack_start (GTK_BOX (vbox), browser->navigationbar, FALSE, FALSE, 0);

    /* Bookmarkbar */
    browser->bookmarkbar = gtk_toolbar_new ();
    katze_widget_add_class (browser->bookmarkbar, "secondary-toolbar");
    gtk_widget_set_name (browser->bookmarkbar, "MidoriBookmarkbar");
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (browser->bookmarkbar),
                               GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style (GTK_TOOLBAR (browser->bookmarkbar),
                           GTK_TOOLBAR_BOTH_HORIZ);
    
    gtk_box_pack_start (GTK_BOX (vbox), browser->bookmarkbar, FALSE, FALSE, 0);
    g_signal_connect (browser->bookmarkbar, "popup-context-menu",
        G_CALLBACK (midori_browser_toolbar_popup_context_menu_cb), browser);

    /* Create the panel */
    hpaned = gtk_hpaned_new ();
    g_signal_connect (hpaned, "notify::position",
                      G_CALLBACK (midori_panel_notify_position_cb),
                      browser);
    g_signal_connect (hpaned, "cycle-child-focus",
                      G_CALLBACK (midori_panel_cycle_child_focus_cb),
                      browser);
    gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);
    gtk_widget_show (hpaned);
    browser->panel = g_object_new (MIDORI_TYPE_PANEL,
                                   "action-group", browser->action_group,
                                   NULL);
    g_object_connect (browser->panel,
        "signal::notify::page",
        midori_panel_notify_page_cb, browser,
        "signal::notify::show-titles",
        midori_panel_notify_show_titles_cb, browser,
        "signal::notify::right-aligned",
        midori_panel_notify_right_aligned_cb, browser,
        "signal::close",
        midori_panel_close_cb, browser,
        NULL);
    gtk_paned_pack1 (GTK_PANED (hpaned), browser->panel, FALSE, FALSE);

    /* Notebook, containing all views */
    vpaned = gtk_vpaned_new ();
    gtk_paned_pack2 (GTK_PANED (hpaned), vpaned, TRUE, FALSE);
    gtk_widget_show (vpaned);
    browser->notebook = midori_notebook_new ();

    gtk_paned_pack1 (GTK_PANED (vpaned), browser->notebook, FALSE, FALSE);
    g_signal_connect (browser->notebook, "tab-switched",
                      G_CALLBACK (midori_browser_switched_tab_cb),
                      browser);
    g_signal_connect (browser->notebook, "notify::tab",
                      G_CALLBACK (midori_browser_notify_tab_cb), browser);
    g_signal_connect (browser->notebook, "tab-moved",
                      G_CALLBACK (midori_browser_tab_moved_cb),
                      browser);
    g_signal_connect (browser->notebook, "context-menu",
        G_CALLBACK (midori_browser_notebook_context_menu_cb),
                      browser);
    g_signal_connect (browser->notebook, "tab-context-menu",
        G_CALLBACK (midori_browser_notebook_tab_context_menu_cb), browser);
    g_signal_connect (browser->notebook, "tab-detached",
                      G_CALLBACK (midori_browser_notebook_create_window_cb), browser);
    g_signal_connect (browser->notebook, "new-tab",
                      G_CALLBACK (midori_browser_notebook_new_tab_cb), browser);
    gtk_widget_show (browser->notebook);

    /* Inspector container */
    browser->inspector = gtk_vbox_new (FALSE, 0);
    gtk_paned_pack2 (GTK_PANED (vpaned), browser->inspector, FALSE, FALSE);
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_can_focus (scrolled, TRUE);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start (GTK_BOX (browser->inspector), scrolled, TRUE, TRUE, 0);
    browser->inspector_view = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolled), browser->inspector_view);

    /* Incremental findbar */
    browser->find = g_object_new (MIDORI_TYPE_FINDBAR, NULL);
    gtk_box_pack_start (GTK_BOX (vbox), browser->find, FALSE, FALSE, 0);

    /* Statusbar */
    browser->statusbar = gtk_statusbar_new ();
    browser->statusbar_contents =
        gtk_statusbar_get_message_area (GTK_STATUSBAR (browser->statusbar));
    gtk_box_pack_start (GTK_BOX (vbox), browser->statusbar, FALSE, FALSE, 0);

    g_signal_connect (browser->statusbar, "button-press-event",
        G_CALLBACK (midori_browser_menu_button_press_event_cb), browser);

    g_object_unref (ui_manager);
}

static void
midori_browser_dispose (GObject* object)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);

    /* We are done, the session mustn't change anymore */
    katze_object_assign (browser->proxy_array, NULL);
    g_signal_handlers_disconnect_by_func (browser->settings,
                                          midori_browser_settings_notify,
                                          browser);
    midori_browser_set_bookmarks (browser, NULL);
    midori_browser_set_history (browser, NULL);

    G_OBJECT_CLASS (midori_browser_parent_class)->dispose (object);
}

static void
midori_browser_finalize (GObject* object)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);

    katze_assign (browser->statusbar_text, NULL);

    katze_object_assign (browser->settings, NULL);
    katze_object_assign (browser->trash, NULL);
    katze_object_assign (browser->search_engines, NULL);
    katze_object_assign (browser->history, NULL);
    katze_object_assign (browser->history_database, NULL);
    katze_object_assign (browser->dial, NULL);

    g_idle_remove_by_data (browser);

    G_OBJECT_CLASS (midori_browser_parent_class)->finalize (object);
}

static void
_midori_browser_set_toolbar_style (MidoriBrowser*     browser,
                                   MidoriToolbarStyle toolbar_style)
{
    GtkToolbarStyle gtk_toolbar_style;
    GtkIconSize icon_size;
    GtkSettings* gtk_settings = gtk_widget_get_settings (GTK_WIDGET (browser));
    g_object_get (gtk_settings, "gtk-toolbar-icon-size", &icon_size, NULL);
    if (toolbar_style == MIDORI_TOOLBAR_DEFAULT && gtk_settings)
        g_object_get (gtk_settings, "gtk-toolbar-style", &gtk_toolbar_style, NULL);
    else
    {
        switch (toolbar_style)
        {
        case MIDORI_TOOLBAR_SMALL_ICONS:
            icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR;
        case MIDORI_TOOLBAR_ICONS:
            gtk_toolbar_style = GTK_TOOLBAR_ICONS;
            break;
        case MIDORI_TOOLBAR_TEXT:
            gtk_toolbar_style = GTK_TOOLBAR_TEXT;
            break;
        case MIDORI_TOOLBAR_BOTH:
            gtk_toolbar_style = GTK_TOOLBAR_BOTH;
            break;
        case MIDORI_TOOLBAR_BOTH_HORIZ:
        case MIDORI_TOOLBAR_DEFAULT:
            gtk_toolbar_style = GTK_TOOLBAR_BOTH_HORIZ;
        }
    }
    gtk_toolbar_set_style (GTK_TOOLBAR (browser->navigationbar),
                           gtk_toolbar_style);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (browser->navigationbar), icon_size);
    midori_panel_set_toolbar_style (MIDORI_PANEL (browser->panel),
                                    gtk_toolbar_style);
}

#ifndef HAVE_WEBKIT2
static void
midori_browser_toolbar_popup_context_menu_history_cb (GtkMenuItem* menu_item,
                                                      MidoriBrowser* browser)
{
    gint steps = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "steps"));
    MidoriView* view = MIDORI_VIEW (midori_browser_get_current_tab (browser));
    midori_view_go_back_or_forward (view, steps);
}
#endif

static void
midori_browser_toolbar_popup_context_menu_history (MidoriBrowser* browser,
                                                   GtkWidget* widget,
                                                   gboolean back,
                                                   gint x,
                                                   gint y)
{
#ifndef HAVE_WEBKIT2
    const gint step = back ? -1 : 1;
    gint steps = step;
    GtkWidget* menu;
    WebKitWebBackForwardList* list;
    WebKitWebHistoryItem* current_item;
    WebKitWebHistoryItem* history_item;
    WebKitWebHistoryItem* (*history_next)(WebKitWebBackForwardList*);
    void (*history_action)(WebKitWebBackForwardList*);

    list = webkit_web_view_get_back_forward_list (
        WEBKIT_WEB_VIEW (midori_view_get_web_view (
        MIDORI_VIEW (midori_browser_get_current_tab (browser)))));

    if (!list)
        return;

    menu = gtk_menu_new ();

    history_action = back ?
        webkit_web_back_forward_list_go_back :
        webkit_web_back_forward_list_go_forward;
    history_next = back ?
        webkit_web_back_forward_list_get_back_item :
        webkit_web_back_forward_list_get_forward_item;
    current_item = webkit_web_back_forward_list_get_current_item (list);

    for (; (history_item = history_next (list)); history_action (list), steps += step)
    {
        const gchar* uri = webkit_web_history_item_get_uri (history_item);
        GtkWidget* menu_item = gtk_image_menu_item_new_with_label (
            webkit_web_history_item_get_title (history_item));
        GdkPixbuf* pixbuf;
        if ((pixbuf = midori_paths_get_icon (uri, widget)))
        {
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
                gtk_image_new_from_pixbuf (pixbuf));
            g_object_unref (pixbuf);
        }
        g_object_set_data (G_OBJECT (menu_item), "uri", (gpointer)uri);
        g_object_set_data (G_OBJECT (menu_item), "steps", GINT_TO_POINTER (steps));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
        g_signal_connect (G_OBJECT (menu_item), "activate",
            G_CALLBACK (midori_browser_toolbar_popup_context_menu_history_cb),
            browser);
        if (steps == (10 - 1))
            break;
    }

    webkit_web_back_forward_list_go_to_item (list, current_item);
    gtk_widget_show_all (menu);

    katze_widget_popup (widget, GTK_MENU (menu), NULL,
        KATZE_MENU_POSITION_LEFT);
#endif
}

static gboolean
midori_browser_toolbar_item_button_press_event_cb (GtkWidget*      toolitem,
                                                   GdkEventButton* event,
                                                   MidoriBrowser*  browser)
{
    if (MIDORI_EVENT_NEW_TAB (event))
    {
        /* check if the middle-click was performed over reload button */
        if (g_object_get_data (G_OBJECT (toolitem), "reload-middle-click"))
        {
            gtk_action_activate (_action_by_name (browser, "TabDuplicate"));
        }

        GtkWidget* parent = gtk_widget_get_parent (toolitem);
        GtkAction* action = gtk_activatable_get_related_action (
            GTK_ACTIVATABLE (parent));

        g_object_set_data (G_OBJECT (action),
                           "midori-middle-click",
                           GINT_TO_POINTER (1));

        return _action_navigation_activate (action, browser);
    }
    else if (MIDORI_EVENT_CONTEXT_MENU (event))
    {
        if (g_object_get_data (G_OBJECT (toolitem), "history-back"))
        {
            midori_browser_toolbar_popup_context_menu_history (
                browser,
                GTK_IS_BIN (toolitem) && gtk_bin_get_child (GTK_BIN (toolitem)) ?
                gtk_widget_get_parent (toolitem) : toolitem,
                TRUE, event->x, event->y);
        }
        else if (g_object_get_data (G_OBJECT (toolitem), "history-forward"))
        {
            midori_browser_toolbar_popup_context_menu_history (
                browser,
                GTK_IS_BIN (toolitem) && gtk_bin_get_child (GTK_BIN (toolitem)) ?
                gtk_widget_get_parent (toolitem) : toolitem,
                FALSE, event->x, event->y);
        }
        else
        {
            midori_browser_toolbar_popup_context_menu_cb (
                GTK_IS_BIN (toolitem) && gtk_bin_get_child (GTK_BIN (toolitem)) ?
                    gtk_widget_get_parent (toolitem) : toolitem,
                event->x, event->y, event->button, browser);
        }
        return TRUE;
    }
    return FALSE;
}

static void
_midori_browser_search_item_allocate_cb (GtkWidget* widget,
                                         GdkRectangle* allocation,
                                         gpointer user_data)
{
    MidoriBrowser* browser = MIDORI_BROWSER (user_data);
    MidoriWebSettings* settings = browser->settings;
    g_object_set (settings, "search-width", allocation->width, NULL);
}

static void
_midori_browser_set_toolbar_items (MidoriBrowser* browser,
                                   const gchar*   items)
{
    gchar** names;
    gchar** name;
    GtkAction* action;
    GtkWidget* toolitem;
    const char* token_location = g_intern_static_string ("Location");
    const char* token_search = g_intern_static_string ("Search");
    const char* token_dontcare = g_intern_static_string ("Dontcare");
    const char* token_current = token_dontcare;
    const char* token_last;

    gtk_container_foreach (GTK_CONTAINER (browser->navigationbar),
        (GtkCallback)gtk_widget_destroy, NULL);

    names = g_strsplit (items ? items : "", ",", 0);
    name = names;
    for (; *name; ++name)
    {
        g_print("zgh add navagation item: name:%s\n",*name);
        action = _action_by_name (browser, *name);
        if (action && strstr (*name, "CompactMenu") == NULL)
        {
            token_last = token_current;

            /* Decide, what kind of token (item) we got now */
            if (name && !g_strcmp0 (*name, "Location"))
                token_current = token_location;
            else if (name && !g_strcmp0 (*name, "Search"))
                token_current = token_search;
            else
                token_current = token_dontcare;

            if ((token_current == token_location || token_current == token_search) &&
                 (token_last == token_location || token_last == token_search))
            {
                GtkWidget* toolitem_first = gtk_action_create_tool_item (
                    _action_by_name (browser, token_last));
                GtkWidget* toolitem_second = gtk_action_create_tool_item (
                    _action_by_name (browser, token_current));
                MidoriPanedAction* paned_action = MIDORI_PANED_ACTION (
                    _action_by_name (browser, "LocationSearch"));
                MidoriWebSettings* midori_settings = browser->settings;
                midori_paned_action_set_child1 (paned_action, toolitem_first, token_last,
                    token_last == token_search ? FALSE : TRUE, TRUE);
                midori_paned_action_set_child2 (paned_action, toolitem_second, token_current,
                    token_current == token_search ? FALSE : TRUE, TRUE);
                g_signal_connect (G_OBJECT (token_current == token_search ? toolitem_second : toolitem_first),
                    "size-allocate", G_CALLBACK (_midori_browser_search_item_allocate_cb), (gpointer) browser);

                gtk_widget_set_size_request (
                    token_last == token_search ? toolitem_first : toolitem_second,
                    katze_object_get_int ((gpointer) midori_settings,
                    "search-width"),
                    -1);

                toolitem = gtk_action_create_tool_item (GTK_ACTION (paned_action));
                token_current = token_dontcare;
            }
            else if (token_current == token_dontcare && token_last != token_dontcare)
            {
                /* There was a location or search item, but was not followed by
                   the other one, that form a couple */
                gtk_toolbar_insert (GTK_TOOLBAR (browser->navigationbar),
                    GTK_TOOL_ITEM (gtk_action_create_tool_item (
                    _action_by_name (browser, token_last))),
                    -1);

                toolitem = gtk_action_create_tool_item (action);
            }
            else if (token_current != token_dontcare && token_last == token_dontcare)
                continue;
            /* A "new tab" button is already part of the notebook */
            else if (!strcmp (gtk_action_get_name (action), "TabNew"))
                continue;
            else
                toolitem = gtk_action_create_tool_item (action);

            if (gtk_bin_get_child (GTK_BIN (toolitem)))
            {
                if (!g_strcmp0 (*name, "Back"))
                    g_object_set_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (toolitem))),
                        "history-back", (void*) 0xdeadbeef);
                else if (g_str_has_suffix (*name, "Forward"))
                    g_object_set_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (toolitem))),
                        "history-forward", (void*) 0xdeadbeef);
                else if (g_strcmp0 (*name, "Reload"))
                    g_object_set_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (toolitem))),
                        "reload-middle-click", (void*) 0xdeadbeef);

                g_signal_connect (gtk_bin_get_child (GTK_BIN (toolitem)),
                    "button-press-event",
                    G_CALLBACK (midori_browser_toolbar_item_button_press_event_cb),
                    browser);
            }
            else
            {
                gtk_tool_item_set_use_drag_window (GTK_TOOL_ITEM (toolitem), TRUE);
                g_signal_connect (toolitem,
                    "button-press-event",
                    G_CALLBACK (midori_browser_toolbar_item_button_press_event_cb),
                    browser);
            }
           #if 1   //zgh
           // if (g_strcmp0(*name, "BookmarkAdd"))
            gtk_toolbar_insert (GTK_TOOLBAR (browser->navigationbar),
                                GTK_TOOL_ITEM (toolitem), -1);
           #endif
        }
    }
    g_strfreev (names);

    /* There was a last item, which could have formed a couple, but
       there is no item left, we add that last item to toolbar as is */
    if (token_current != token_dontcare)
    {
        gtk_toolbar_insert (GTK_TOOLBAR (browser->navigationbar),
            GTK_TOOL_ITEM (gtk_action_create_tool_item (
            _action_by_name (browser, token_current))), -1);
    }

    if (!katze_object_get_boolean (browser->settings, "show-menubar"))
    {
        toolitem = gtk_action_create_tool_item (
            GTK_ACTION (_action_by_name (browser, "CompactMenu")));
        gtk_toolbar_insert (GTK_TOOLBAR (browser->navigationbar),
                            GTK_TOOL_ITEM (toolitem), -1);
        g_signal_connect (gtk_bin_get_child (GTK_BIN (toolitem)),
            "button-press-event",
            G_CALLBACK (midori_browser_toolbar_item_button_press_event_cb),
            browser);
    }
}

static void
_midori_browser_update_settings (MidoriBrowser* browser)
{
    gboolean remember_last_window_size;
    MidoriWindowState last_window_state;
    guint inactivity_reset;
    gboolean compact_sidepanel;
    gboolean right_align_sidepanel, open_panels_in_windows;
    gint last_panel_position, last_panel_page;
    gboolean show_menubar, show_bookmarkbar;
    gboolean show_panel;
    MidoriToolbarStyle toolbar_style;
    gchar* toolbar_items;
    gboolean close_buttons_left, close_buttons_on_tabs;

    g_object_get (browser->settings,
                  "remember-last-window-size", &remember_last_window_size,
                  "last-window-width", &browser->last_window_width,
                  "last-window-height", &browser->last_window_height,
                  "last-window-state", &last_window_state,
                  "inactivity-reset", &inactivity_reset,
                  "compact-sidepanel", &compact_sidepanel,
                  "right-align-sidepanel", &right_align_sidepanel,
                  "open-panels-in-windows", &open_panels_in_windows,
                  "last-panel-position", &last_panel_position,
                  "last-panel-page", &last_panel_page,
                  "show-menubar", &show_menubar,
                  "show-navigationbar", &browser->show_navigationbar,
                  "show-bookmarkbar", &show_bookmarkbar,
                  "show-panel", &show_panel,
                  "show-statusbar", &browser->show_statusbar,
                  "toolbar-style", &toolbar_style,
                  "toolbar-items", &toolbar_items,
                  "close-buttons-left", &close_buttons_left,
                  "close-buttons-on-tabs", &close_buttons_on_tabs,
                  "maximum-history-age", &browser->maximum_history_age,
                  NULL);

    midori_notebook_set_close_buttons_visible (
        MIDORI_NOTEBOOK (browser->notebook), close_buttons_on_tabs);
    midori_notebook_set_close_buttons_left (
        MIDORI_NOTEBOOK (browser->notebook), close_buttons_left);
    midori_findbar_set_close_button_left (MIDORI_FINDBAR (browser->find),
        close_buttons_left);
    if (browser->dial != NULL)
        midori_speed_dial_set_close_buttons_left (browser->dial, close_buttons_left);

    midori_browser_set_inactivity_reset (browser, inactivity_reset);

    if (remember_last_window_size)
    {
        if (browser->last_window_width && browser->last_window_height)
            gtk_window_set_default_size (GTK_WINDOW (browser),
                browser->last_window_width, browser->last_window_height);
        else
            katze_window_set_sensible_default_size (GTK_WINDOW (browser));
        switch (last_window_state)
        {
            case MIDORI_WINDOW_MINIMIZED:
                gtk_window_iconify (GTK_WINDOW (browser));
                break;
            case MIDORI_WINDOW_MAXIMIZED:
                gtk_window_maximize (GTK_WINDOW (browser));
                break;
            case MIDORI_WINDOW_FULLSCREEN:
                gtk_window_fullscreen (GTK_WINDOW (browser));
                break;
            default:
                ;/* Do nothing. */
        }
    }

    _midori_browser_set_toolbar_style (browser, toolbar_style);
    _toggle_tabbar_smartly (browser, FALSE);
    _midori_browser_set_toolbar_items (browser, toolbar_items);

    if (browser->search_engines)
    {
        const gchar* default_search = midori_settings_get_location_entry_search (
            MIDORI_SETTINGS (browser->settings));
        KatzeItem* item;

        if ((item = katze_array_get_nth_item (browser->search_engines,
                                              browser->last_web_search)))
            midori_search_action_set_current_item (MIDORI_SEARCH_ACTION (
                _action_by_name (browser, "Search")), item);

        if (default_search != NULL
         && (item = katze_array_find_uri (browser->search_engines, default_search)))
            midori_search_action_set_default_item (MIDORI_SEARCH_ACTION (
                _action_by_name (browser, "Search")), item);
    }

    g_object_set (browser->panel, "show-titles", !compact_sidepanel,
        "right-aligned", right_align_sidepanel,
        "open-panels-in-windows", open_panels_in_windows, NULL);
    gtk_paned_set_position (GTK_PANED (gtk_widget_get_parent (browser->panel)),
                            last_panel_position);
    /* The browser may not yet be visible, which means that we can't set the
       page. So we set it in midori_browser_size_allocate_cb */
    if (gtk_widget_get_visible (GTK_WIDGET (browser)))
        midori_panel_set_current_page (MIDORI_PANEL (browser->panel), last_panel_page);
    else
        g_object_set_data (G_OBJECT (browser), "last-page",
                           GINT_TO_POINTER (last_panel_page));

    _action_set_active (browser, "Menubar", show_menubar);
    _action_set_active (browser, "Navigationbar", browser->show_navigationbar);
    _action_set_active (browser, "Bookmarkbar", show_bookmarkbar
                                             && browser->bookmarks != NULL);
    _action_set_active (browser, "Panel", show_panel);
    _action_set_active (browser, "Statusbar", browser->show_statusbar);

    g_free (toolbar_items);
}

static void
midori_browser_settings_notify (MidoriWebSettings* web_settings,
                                GParamSpec*        pspec,
                                MidoriBrowser*     browser)
{
    const gchar* name;
    GValue value = {0, };

    name = g_intern_string (pspec->name);
    g_value_init (&value, pspec->value_type);
    g_object_get_property (G_OBJECT (web_settings), name, &value);

    if (name == g_intern_string ("toolbar-style"))
        _midori_browser_set_toolbar_style (browser, g_value_get_enum (&value));
    else if (name == g_intern_string ("toolbar-items"))
        _midori_browser_set_toolbar_items (browser, g_value_get_string (&value));
    else if (name == g_intern_string ("compact-sidepanel"))
    {
        g_signal_handlers_block_by_func (browser->panel,
            midori_panel_notify_show_titles_cb, browser);
        g_object_set (browser->panel, "show-titles",
                      !g_value_get_boolean (&value), NULL);
        g_signal_handlers_unblock_by_func (browser->panel,
            midori_panel_notify_show_titles_cb, browser);
    }
    else if (name == g_intern_string ("open-panels-in-windows"))
        g_object_set (browser->panel, "open-panels-in-windows",
                      g_value_get_boolean (&value), NULL);
    else if (name == g_intern_string ("always-show-tabbar"))
        _toggle_tabbar_smartly (browser, FALSE);
    else if (name == g_intern_string ("show-menubar"))
    {
        _action_set_active (browser, "Menubar", g_value_get_boolean (&value));
    }
    else if (name == g_intern_string ("show-navigationbar"))
    {
        browser->show_navigationbar = g_value_get_boolean (&value);
        _action_set_active (browser, "Navigationbar", g_value_get_boolean (&value));
    }
    else if (name == g_intern_string ("show-bookmarkbar"))
    {
        _action_set_active (browser, "Bookmarkbar", g_value_get_boolean (&value));
    }
    else if (name == g_intern_string ("show-statusbar"))
    {
        browser->show_statusbar = g_value_get_boolean (&value);
        _action_set_active (browser, "Statusbar", g_value_get_boolean (&value));
    }
    else if (name == g_intern_string ("maximum-history-age"))
        browser->maximum_history_age = g_value_get_int (&value);
    else if (name == g_intern_string ("close-buttons-on-tabs"))
        midori_notebook_set_close_buttons_visible (
            MIDORI_NOTEBOOK (browser->notebook), g_value_get_boolean (&value));
    else if (name == g_intern_string ("close-buttons-left"))
    {
        midori_findbar_set_close_button_left (MIDORI_FINDBAR (browser->find),
                                              g_value_get_boolean (&value));
        midori_speed_dial_set_close_buttons_left (browser->dial,
            g_value_get_boolean (&value));
        midori_notebook_set_close_buttons_left (
            MIDORI_NOTEBOOK (browser->notebook), g_value_get_boolean (&value));
    }
    else if (name == g_intern_string ("inactivity-reset"))
        midori_browser_set_inactivity_reset (browser, g_value_get_uint (&value));
    else if (!g_object_class_find_property (G_OBJECT_GET_CLASS (web_settings),
                                             name))
         g_warning (_("Unexpected setting '%s'"), name);
    g_value_unset (&value);
}

static void
midori_bookmarkbar_insert_item (GtkWidget* toolbar,
                                KatzeItem* item)
{
    MidoriBrowser* browser = midori_browser_get_for_widget (toolbar);
    GtkAction* action = _action_by_name (browser, "Bookmarks");
    GtkToolItem* toolitem = katze_array_action_create_tool_item_for (
        KATZE_ARRAY_ACTION (action), item);
    g_object_set_data (G_OBJECT (toolitem), "KatzeItem", item);

    if (!KATZE_IS_ITEM (item)) /* Separator */
        gtk_tool_item_set_use_drag_window (toolitem, TRUE);

    gtk_widget_show (GTK_WIDGET (toolitem));
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), toolitem, -1);
}

static void
midori_bookmarkbar_add_item_cb (KatzeArray*    bookmarks,
                                KatzeItem*     item,
                                MidoriBrowser* browser)
{
    if (gtk_widget_get_visible (browser->bookmarkbar))
        midori_bookmarkbar_populate (browser);
    else if (katze_item_get_meta_boolean (item, "toolbar"))
        _action_set_active (browser, "Bookmarkbar", TRUE);
    midori_browser_update_history (item, "bookmark", "created");
}

static void
midori_bookmarkbar_update_item_cb (KatzeArray*    bookmarks,
                                   KatzeItem*     item,
                                   MidoriBrowser* browser)
{
    if (gtk_widget_get_visible (browser->bookmarkbar))
        midori_bookmarkbar_populate (browser);
    midori_browser_update_history (item, "bookmark", "modify");
}

static void
midori_bookmarkbar_remove_item_cb (KatzeArray*    bookmarks,
                                   KatzeItem*     item,
                                   MidoriBrowser* browser)
{
    if (gtk_widget_get_visible (browser->bookmarkbar))
        midori_bookmarkbar_populate (browser);
    midori_browser_update_history (item, "bookmark", "delete");
}

static void
midori_bookmarkbar_populate (MidoriBrowser* browser)
{
    if (browser->bookmarkbar_populate)
        return;

    g_idle_add (midori_browser_idle, browser);
    browser->bookmarkbar_populate = TRUE;
}

static void
midori_bookmarkbar_populate_idle (MidoriBrowser* browser)
{
    KatzeArray* array;
    KatzeItem* item;

    midori_bookmarkbar_clear (browser->bookmarkbar);

    /* Use a dummy to ensure height of the toolbar */
    gtk_toolbar_insert (GTK_TOOLBAR (browser->bookmarkbar),
                        gtk_separator_tool_item_new (), -1);

    array = midori_bookmarks_db_query_recursive (browser->bookmarks,
        "id, parentid, title, uri, desc, app, toolbar, pos_panel, pos_bar", "toolbar = 1", NULL, FALSE);
    if (!array)
    {
        _action_set_sensitive (browser, "BookmarkAdd", FALSE);
        _action_set_sensitive (browser, "BookmarkFolderAdd", FALSE);
        return;
    }

    KATZE_ARRAY_FOREACH_ITEM (item, array)
    {
        midori_bookmarkbar_insert_item (browser->bookmarkbar, item);
    }
    _action_set_sensitive (browser, "BookmarkAdd", TRUE);
    _action_set_sensitive (browser, "BookmarkFolderAdd", TRUE);
}

static void
midori_bookmarkbar_clear (GtkWidget* toolbar)
{
    GList* children = gtk_container_get_children (GTK_CONTAINER (toolbar));
    while (children != NULL)
    {
        gtk_widget_destroy (children->data);
        children = g_list_next (children);
    }
}

static void
midori_browser_show_bookmarkbar_notify_value_cb (MidoriWebSettings* settings,
                                                 GParamSpec*        pspec,
                                                 MidoriBrowser*     browser)
{
    if (!katze_object_get_boolean (browser->settings, "show-bookmarkbar"))
        midori_bookmarkbar_clear (browser->bookmarkbar);
    else
        midori_bookmarkbar_populate (browser);
}

static void
midori_browser_set_bookmarks (MidoriBrowser* browser,
                              MidoriBookmarksDb*    bookmarks)
{
    MidoriWebSettings* settings;

    if (browser->bookmarks != NULL)
    {
        g_signal_handlers_disconnect_by_func (browser->bookmarks,
            midori_bookmarkbar_add_item_cb, browser);
        g_signal_handlers_disconnect_by_func (browser->bookmarks,
            midori_bookmarkbar_update_item_cb, browser);
        g_signal_handlers_disconnect_by_func (browser->bookmarks,
            midori_bookmarkbar_remove_item_cb, browser);
    }

    g_object_set (G_OBJECT (_action_by_name (browser, "Bookmarks")),
        "array", KATZE_ARRAY (bookmarks),
        NULL);

    settings = midori_browser_get_settings (browser);
    g_signal_handlers_disconnect_by_func (settings,
        midori_browser_show_bookmarkbar_notify_value_cb, browser);
    katze_object_assign (browser->bookmarks, bookmarks);

    _action_set_visible (browser, "Bookmarks", bookmarks != NULL);
    if (bookmarks != NULL)
    {
        /* FIXME: Proxies aren't shown propely. Why? */
        GSList* proxies = gtk_action_get_proxies (
            _action_by_name (browser, "Bookmarks"));
        for (; proxies; proxies = g_slist_next (proxies))
            gtk_widget_show (proxies->data);
    }
    _action_set_visible (browser, "BookmarkAdd", bookmarks != NULL);
    _action_set_visible (browser, "BookmarksImport", bookmarks != NULL);
    _action_set_visible (browser, "BookmarksExport", bookmarks != NULL);
    _action_set_visible (browser, "Bookmarkbar", bookmarks != NULL);

    if (!bookmarks)
        return;

    if (katze_object_get_boolean (browser->settings, "show-bookmarkbar"))
        _action_set_active (browser, "Bookmarkbar", TRUE);
    g_object_ref (bookmarks);
    g_signal_connect (settings, "notify::show-bookmarkbar",
        G_CALLBACK (midori_browser_show_bookmarkbar_notify_value_cb), browser);
    g_object_notify (G_OBJECT (settings), "show-bookmarkbar");
    g_signal_connect_after (bookmarks, "add-item",
        G_CALLBACK (midori_bookmarkbar_add_item_cb), browser);
    g_signal_connect_after (bookmarks, "update-item",
        G_CALLBACK (midori_bookmarkbar_update_item_cb), browser);
    g_signal_connect_after (bookmarks, "remove-item",
        G_CALLBACK (midori_bookmarkbar_remove_item_cb), browser);
}

static void
midori_browser_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);
    KatzeItem* item;

    switch (prop_id)
    {
    case PROP_URI:
        midori_browser_set_current_uri (browser, g_value_get_string (value));
        break;
    case PROP_TAB:
        midori_browser_set_current_tab (browser, g_value_get_object (value));
        break;
    case PROP_STATUSBAR_TEXT:
        _midori_browser_set_statusbar_text (browser,
            MIDORI_VIEW (midori_browser_get_current_tab (browser)),
            g_value_get_string (value));
        break;
    case PROP_SETTINGS:
        g_signal_handlers_disconnect_by_func (browser->settings,
                                              midori_browser_settings_notify,
                                              browser);
        katze_object_assign (browser->settings, g_value_dup_object (value));
        if (!browser->settings)
            browser->settings = midori_web_settings_new ();

        _midori_browser_update_settings (browser);
        g_signal_connect (browser->settings, "notify",
            G_CALLBACK (midori_browser_settings_notify), browser);
        GList* tabs = midori_browser_get_tabs (browser);
        for (; tabs; tabs = g_list_next (tabs))
            midori_view_set_settings (tabs->data, browser->settings);
        g_list_free (tabs);
        break;
    case PROP_BOOKMARKS:
        midori_browser_set_bookmarks (browser, g_value_get_object (value));
        break;
    case PROP_TRASH:
        /* FIXME: Disconnect handlers */
        katze_object_assign (browser->trash, g_value_dup_object (value));
        g_object_set (_action_by_name (browser, "Trash"),
                      "array", browser->trash, "reversed", TRUE,
                      NULL);
        _action_set_visible (browser, "Trash", browser->trash != NULL);
        _action_set_visible (browser, "UndoTabClose", browser->trash != NULL);
        if (browser->trash != NULL)
        {
            g_signal_connect_after (browser->trash, "clear",
                G_CALLBACK (midori_browser_trash_clear_cb), browser);
            midori_browser_trash_clear_cb (browser->trash, browser);
        }
        break;
    case PROP_SEARCH_ENGINES:
    {
        /* FIXME: Disconnect handlers */
        katze_object_assign (browser->search_engines, g_value_dup_object (value));
        midori_location_action_set_search_engines (MIDORI_LOCATION_ACTION (
            _action_by_name (browser, "Location")), browser->search_engines);
        midori_search_action_set_search_engines (MIDORI_SEARCH_ACTION (
            _action_by_name (browser, "Search")), browser->search_engines);
        /* FIXME: Connect to updates */

        if (browser->search_engines)
        {
            const gchar* default_search = midori_settings_get_location_entry_search (
                MIDORI_SETTINGS (browser->settings));
            g_object_get (browser->settings, "last-web-search", &browser->last_web_search, NULL);
            item = katze_array_get_nth_item (browser->search_engines, browser->last_web_search);
            midori_search_action_set_current_item (MIDORI_SEARCH_ACTION (
                _action_by_name (browser, "Search")), item);

            if (default_search != NULL && (item = katze_array_find_uri (browser->search_engines, default_search)))
                midori_search_action_set_default_item (MIDORI_SEARCH_ACTION (
                    _action_by_name (browser, "Search")), item);
        }
        break;
    }
    case PROP_HISTORY:
        midori_browser_set_history (browser, g_value_get_object (value));
        break;
    case PROP_SPEED_DIAL:
        if (browser->dial != NULL)
            g_signal_handlers_disconnect_by_func (browser->dial,
                midori_browser_speed_dial_refresh_cb, browser);
        katze_object_assign (browser->dial, g_value_dup_object (value));
        if (browser->dial != NULL)
            g_signal_connect (browser->dial, "refresh",
                G_CALLBACK (midori_browser_speed_dial_refresh_cb), browser);
        break;
    case PROP_SHOW_TABS:
        browser->show_tabs = g_value_get_boolean (value);
        _toggle_tabbar_smartly (browser, FALSE);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
midori_browser_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec)
{
    MidoriBrowser* browser = MIDORI_BROWSER (object);

    switch (prop_id)
    {
    case PROP_MENUBAR:
        g_value_set_object (value, browser->menubar);
        break;
    case PROP_NAVIGATIONBAR:
        g_value_set_object (value, browser->navigationbar);
        break;
    case PROP_NOTEBOOK:
        g_value_set_object (value, MIDORI_NOTEBOOK (browser->notebook)->notebook);
        break;
    case PROP_PANEL:
        g_value_set_object (value, browser->panel);
        break;
    case PROP_URI:
        g_value_set_string (value, midori_browser_get_current_uri (browser));
        break;
    case PROP_TAB:
        g_value_set_object (value, midori_browser_get_current_tab (browser));
        break;
    case PROP_LOAD_STATUS:
    {
        GtkWidget* view = midori_browser_get_current_tab (browser);
        if (view)
            g_value_set_enum (value,
                midori_view_get_load_status (MIDORI_VIEW (view)));
        else
            g_value_set_enum (value, MIDORI_LOAD_FINISHED);
        break;
    }
    case PROP_STATUSBAR:
        g_value_set_object (value, browser->statusbar_contents);
        break;
    case PROP_STATUSBAR_TEXT:
        g_value_set_string (value, browser->statusbar_text);
        break;
    case PROP_SETTINGS:
        g_value_set_object (value, browser->settings);
        break;
    case PROP_PROXY_ITEMS:
        g_value_set_object (value, browser->proxy_array);
        break;
    case PROP_BOOKMARKS:
        g_value_set_object (value, browser->bookmarks);
        break;
    case PROP_TRASH:
        g_value_set_object (value, browser->trash);
        break;
    case PROP_SEARCH_ENGINES:
        g_value_set_object (value, browser->search_engines);
        break;
    case PROP_HISTORY:
        g_value_set_object (value, browser->history);
        break;
    case PROP_SPEED_DIAL:
        g_value_set_object (value, browser->dial);
        break;
    case PROP_SHOW_TABS:
        g_value_set_boolean (value, browser->show_tabs);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * midori_browser_new:
 *
 * Creates a new browser widget.
 *
 * A browser is a window with a menubar, toolbars, a notebook, panels
 * and a statusbar. You should mostly treat it as an opaque widget.
 *
 * Return value: a new #MidoriBrowser
 **/
MidoriBrowser*
midori_browser_new (void)
{
    MidoriBrowser* browser = g_object_new (MIDORI_TYPE_BROWSER,
                                           NULL);

    return browser;
}

/**
 * midori_browser_add_tab:
 * @browser: a #MidoriBrowser
 * @widget: a view
 *
 * Appends a view in the form of a new tab and creates an
 * according item in the Window menu.
 *
 * Since: 0.4.9: Return type is void
 **/
void
midori_browser_add_tab (MidoriBrowser* browser,
                        GtkWidget*     view)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (GTK_IS_WIDGET (view));

#ifndef HAVE_WEBKIT2
    if (!g_object_get_data (G_OBJECT (webkit_get_default_session ()),
                            "midori-session-initialized"))
        g_critical ("midori_load_soup_session was not called!");
#endif
    g_signal_emit (browser, signals[ADD_TAB], 0, view);
}

/**
 * midori_browser_page_num:
 * @browser: a #MidoriBrowser
 * @widget: a widget in the browser
 *
 * Retrieves the position of @widget in the browser.
 *
 * If there is no page present at all, -1 is returned.
 *
 * Return value: the index of the widget, or -1
 *
 * Since: 0.4.5
 **/
gint
midori_browser_page_num (MidoriBrowser* browser,
                         GtkWidget*     view)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), -1);
    g_return_val_if_fail (MIDORI_IS_VIEW (view), -1);

    return midori_notebook_get_tab_index (MIDORI_NOTEBOOK (browser->notebook), MIDORI_TAB (view));
}


/**
 * midori_browser_close_tab:
 * @browser: a #MidoriBrowser
 * @widget: a view
 *
 * Closes an existing view, removing it and
 * its associated menu item from the browser.
 **/
void
midori_browser_close_tab (MidoriBrowser* browser,
                           GtkWidget*     view)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (GTK_IS_WIDGET (view));

    g_signal_emit (browser, signals[REMOVE_TAB], 0, view);
}

/**
 * midori_browser_add_item:
 * @browser: a #MidoriBrowser
 * @item: an item
 *
 * Return value: a #GtkWidget
 *
 * Since: 0.4.9: Return type is GtkWidget*
 **/
GtkWidget*
midori_browser_add_item (MidoriBrowser* browser,
                         KatzeItem*     item)
{
    const gchar* uri;
    GtkWidget* view;

    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);
    g_return_val_if_fail (KATZE_IS_ITEM (item), NULL);

    uri = katze_item_get_uri (item);
    view = midori_view_new_with_item (item, browser->settings);
    midori_browser_add_tab (browser, view);
    midori_view_set_uri (MIDORI_VIEW (view), uri);
    return view;
}

/**
 * midori_browser_add_uri:
 * @browser: a #MidoriBrowser
 * @uri: an URI
 *
 * Appends an uri in the form of a new view.
 *
 * Return value: a #GtkWidget
 *
 * Since: 0.4.9: Return type is GtkWidget*
 **/
GtkWidget*
midori_browser_add_uri (MidoriBrowser* browser,
                        const gchar*   uri)
{
    KatzeItem* item;

    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);
    g_return_val_if_fail (uri != NULL, NULL);

    item = katze_item_new ();
    item->uri = g_strdup (uri);
    return midori_browser_add_item (browser, item);
}

/**
 * midori_browser_activate_action:
 * @browser: a #MidoriBrowser
 * @name: action, setting=value expression or extension=true|false
 *
 * Activates the specified action. See also midori_browser_assert_action().
 **/
void
midori_browser_activate_action (MidoriBrowser* browser,
                                const gchar*   name)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (name != NULL);

    g_signal_emit (browser, signals[ACTIVATE_ACTION], 0, name);
}

void
midori_browser_set_action_visible (MidoriBrowser* browser,
                                   const gchar*   name,
                                   gboolean       visible)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (name != NULL);

    _action_set_visible (browser, name, visible);
    _action_set_sensitive (browser, name, visible);
}

/**
 * midori_browser_block_action:
 * @browser: a #MidoriBrowser
 * @name: the action to be blocked
 *
 * Blocks built-in behavior of the specified action without
 * disabling it, which gives you a chance to connect your
 * own signal handling.
 * Call midori_browser_unblock_action() to undo the effect.
 *
 * Since: 0.3.4
 **/
void
midori_browser_block_action (MidoriBrowser* browser,
                             GtkAction*     action)
{
    const gchar* name;
    guint i;

    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (GTK_IS_ACTION (action));

    name = gtk_action_get_name (action);
    for (i = 0; i < entries_n; i++)
        if (g_str_equal (entries[i].name, name))
        {
            g_signal_handlers_block_by_func (action, entries[i].callback, browser);
            return;
        }
    g_critical ("%s: Action \"%s\" can't be blocked.", G_STRFUNC, name);
}

/**
 * midori_browser_unblock_action:
 * @browser: a #MidoriBrowser
 * @name: the action to be unblocked
 *
 * Restores built-in behavior of the specified action after
 * previously blocking it with midori_browser_block_action().
 *
 * Since: 0.3.4
 **/
void
midori_browser_unblock_action (MidoriBrowser* browser,
                               GtkAction*     action)
{
    const gchar* name;
    guint i;

    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (GTK_IS_ACTION (action));

    name = gtk_action_get_name (action);
    for (i = 0; i < entries_n; i++)
        if (g_str_equal (entries[i].name, name))
        {
            g_signal_handlers_unblock_by_func (action, entries[i].callback, browser);
            return;
        }
    g_critical ("%s: Action \"%s\" can't be unblocked.", G_STRFUNC, name);
}


/**
 * midori_browser_get_action_group:
 * @browser: a #MidoriBrowser
 *
 * Retrieves the action group holding all actions used
 * by the browser. It allows obtaining individual
 * actions and adding new actions.
 *
 * Return value: the action group of the browser
 *
 * Since: 0.1.4
 **/
GtkActionGroup*
midori_browser_get_action_group (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    return browser->action_group;
}

/**
 * midori_browser_set_current_uri:
 * @browser: a #MidoriBrowser
 * @uri: an URI
 *
 * Loads the specified URI in the current view.
 *
 * If the current view is opaque, and cannot load
 * new pages, it will automatically open a new tab.
 **/
void
midori_browser_set_current_uri (MidoriBrowser* browser,
                                const gchar*   uri)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (uri != NULL);

    midori_view_set_uri (MIDORI_VIEW (midori_browser_get_current_tab (browser)), uri);
}

/**
 * midori_browser_get_current_uri:
 * @browser: a #MidoriBrowser
 *
 * Determines the URI loaded in the current view.
 *
 * If there is no view present at all, %NULL is returned.
 *
 * Return value: the current URI, or %NULL
 **/
const gchar*
midori_browser_get_current_uri (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    return midori_view_get_display_uri (MIDORI_VIEW (
        midori_browser_get_current_tab (browser)));
}

/**
 * midori_browser_set_current_page:
 * @browser: a #MidoriBrowser
 * @n: the index of a page
 *
 * Switches to the page with the index @n.
 *
 * The widget will also grab the focus automatically.
 **/
void
midori_browser_set_current_page (MidoriBrowser* browser,
                                 gint           n)
{
    GtkWidget* view;

    g_return_if_fail (MIDORI_IS_BROWSER (browser));

    view = midori_browser_get_nth_tab (browser, n);
    g_return_if_fail (view != NULL);

    midori_browser_set_tab (browser, view);
}

/**
 * midori_browser_get_current_page:
 * @browser: a #MidoriBrowser
 *
 * Determines the currently selected page.
 *
 * If there is no page present at all, %NULL is returned.
 *
 * Return value: the selected page, or -1
 **/
gint
midori_browser_get_current_page (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), -1);

    return midori_notebook_get_index (MIDORI_NOTEBOOK (browser->notebook));
}

/**
 * midori_browser_set_current_item:
 * @browser: a #MidoriBrowser
 * @item: a #KatzeItem
 *
 * Switches to the page containing @item, see also midori_browser_set_current_page().
 *
 * The widget will also grab the focus automatically.
 *
 * Since: 0.4.8
 **/
void
midori_browser_set_current_item (MidoriBrowser* browser,
                                 KatzeItem*     item)
{
    guint i;
    guint n = katze_array_get_length (browser->proxy_array);

    for (i = 0; i < n; i++)
    {
        GtkWidget* view = midori_browser_get_nth_tab (browser, i);
        if (midori_view_get_proxy_item (MIDORI_VIEW (view)) == item)
            midori_browser_set_current_page (browser, i);
    }
}

/**
 * midori_browser_get_nth_tab:
 * @browser: a #MidoriBrowser
 * @page: the index of a tab
 *
 * Retrieves the tab at the position @page.
 *
 * If there is no page present at all, %NULL is returned.
 *
 * Return value: the selected page, or -1
 *
 * Since: 0.1.9
 **/
GtkWidget*
midori_browser_get_nth_tab (MidoriBrowser* browser,
                            gint           page)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    return (GtkWidget*)midori_notebook_get_nth_tab (MIDORI_NOTEBOOK (browser->notebook), page);
}

/**
 * midori_browser_set_tab:
 * @browser: a #MidoriBrowser
 * @view: a #GtkWidget
 *
 * Switches to the page containing @view.
 *
 * The widget will also grab the focus automatically.
 *
 * Since: 0.2.6
 **/
void
midori_browser_set_current_tab (MidoriBrowser* browser,
                                GtkWidget*     view)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));
    g_return_if_fail (GTK_IS_WIDGET (view));

    midori_notebook_set_tab (MIDORI_NOTEBOOK (browser->notebook), MIDORI_TAB (view));
    if (midori_tab_is_blank (MIDORI_TAB (view)))
        midori_browser_activate_action (browser, "Location");
    else
        gtk_widget_grab_focus (view);

    midori_browser_notify_tab_cb (browser->notebook, NULL, browser);
}

/**
 * midori_browser_get_tab:
 * @browser: a #MidoriBrowser
 *
 * Retrieves the currently selected tab.
 *
 * If there is no tab present at all, %NULL is returned.
 *
 * See also midori_browser_get_current_page().
 *
 * Return value: the selected tab, or %NULL
 *
 * Since: 0.2.6
 **/
GtkWidget*
midori_browser_get_current_tab (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    return (GtkWidget*)midori_notebook_get_tab (MIDORI_NOTEBOOK (browser->notebook));
}

/**
 * midori_browser_get_tabs:
 * @browser: a #MidoriBrowser
 *
 * Retrieves the tabs as a list.
 *
 * Return value: a newly allocated #GList of #MidoriView
 *
 * Since: 0.2.5
 **/
GList*
midori_browser_get_tabs (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    return gtk_container_get_children (GTK_CONTAINER (browser->notebook));
}

/**
 * midori_browser_get_proxy_array:
 * @browser: a #MidoriBrowser
 *
 * Retrieves a proxy array representing the respective proxy items.
 * The array is updated automatically.
 *
 * Return value: the proxy #KatzeArray
 **/
KatzeArray*
midori_browser_get_proxy_array (MidoriBrowser* browser)
{
    g_return_val_if_fail (MIDORI_IS_BROWSER (browser), NULL);

    return browser->proxy_array;
}

/**
 * midori_browser_get_for_widget:
 * @widget: a #GtkWidget
 *
 * Determines the browser appropriate for the specified widget.
 *
 * Return value: a #MidoriBrowser
 *
 * Since 0.1.7
 **/
MidoriBrowser*
midori_browser_get_for_widget (GtkWidget* widget)
{
    gpointer browser;

    g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

    browser = gtk_widget_get_toplevel (GTK_WIDGET (widget));
    if (!MIDORI_IS_BROWSER (browser))
    {
        if (!GTK_IS_WINDOW (browser))
            return NULL;

        browser = gtk_window_get_transient_for (GTK_WINDOW (browser));
        if (!MIDORI_IS_BROWSER (browser))
        {
            /* For some reason, when called on the widget of the
             * application menubar we get here.
             */

            GList* top_levels = gtk_window_list_toplevels ();
            GList *iter;

            for (iter = top_levels; iter; iter = g_list_next (iter))
            {
                browser = iter->data;

                if (MIDORI_IS_BROWSER (browser) && gtk_widget_is_ancestor( GTK_WIDGET (browser), widget))
                {
                    g_list_free (top_levels);
                    return MIDORI_BROWSER (browser);
                }
            }

            g_list_free (top_levels);
            return NULL;
        }
    }

    return MIDORI_BROWSER (browser);
}

/**
 * midori_browser_quit:
 * @browser: a #MidoriBrowser
 *
 * Quits the browser, including any other browser windows.
 *
 * This function relys on the application implementing
 * the MidoriBrowser::quit signal. If the browser was added
 * to the MidoriApp, this is handled automatically.
 **/
void
midori_browser_quit (MidoriBrowser* browser)
{
    g_return_if_fail (MIDORI_IS_BROWSER (browser));

    g_signal_emit (browser, signals[QUIT], 0);
}
