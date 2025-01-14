/*

*/

#ifndef __MIDORI_VIEW_H__
#define __MIDORI_VIEW_H__

#include "midori-websettings.h"
#include "cdosbrowser-core.h"

#include <katze/katze.h>

G_BEGIN_DECLS

typedef enum
{
    MIDORI_DELAY_UNDELAYED = -1, /* The view is in a regular undelayed state */
    MIDORI_DELAY_DELAYED = 1, /* The view is delayed but has not displayed any indication of such */
    MIDORI_DELAY_PENDING_UNDELAY = -2 /* The view is delayed and showing a message asking to be undelayed */
} MidoriDelay;

#define MIDORI_TYPE_VIEW \
    (midori_view_get_type ())

/*typedef enum
{
    MIDORI_DOWNLOAD_CANCEL,
    MIDORI_DOWNLOAD_OPEN,
    MIDORI_DOWNLOAD_SAVE,
    MIDORI_DOWNLOAD_SAVE_AS,
    MIDORI_DOWNLOAD_OPEN_IN_VIEWER,
} MidoriDownloadType;*/

#define MIDORI_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_VIEW, MidoriView))
#define MIDORI_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_VIEW, MidoriViewClass))
#define MIDORI_IS_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_VIEW))
#define MIDORI_IS_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_VIEW))
#define MIDORI_VIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_VIEW, MidoriViewClass))

typedef struct _MidoriView                MidoriView;
typedef struct _MidoriViewClass           MidoriViewClass;

GType
midori_view_get_type                   (void) G_GNUC_CONST;

GtkWidget*
midori_view_new_with_title             (const gchar*       title,
                                        MidoriWebSettings* settings,
                                        gboolean           append);

GtkWidget*
midori_view_new_with_item              (KatzeItem*         item,
                                        MidoriWebSettings* settings);

GtkWidget*
midori_view_new_from_view              (MidoriView*        view,
                                        KatzeItem*         item,
                                        MidoriWebSettings* settings);

void
midori_view_set_settings               (MidoriView*        view,
                                        MidoriWebSettings* settings);

gdouble
midori_view_get_progress               (MidoriView*        view);

MidoriLoadStatus
midori_view_get_load_status            (MidoriView*        view);

void
midori_view_set_uri                    (MidoriView*        view,
                                        const gchar*       uri);

void
midori_view_set_html                   (MidoriView*        view,
                                        const gchar*       data,
                                        const gchar*       uri,
                                        void*              web_frame);

void
midori_view_set_overlay_text           (MidoriView*        view,
                                        const gchar*       text);

gboolean
midori_view_is_blank                   (MidoriView*        view);

const gchar*
midori_view_get_display_uri            (MidoriView*        view);

const gchar*
midori_view_get_display_title          (MidoriView*        view);

GdkPixbuf*
midori_view_get_icon                   (MidoriView*        view);

const gchar*
midori_view_get_icon_uri               (MidoriView*        view);

const gchar*
midori_view_get_link_uri               (MidoriView*        view);

gboolean
midori_view_has_selection              (MidoriView*        view);

const gchar*
midori_view_get_selected_text          (MidoriView*        view);

GtkWidget*
midori_view_get_tab_menu               (MidoriView*        view);

GtkWidget*
midori_view_duplicate                  (MidoriView*        view);

PangoEllipsizeMode
midori_view_get_label_ellipsize        (MidoriView*        view);

KatzeItem*
midori_view_get_proxy_item             (MidoriView*        view);

gfloat
midori_view_get_zoom_level             (MidoriView*        view);

gboolean
midori_view_can_zoom_in                (MidoriView*        view);

gboolean
midori_view_can_zoom_out               (MidoriView*        view);

void
midori_view_set_zoom_level             (MidoriView*        view,
                                        gfloat             zoom_level);

void
midori_view_reload                     (MidoriView*        view,
                                        gboolean           from_cache);

gboolean
midori_view_can_go_back                (MidoriView*        view);

void
midori_view_go_back                    (MidoriView*        view);

gboolean
midori_view_can_go_forward             (MidoriView*        view);

void
midori_view_go_forward                 (MidoriView*        view);

const gchar*
midori_view_get_previous_page          (MidoriView*        view);

const gchar*
midori_view_get_next_page              (MidoriView*        view);

void
midori_view_print                      (MidoriView*        view);

//gboolean
//midori_view_can_view_source            (MidoriView*        view);

gchar*
midori_view_save_source                (MidoriView*        view,
                                        const gchar*       uri,
                                        const gchar*       outfile,
                                        gboolean           use_dom);

gboolean
midori_view_execute_script             (MidoriView*        view,
                                        const gchar*       script,
                                        gchar**            exception);

GtkWidget*
midori_view_get_web_view               (MidoriView*        view);

MidoriView*
midori_view_get_for_widget             (GtkWidget*         web_view);

void
midori_view_populate_popup             (MidoriView*        view,
                                        GtkWidget*         menu,
                                        gboolean           manual);

GtkWidget*
midori_view_add_info_bar               (MidoriView*        view,
                                        GtkMessageType     message_type,
                                        const gchar*       message,
                                        GCallback          response_cb,
                                        gpointer           user_data,
                                        const gchar*       first_button_text,
                                        ...);

const gchar*
midori_view_fallback_extension         (MidoriView*        view,
                                        const gchar*       extension);

GList*
midori_view_get_resources              (MidoriView*        view);

void
midori_view_list_versions              (GString*           markup,
                                        gboolean           html);

void
midori_view_list_plugins               (MidoriView*        view,
                                        GString*           markup,
                                        gboolean           html);

gboolean
midori_view_get_tls_info               (MidoriView*        view,
                                        void*              request,
                                        GTlsCertificate**     tls_cert,
                                        GTlsCertificateFlags* tls_flags,
                                        gchar**               hostname);

gchar** midori_view_get_website_record (MidoriView*        view);

//add by luyue 2015/1/20
void
midori_view_set_doublezoom_state       (MidoriView*        view,
                                        MidoriWebSettings* settings);      
void
midori_view_set_zoomtext_state         (MidoriView*        view,
                                        MidoriWebSettings* settings);
void
midori_view_set_doublezoom_level       (MidoriView*        view,
                                        MidoriWebSettings* settings);

//add by luyue 2015/6/30 start
void
midori_view_set_secure_level (MidoriView*        view,
                              MidoriWebSettings* settings);
//add end

//add by luyue 2015/8/8 start
void
midori_view_set_user_agent (MidoriView*  view,
                            const gchar* uri);
//add end

G_END_DECLS

#endif /* __MIDORI_VIEW_H__ */
