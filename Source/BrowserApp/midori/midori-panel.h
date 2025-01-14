/*

*/

#ifndef __MIDORI_PANEL_H__
#define __MIDORI_PANEL_H__

#include "cdosbrowser-core.h"

G_BEGIN_DECLS

#define MIDORI_TYPE_PANEL \
    (midori_panel_get_type ())
#define MIDORI_PANEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIDORI_TYPE_PANEL, MidoriPanel))
#define MIDORI_PANEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MIDORI_TYPE_PANEL, MidoriPanelClass))
#define MIDORI_IS_PANEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIDORI_TYPE_PANEL))
#define MIDORI_IS_PANEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MIDORI_TYPE_PANEL))
#define MIDORI_PANEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MIDORI_TYPE_PANEL, MidoriPanelClass))

typedef struct _MidoriPanel                MidoriPanel;
typedef struct _MidoriPanelClass           MidoriPanelClass;

GType
midori_panel_get_type               (void) G_GNUC_CONST;

GtkWidget*
midori_panel_new                    (void);

void
midori_panel_set_right_aligned      (MidoriPanel*       panel,
                                     gboolean           right_aligned);

gint
midori_panel_append_page            (MidoriPanel*       panel,
                                     MidoriViewable*    viewable);

gint
midori_panel_get_current_page       (MidoriPanel*       panel);

GtkWidget*
midori_panel_get_nth_page           (MidoriPanel*       panel,
                                     guint              page_num);

guint
midori_panel_get_n_pages            (MidoriPanel*       panel);

gint
midori_panel_page_num               (MidoriPanel*       panel,
                                     GtkWidget*         child);

void
midori_panel_set_current_page       (MidoriPanel*       panel,
                                     gint               n);

gint
midori_panel_append_widget          (MidoriPanel*       panel,
                                     GtkWidget*         widget,
                                     const gchar*       stock_id,
                                     const gchar*       label,
                                     GtkWidget*         toolbar);

void midori_panel_open_in_window(MidoriPanel* panel,
                                 gboolean     open_in_window,
                                 gint page_n);

G_END_DECLS

#endif /* __MIDORI_PANEL_H__ */
