/*
 Copyright (C) 2008 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __MIDORI_BROWSER_H__
#define __MIDORI_BROWSER_H__

#include <katze/katze.h>
#include "midori-view.h"

G_BEGIN_DECLS

#define MIDORI_TYPE_BROWSER \
    (midori_browser_get_type ())
#define MIDORI_BROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_BROWSER, MidoriBrowser))
#define MIDORI_BROWSER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_BROWSER, MidoriBrowserClass))
#define MIDORI_IS_BROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_BROWSER))
#define MIDORI_IS_BROWSER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_BROWSER))
#define MIDORI_BROWSER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_BROWSER, MidoriBrowserClass))

typedef struct _MidoriBrowser                MidoriBrowser;
typedef struct _MidoriBrowserClass           MidoriBrowserClass;

struct _MidoriBrowserClass
{
    GtkWindowClass parent_class;

    /* Signals */
    void
    (*window_object_cleared)   (MidoriBrowser*       browser,
#ifndef HAVE_WEBKIT2
                                WebKitWebFrame*      web_frame,
#else
                                void*                web_frame,
#endif
                                JSContextRef*        context,
                                JSObjectRef*         window_object);
    void
    (*statusbar_text_changed)  (MidoriBrowser*       browser,
                                const gchar*         text);
    void
    (*element_motion)          (MidoriBrowser*       browser,
                                const gchar*         link_uri);
    void
    (*new_window)              (MidoriBrowser*       browser,
                                const gchar*         uri);

    void
    (*add_tab)                 (MidoriBrowser*       browser,
                                GtkWidget*           view);
    void
    (*remove_tab)              (MidoriBrowser*       browser,
                                GtkWidget*           view);
    void
    (*activate_action)         (MidoriBrowser*       browser,
                                const gchar*         name);
    void
    (*quit)                    (MidoriBrowser*       browser);
};

//add by luyue 2014/12/30
GtkWidget *popup_window;

GType
midori_browser_get_type               (void) G_GNUC_CONST;

MidoriBrowser*
midori_browser_new                    (void);

void
midori_browser_add_tab                (MidoriBrowser*     browser,
                                       GtkWidget*         widget);

void
midori_browser_close_tab              (MidoriBrowser*     browser,
                                       GtkWidget*         widget);

//add by luyue 2015/5/16
void
midori_browser_close_tab1              (MidoriBrowser*     browser,
                                       GtkWidget*         widget);
//add end

GtkWidget*
midori_browser_add_item               (MidoriBrowser*     browser,
                                       KatzeItem*         item);

GtkWidget*
midori_browser_add_uri                (MidoriBrowser*     browser,
                                       const gchar*       uri);

void
midori_browser_activate_action        (MidoriBrowser*     browser,
                                       const gchar*       name);

void
midori_browser_assert_action          (MidoriBrowser*     browser,
                                       const gchar*       name);

void
midori_browser_set_action_visible     (MidoriBrowser*     browser,
                                       const gchar*       name,
                                       gboolean           visible);

GtkActionGroup*
midori_browser_get_action_group       (MidoriBrowser*     browser);

void
midori_browser_set_current_uri        (MidoriBrowser*     browser,
                                       const gchar*       uri);

const gchar*
midori_browser_get_current_uri        (MidoriBrowser*     browser);

void
midori_browser_set_current_page_smartly (MidoriBrowser* browser,
                                         gint           n);

void
midori_browser_set_current_tab_smartly (MidoriBrowser* browser,
                                        GtkWidget*     view);

void
midori_browser_set_current_page       (MidoriBrowser*     browser,
                                       gint               n);

gint
midori_browser_get_current_page       (MidoriBrowser*     browser);

void
midori_browser_set_current_item       (MidoriBrowser* browser,
                                       KatzeItem*     item);

GtkWidget*
midori_browser_get_nth_tab            (MidoriBrowser*     browser,
                                       gint               n);

void
midori_browser_set_current_tab        (MidoriBrowser*     browser,
                                       GtkWidget*         widget);
#define midori_browser_set_tab midori_browser_set_current_tab

GtkWidget*
midori_browser_get_current_tab        (MidoriBrowser*     browser);
#define midori_browser_get_tab midori_browser_get_current_tab

gint
midori_browser_page_num               (MidoriBrowser*     browser,
                                       GtkWidget*         view);

GList*
midori_browser_get_tabs               (MidoriBrowser*     browser);

gint
midori_browser_get_n_pages            (MidoriBrowser*     browser);

KatzeArray*
midori_browser_get_proxy_array        (MidoriBrowser*     browser);

MidoriBrowser*
midori_browser_get_for_widget         (GtkWidget*         widget);

void
midori_browser_quit                   (MidoriBrowser*     browser);

const gchar**
midori_browser_get_toolbar_actions    (MidoriBrowser*     browser);

MidoriWebSettings*
midori_browser_get_settings           (MidoriBrowser*     browser);

void
midori_browser_update_history         (KatzeItem*         item,
                                       const gchar*       type,
                                       const gchar*       event);

void
midori_browser_save_uri               (MidoriBrowser*     browser,
                                       MidoriView*        view,
                                       const gchar*       uri,
                                       gchar*             title);

void
midori_browser_set_inactivity_reset   (MidoriBrowser*     browser,
                                       gint               inactivity_reset);
             
//add by zgh 20141231                     
void 
_action_pageinfo_activate ( GtkAction*     action,
                            MidoriBrowser* browser);

void midori_browser_clear_history(MidoriBrowser*     browser);

void midori_browser_clear_bookmarks(MidoriBrowser* browser);

void midori_browser_change_history_seting(MidoriBrowser* browser, gint *settings);

// 当前用于Web扩展内通过新建窗口形式打开某个网页
void
midori_browser_open_new_tab_from_extension   (MidoriBrowser* browser, const gchar* uri, gboolean background);

G_END_DECLS

#endif /* __MIDORI_BROWSER_H__ */
