/*

*/

#include "gtk3-compat.h"

#include "katze-utils.h"
#include "katze-array.h"
#include "cdosbrowser-core.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <string.h>

#if HAVE_CONFIG_H
    #include "config.h"
#endif

#if HAVE_UNISTD_H
    #include <unistd.h>
#endif

#define I_ g_intern_static_string

static void
proxy_toggle_button_toggled_cb (GtkToggleButton* button,
                                GObject*         object)
{
    gboolean toggled;
    const gchar* property;

    toggled = gtk_toggle_button_get_active (button);
    property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, toggled, NULL);
}

static void
proxy_file_file_set_cb (GtkFileChooser* button,
                        GObject*        object)
{
    const gchar* file = gtk_file_chooser_get_filename (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, file, NULL);
}

static void
proxy_folder_file_set_cb (GtkFileChooser* button,
                          GObject*        object)
{
    const gchar* file = gtk_file_chooser_get_current_folder (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, file, NULL);
}

static void
proxy_uri_file_set_cb (GtkFileChooser* button,
                       GObject*        object)
{
    const gchar* file = gtk_file_chooser_get_uri (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, file, NULL);
}

#if GTK_CHECK_VERSION (3, 2, 0)
static void
proxy_font_chooser_font_activated_cb (GtkFontChooser* chooser,
                                      GObject*        object)
{
    PangoFontFamily* font_family = gtk_font_chooser_get_font_family (GTK_FONT_CHOOSER (chooser));
    const gchar* font_name = pango_font_family_get_name (font_family);
    const gchar* property = g_object_get_data (G_OBJECT (chooser), "property");
    g_object_set (object, property, font_name, NULL);
}

static gboolean
proxy_font_chooser_filter_monospace_cb (PangoFontFamily* family,
                                        PangoFontFace*   face,
                                        gpointer         data)
{
    gboolean monospace = GPOINTER_TO_INT (data);
    return monospace == pango_font_family_is_monospace (family);
}
#else
static void
proxy_combo_box_text_changed_cb (GtkComboBoxText* button,
                                 GObject*         object)
{
    gchar* text = gtk_combo_box_text_get_active_text (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    g_object_set (object, property, text, NULL);
    g_free (text);
}
#endif

static gboolean
proxy_entry_focus_out_event_cb (GtkEntry*      entry,
                                GdkEventFocus* event,
                                GObject*       object);

static void
proxy_entry_activate_cb (GtkEntry* entry,
                         GObject*  object)
{
    const gchar* text = gtk_entry_get_text (entry);
    const gchar* property = g_object_get_data (G_OBJECT (entry), "property");
    g_object_set (object, property, text, NULL);
}

static gboolean
proxy_entry_focus_out_event_cb (GtkEntry*      entry,
                                GdkEventFocus* event,
                                GObject*       object)
{
    const gchar* text = gtk_entry_get_text (entry);
    const gchar* property = g_object_get_data (G_OBJECT (entry), "property");
    g_object_set (object, property, text, NULL);
    return FALSE;
}

static void
proxy_days_changed_cb (GtkComboBox* combo,
                       GObject*     object)
{
    gint active = gtk_combo_box_get_active (combo);
    const gchar* property = g_object_get_data (G_OBJECT (combo), "property");
    gint max_age;
    switch (active)
    {
    case 0: max_age =   0; break;
    case 1: max_age =   1; break;
    case 2: max_age =   7; break;
    case 3: max_age =  30; break;
    case 4: max_age = 365; break;
    default:
        max_age = 30;
    }
    g_object_set (object, property, max_age, NULL);
}

static void
proxy_spin_button_changed_cb (GtkSpinButton* button,
                              GObject*       object)
{
    GObjectClass* class = G_OBJECT_GET_CLASS (object);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    GParamSpec* pspec = g_object_class_find_property (class, property);
    if (G_PARAM_SPEC_TYPE (pspec) == G_TYPE_PARAM_INT)
    {
        gint value = gtk_spin_button_get_value_as_int (button);
        g_object_set (object, property, value, NULL);
    }
    else
    {
        gdouble value = gtk_spin_button_get_value (button);
        g_object_set (object, property, value, NULL);
    }
}

static void
proxy_combo_box_changed_cb (GtkComboBox* button,
                            GObject*     object)
{
    gint value = gtk_combo_box_get_active (button);
    const gchar* property = g_object_get_data (G_OBJECT (button), "property");
    gint custom_value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
                                         "katze-custom-value"));
    const gchar* custom_property = g_object_get_data (G_OBJECT (button),
                                                      "katze-custom-property");

    if (custom_value)
    {
        GtkWidget* child = gtk_bin_get_child (GTK_BIN (button));

        if (value == custom_value && GTK_IS_CELL_VIEW (child))
        {
            GtkWidget* entry = gtk_entry_new ();
            gchar* custom_text = katze_object_get_string (object, custom_property);
            if (custom_text && *custom_text)
                gtk_entry_set_text (GTK_ENTRY (entry), custom_text);
            g_free (custom_text);
            gtk_widget_show (entry);
            gtk_container_add (GTK_CONTAINER (button), entry);
            gtk_widget_grab_focus (entry);
            g_signal_connect (entry, "focus-out-event",
                G_CALLBACK (proxy_entry_focus_out_event_cb), object);
            g_object_set_data_full (G_OBJECT (entry), "property",
                                    g_strdup (custom_property), g_free);
        }
        else if (value != custom_value && GTK_IS_ENTRY (child))
        {
            g_signal_handlers_block_by_func (
                button, proxy_combo_box_changed_cb, object);
            /* Force the combo to change the item again */
            gtk_widget_destroy (child);
            gtk_combo_box_set_active (button, value + 1);
            gtk_combo_box_set_active (button, value);
            g_signal_handlers_unblock_by_func (
                button, proxy_combo_box_changed_cb, object);
        }
    }

    g_object_set (object, property, value, NULL);

    if (custom_value)
    {
        if (value == custom_value)
            gtk_widget_set_tooltip_text (GTK_WIDGET (button), NULL);
        else
        {
            gchar* custom_text = katze_object_get_string (object, custom_property);
            gtk_widget_set_tooltip_text (GTK_WIDGET (button), custom_text);
            g_free (custom_text);
        }
    }
}

static void
proxy_object_notify_boolean_cb (GObject*    object,
                                GParamSpec* pspec,
                                GtkWidget*  proxy)
{
    const gchar* property = g_object_get_data (G_OBJECT (proxy), "property");
    gboolean value = katze_object_get_boolean (object, property);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (proxy), value);
}

static void
proxy_object_notify_string_cb (GObject*    object,
                               GParamSpec* pspec,
                               GtkWidget*  proxy)
{
    const gchar* property = g_object_get_data (G_OBJECT (proxy), "property");
    gchar* value = katze_object_get_string (object, property);
    gtk_entry_set_text (GTK_ENTRY (proxy), value);
    g_free (value);
}

static void
proxy_widget_boolean_destroy_cb (GtkWidget* proxy,
                                 GObject*   object)
{
    g_signal_handlers_disconnect_by_func (object,
        proxy_object_notify_boolean_cb, proxy);
}

static void
proxy_widget_string_destroy_cb (GtkWidget* proxy,
                                 GObject*  object)
{
    g_signal_handlers_disconnect_by_func (object,
        proxy_object_notify_string_cb, proxy);
}

/**
 * katze_property_proxy:
 * @object: a #GObject
 * @property: the name of a property
 * @hint: a special hint
 *
 * Create a widget of an appropriate type to represent the specified
 * object's property. If the property is writable changes of the value
 * through the widget will be reflected in the value of the property.
 *
 * Supported values for @hint are as follows:
 *     "blurb": the blurb of the property will be used to provide a kind
 *         of label, instead of the name.
 *     "file": the widget created will be particularly suitable for
 *         choosing an existing filename.
 *     "folder": the widget created will be particularly suitable for
 *         choosing an existing folder.
 *     "uri": the widget created will be particularly suitable for
 *         choosing an existing filename, encoded as an URI.
 *     "font": the widget created will be particularly suitable for
 *         choosing a variable-width font from installed fonts.
 *     Since 0.1.6 the following hints are also supported:
 *     "toggle": the widget created will be an empty toggle button. This
 *         is only supported with boolean properties.
 *         Since 0.1.8 "toggle" creates GtkCheckButton widgets without checkmarks.
 *     Since 0.2.0 the following hints are also supported:
 *     "font-monospace": the widget created will be particularly suitable for
 *         choosing a fixed-width font from installed fonts.
 *     Since 0.2.1 the following hints are also supported:
 *     "application-TYPE": the widget created will be particularly suitable
 *         for choosing an application to open TYPE files, ie. "text/plain".
 *     "application-CATEGORY": the widget created will be particularly suitable
 *         for choosing an application to open CATEGORY files, ie. "Network".
 *      Since 0.5.8 the CATEGORY hint is no longer supported.
 *     "custom-PROPERTY": the last value of an enumeration will be the "custom"
 *         value, where the user may enter text freely, which then updates
 *         the property PROPERTY instead. This applies only to enumerations.
 *         Since 0.4.1 mnemonics are automatically stripped.
 *     Since 0.2.9 the following hints are also supported:
 *     "languages": the widget will be particularly suitable for choosing
 *         multiple language codes, ie. "de,en_GB".
 *     Since 0.3.6 the following hints are also supported:
 *     "address": the widget will be particularly suitable for typing
 *         a valid URI or IP address and highlight errors.
 *     Since 0.4.0 the following hints are also supported:
 *     "days": the widget will be particularly suitable for choosing
 *         a period of time in days.
 *
 * Any other values for @hint are silently ignored.
 *
 * Since 0.1.2 strings without hints and booleans are truly synchronous
 *     including property changes causing the proxy to be updated.
 *
 * Since 0.2.1 the proxy may contain a label if the platform
 *     has according widgets.
 *
 * Return value: (transfer full): a new widget
 **/
GtkWidget*
katze_property_proxy (gpointer     object,
                      const gchar* property,
                      const gchar* hint)
{
    GObjectClass* class;
    GParamSpec* pspec;
    GType type;
    const gchar* nick;
    const gchar* _hint;
    GtkWidget* widget;
    gchar* string;

    g_return_val_if_fail (G_IS_OBJECT (object), NULL);

    class = G_OBJECT_GET_CLASS (object);
    pspec = g_object_class_find_property (class, property);
    if (!pspec)
    {
        g_warning (_("Property '%s' is invalid for %s"),
                   property, G_OBJECT_CLASS_NAME (class));
        return gtk_label_new (property);
    }

    type = G_PARAM_SPEC_TYPE (pspec);
    nick = g_param_spec_get_nick (pspec);
    _hint = g_intern_string (hint);
    if (_hint == I_("blurb"))
        nick = g_param_spec_get_blurb (pspec);
    string = NULL;
    if (type == G_TYPE_PARAM_BOOLEAN)
    {
        gchar* notify_property;
        gboolean toggled = katze_object_get_boolean (object, property);


        widget = gtk_check_button_new ();
        if (_hint == I_("toggle"))
            gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (widget), FALSE);
        else
            gtk_button_set_label (GTK_BUTTON (widget), gettext (nick));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), toggled);

        g_signal_connect (widget, "toggled",
                          G_CALLBACK (proxy_toggle_button_toggled_cb), object);
        notify_property = g_strdup_printf ("notify::%s", property);
        g_signal_connect (object, notify_property,
            G_CALLBACK (proxy_object_notify_boolean_cb), widget);
        g_signal_connect (widget, "destroy",
            G_CALLBACK (proxy_widget_boolean_destroy_cb), object);
        g_free (notify_property);
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == I_("file"))
    {
        string = katze_object_get_string (object, property);

        widget = gtk_file_chooser_button_new (_("Choose file"),
            GTK_FILE_CHOOSER_ACTION_OPEN);

        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget),
                                       string ? string : "");
        if (pspec->flags & G_PARAM_WRITABLE)
            g_signal_connect (widget, "selection-changed",
                              G_CALLBACK (proxy_file_file_set_cb), object);
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == I_("folder"))
    {
        string = katze_object_get_string (object, property);

        widget = gtk_file_chooser_button_new (_("Choose folder"),
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget),
                                             string ? string : "");
        if (pspec->flags & G_PARAM_WRITABLE)
            g_signal_connect (widget, "selection-changed",
                              G_CALLBACK (proxy_folder_file_set_cb), object);
    }
    else if (type == G_TYPE_PARAM_STRING && _hint == I_("uri"))
    {
        string = katze_object_get_string (object, property);

        widget = gtk_file_chooser_button_new (_("Choose file"),
            GTK_FILE_CHOOSER_ACTION_OPEN);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (widget),
                                  string ? string : "");
        g_signal_connect (widget, "file-set",
                          G_CALLBACK (proxy_uri_file_set_cb), object);
    }
    else if (type == G_TYPE_PARAM_STRING && (_hint == I_("font")
        || _hint == I_("font-monospace")))
    {
        string = katze_object_get_string (object, property);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        /* 'sans' and 'sans-serif' are presumably the same */
        if (!g_strcmp0 (string, "sans-serif"))
            katze_assign (string, g_strdup ("sans"));
        gboolean monospace = _hint == I_("font-monospace");

        #if GTK_CHECK_VERSION (3, 2, 0)
        widget = gtk_font_button_new ();
        gtk_font_button_set_show_size (GTK_FONT_BUTTON (widget), FALSE);
        gtk_font_chooser_set_font (GTK_FONT_CHOOSER (widget), string);
        /* font-activated doesn't work with at least GTK+ 3.8.4 */
        g_signal_connect (widget, "font-set",
                          G_CALLBACK (proxy_font_chooser_font_activated_cb), object);
        gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (widget),
            (GtkFontFilterFunc)proxy_font_chooser_filter_monospace_cb, GINT_TO_POINTER (monospace), NULL);
        #else
        GtkComboBox* combo;
        gint n_families, i;
        PangoContext* context;
        PangoFontFamily** families;

        widget = gtk_combo_box_text_new ();
        combo = GTK_COMBO_BOX (widget);
        context = gtk_widget_get_pango_context (widget);
        pango_context_list_families (context, &families, &n_families);
        if (string)
        {
            gint j = 0;
            for (i = 0; i < n_families; i++)
            {
                if (monospace != pango_font_family_is_monospace (families[i]))
                    continue;
                const gchar* font = pango_font_family_get_name (families[i]);
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), font);
                if (!g_ascii_strcasecmp (font, string))
                    gtk_combo_box_set_active (combo, j);
                j++;
            }
        }
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (
            gtk_combo_box_get_model (combo)), 0, GTK_SORT_ASCENDING);
        g_signal_connect (widget, "changed",
                          G_CALLBACK (proxy_combo_box_text_changed_cb), object);
        g_free (families);
        #endif
    }
    else if (type == G_TYPE_PARAM_STRING)
    {
        gchar* notify_property;

        if (_hint == I_("address"))
            widget = katze_uri_entry_new (NULL);
        else
            widget = gtk_entry_new ();
        g_object_get (object, property, &string, NULL);
        if (!string)
            string = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
        if (!(string && *string) && _hint == I_("languages"))
        {
            gchar* lang = g_strjoinv (",", (gchar**)g_get_language_names ());
            if (g_str_has_suffix (lang, ",C"))
            {
                string = g_strndup (lang, strlen (lang) - 2);
                g_free (lang);
            }
            else
                string = lang;
        }
        gtk_entry_set_text (GTK_ENTRY (widget), string ? string : "");
        g_signal_connect (widget, "activate",
                          G_CALLBACK (proxy_entry_activate_cb), object);
        g_signal_connect (widget, "focus-out-event",
                          G_CALLBACK (proxy_entry_focus_out_event_cb), object);
        notify_property = g_strdup_printf ("notify::%s", property);
        g_signal_connect (object, notify_property,
            G_CALLBACK (proxy_object_notify_string_cb), widget);
        g_signal_connect (widget, "destroy",
            G_CALLBACK (proxy_widget_string_destroy_cb), object);
        g_free (notify_property);
    }
    else if (type == G_TYPE_PARAM_DOUBLE)
    {
        gdouble value;
        g_object_get (object, property, &value, NULL);

        widget = gtk_spin_button_new_with_range (
            G_PARAM_SPEC_DOUBLE (pspec)->minimum,
            G_PARAM_SPEC_DOUBLE (pspec)->maximum, 1);
        /* Keep it narrow, 5 + 2 digits are usually fine */
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 5 + 2);
        gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 2);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
        gtk_spin_button_set_increments (GTK_SPIN_BUTTON (widget), 0.1, -0.1);
        g_signal_connect (widget, "value-changed",
                          G_CALLBACK (proxy_spin_button_changed_cb), object);
    }
    else if (type == G_TYPE_PARAM_FLOAT)
    {
        gfloat value;
        g_object_get (object, property, &value, NULL);

        widget = gtk_spin_button_new_with_range (
            G_PARAM_SPEC_FLOAT (pspec)->minimum,
            G_PARAM_SPEC_FLOAT (pspec)->maximum, 1);
        /* Keep it narrow, 5 + 2 digits are usually fine */
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 5 + 2);
        gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 2);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
        g_signal_connect (widget, "value-changed",
                          G_CALLBACK (proxy_spin_button_changed_cb), object);
    }
    else if (type == G_TYPE_PARAM_INT && _hint == I_("days"))
    {
        gint value = katze_object_get_int (object, property);
        gint active;
        widget = gtk_combo_box_text_new ();
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("1 hour"));
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("1 day"));
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("1 week"));
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("1 month"));
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), _("1 year"));
        switch (value)
        {
        case   0: active = 0; break;
        case   1: active = 1; break;
        case   7: active = 2; break;
        case  30: active = 3; break;
        case 365: active = 4; break;
        default:
            active = 3;
        }
        gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);
        g_signal_connect (widget, "changed",
            G_CALLBACK (proxy_days_changed_cb), object);
    }
    else if (type == G_TYPE_PARAM_INT)
    {
        gint value = katze_object_get_int (object, property);

        widget = gtk_spin_button_new_with_range (
            G_PARAM_SPEC_INT (pspec)->minimum,
            G_PARAM_SPEC_INT (pspec)->maximum, 1);

        /* Keep it narrow, 5 digits are usually fine */
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
        g_signal_connect (widget, "value-changed",
                          G_CALLBACK (proxy_spin_button_changed_cb), object);
    }
    else if (type == G_TYPE_PARAM_ENUM)
    {
        guint i;
        GEnumClass* enum_class = G_ENUM_CLASS (
            g_type_class_ref (pspec->value_type));
        gint value = katze_object_get_enum (object, property);
        const gchar* custom = NULL;

        if (hint && g_str_has_prefix (hint, "custom-"))
            custom = &hint[7];

        widget = gtk_combo_box_text_new ();
        for (i = 0; i < enum_class->n_values; i++)
        {
            const gchar* raw_label = gettext (enum_class->values[i].value_nick);
            gchar* label = katze_strip_mnemonics (raw_label);
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), label);
            g_free (label);
        }
        gtk_combo_box_set_active (GTK_COMBO_BOX (widget), value);
        g_signal_connect (widget, "changed",
                          G_CALLBACK (proxy_combo_box_changed_cb), object);
        if (custom)
        {
            gchar* custom_text = katze_object_get_string (object, custom);

            if (value == (gint)(enum_class->n_values - 1))
            {
                GtkWidget* entry = gtk_entry_new ();
                gchar* text = katze_object_get_string (object, custom);
                if (text && *text)
                    gtk_entry_set_text (GTK_ENTRY (entry), text);
                gtk_widget_show (entry);
                gtk_container_add (GTK_CONTAINER (widget), entry);
                g_signal_connect (entry, "focus-out-event",
                    G_CALLBACK (proxy_entry_focus_out_event_cb), object);
                g_object_set_data_full (G_OBJECT (entry), "property",
                                        g_strdup (custom), g_free);
                gtk_widget_set_tooltip_text (widget, NULL);
            }
            else
                gtk_widget_set_tooltip_text (widget, custom_text);

            g_free (custom_text);

            g_object_set_data (G_OBJECT (widget), "katze-custom-value",
                               GINT_TO_POINTER (enum_class->n_values - 1));
            g_object_set_data (G_OBJECT (widget), "katze-custom-property",
                               (gpointer)custom);
        }
        g_type_class_unref (enum_class);
    }
    else
        widget = gtk_label_new (gettext (nick));
    g_free (string);

    gtk_widget_set_sensitive (widget, pspec->flags & G_PARAM_WRITABLE);

    g_object_set_data_full (G_OBJECT (widget), "property",
                            g_strdup (property), g_free);

    return widget;
}

typedef struct
{
     GtkWidget* widget;
     KatzeMenuPos position;
} KatzePopupInfo;

static void
katze_widget_popup_position_menu (GtkMenu*  menu,
                                  gint*     x,
                                  gint*     y,
                                  gboolean* push_in,
                                  gpointer  user_data)
{
    gint wx, wy;
    gint menu_width;
    GtkAllocation allocation;
    GtkRequisition menu_req;
    GtkRequisition widget_req;
    KatzePopupInfo* info = user_data;
    GtkWidget* widget = info->widget;
    GdkWindow* window = gtk_widget_get_window (widget);
    gint widget_height;

    if (!window)
        return;

    #if !GTK_CHECK_VERSION (3, 0, 0)
    if (GTK_IS_ENTRY (widget))
        window = gdk_window_get_parent (window);
    #endif

    /* Retrieve size and position of both widget and menu */
    gtk_widget_get_allocation (widget, &allocation);
    gdk_window_get_origin (window, &wx, &wy);
    wx += allocation.x;
    wy += allocation.y;
    #if GTK_CHECK_VERSION (3, 0, 0)
    gtk_widget_get_preferred_size (GTK_WIDGET (menu), &menu_req, NULL);
    gtk_widget_get_preferred_size (widget, &widget_req, NULL);
    #else
    gtk_widget_size_request (GTK_WIDGET (menu), &menu_req);
    gtk_widget_size_request (widget, &widget_req);
    #endif
    menu_width = menu_req.width;
    widget_height = widget_req.height; /* Better than allocation.height */

    /* Calculate menu position */
    if (info->position == KATZE_MENU_POSITION_CURSOR)
        ; /* Do nothing? */
    else if (info->position == KATZE_MENU_POSITION_RIGHT)
    {
        *x = wx + allocation.width - menu_width;
        *y = wy + widget_height;
    } else if (info->position == KATZE_MENU_POSITION_LEFT)
    {
        *x = wx;
        *y = wy + widget_height;
    }

    *push_in = TRUE;
}

/**
 * katze_widget_popup:
 * @widget: a widget
 * @menu: the menu to popup
 * @event: a button event, or %NULL
 * @pos: the preferred positioning
 *
 * Pops up the given menu relative to @widget. Use this
 * instead of writing custom positioning functions.
 *
 * Return value: a new label widget
 **/
void
katze_widget_popup (GtkWidget*      widget,
                    GtkMenu*        menu,
                    GdkEventButton* event,
                    KatzeMenuPos    pos)
{
    int button, event_time;
    if (event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time ();
    }

    if (!gtk_menu_get_attach_widget (menu))
        gtk_menu_attach_to_widget (menu, widget, NULL);


    if (widget)
    {
        KatzePopupInfo info = { widget, pos };
        gtk_menu_popup (menu, NULL, NULL,
                        katze_widget_popup_position_menu, &info,
                        button, event_time);
    }
    else
        gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button, event_time);
}

/**
 * katze_image_menu_item_new_ellipsized:
 * @label: a label or %NULL
 *
 * Creates an image menu item where the label is
 * reasonably ellipsized for you.
 *
 * Return value: (transfer full): a new label widget
 **/
GtkWidget*
katze_image_menu_item_new_ellipsized (const gchar* label)
{
    GtkWidget* menuitem;
    GtkWidget* label_widget;

    menuitem = gtk_image_menu_item_new ();
    label_widget = gtk_label_new (label);
    /* FIXME: Should text direction be respected here? */
    gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.0);
    gtk_label_set_max_width_chars (GTK_LABEL (label_widget), 50);
    gtk_label_set_ellipsize (GTK_LABEL (label_widget), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_show (label_widget);
    gtk_container_add (GTK_CONTAINER (menuitem), label_widget);

    return menuitem;
}

/**
 * katze_tree_view_get_selected_iter:
 * @treeview: a #GtkTreeView
 * @model: a pointer to store the model, or %NULL
 * @iter: a pointer to store the iter, or %NULL
 *
 * Determines whether there is a selection in @treeview
 * and sets the @iter to the current selection.
 *
 * If there is a selection and @model is not %NULL, it is
 * set to the model, mainly for convenience.
 *
 * Either @model or @iter or both can be %NULL in which case
 * no value will be assigned in any case.
 *
 * Return value: %TRUE if there is a selection
 *
 * Since: 0.1.3
 **/
gboolean
katze_tree_view_get_selected_iter (GtkTreeView*   treeview,
                                   GtkTreeModel** model,
                                   GtkTreeIter*   iter)
{
    GtkTreeSelection* selection;

    g_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), FALSE);

    if ((selection = gtk_tree_view_get_selection (treeview)))
        if (gtk_tree_selection_get_selected (selection, model, iter))
            return TRUE;
    return FALSE;
}

void
katze_bookmark_populate_tree_view (KatzeArray*   array,
                                   GtkTreeStore* model,
                                   GtkTreeIter*  parent)
{
    KatzeItem* child;
    GtkTreeIter iter;
    GtkTreeIter root_iter;

    KATZE_ARRAY_FOREACH_ITEM (child, array)
    {
        if (KATZE_ITEM_IS_BOOKMARK (child))
        {
            //gchar* tooltip = g_markup_escape_text (katze_item_get_uri (child), -1);
            gchar* tooltip = g_markup_escape_text (g_locale_to_utf8(katze_item_get_uri (child),-1,0,0,0), -1);
            gtk_tree_store_insert_with_values (model, NULL, parent,
                                               0, 0, child, 1, tooltip, -1);
            g_free (tooltip);
        }
        else
        {
            gtk_tree_store_insert_with_values (model, &root_iter, parent,
                                               0, 0, child, -1);
            /* That's an invisible dummy, so we always have an expander */
            gtk_tree_store_insert_with_values (model, &iter, &root_iter,
                                               0, 0, NULL, -1);
        }
    }
}

/**
 * katze_strip_mnemonics:
 * @original: a string with mnemonics
 *
 * Parses the given string for mnemonics in the form
 * "B_utton" or "Button (_U)" and returns a string
 * without any mnemonics.
 *
 * Return value: a newly allocated string without mnemonics
 *
 * Since: 0.1.8
 **/
gchar*
katze_strip_mnemonics (const gchar* original)
{
  /* A copy of _gtk_toolbar_elide_underscores
     Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
     Copied from GTK+ 2.17.1 */
  gchar *q, *result;
  const gchar *p, *end;
  gsize len;
  gboolean last_underscore;

  if (!original)
    return NULL;

  len = strlen (original);
  q = result = g_malloc (len + 1);
  last_underscore = FALSE;

  end = original + len;
  for (p = original; p < end; p++)
    {
      if (!last_underscore && *p == '_')
	last_underscore = TRUE;
      else
	{
	  last_underscore = FALSE;
	  if (original + 2 <= p && p + 1 <= end &&
              p[-2] == '(' && p[-1] == '_' && p[0] != '_' && p[1] == ')')
	    {
	      q--;
	      *q = '\0';
	      p++;
	    }
	  else
	    *q++ = *p;
	}
    }

  if (last_underscore)
    *q++ = '_';

  *q = '\0';

  return result;
}

const gchar*
katze_skip_whitespace (const gchar* str)
{
    if (str == NULL)
        return NULL;
    while (*str == ' ' || *str == '\t' || *str == '\n')
        str++;
    return str;
}

/**
 * katze_object_get_boolean:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the boolean value of the specified property.
 *
 * Return value: a boolean
 **/
gboolean
katze_object_get_boolean (gpointer     object,
                          const gchar* property)
{
    gboolean value = FALSE;

    g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_int:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the integer value of the specified property.
 *
 * Return value: an integer
 **/
gint
katze_object_get_int (gpointer     object,
                      const gchar* property)
{
    gint value = -1;

    g_return_val_if_fail (G_IS_OBJECT (object), -1);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_enum:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the enum value of the specified property.
 *
 * Return value: an enumeration
 **/
gint
katze_object_get_enum (gpointer     object,
                       const gchar* property)
{
    gint value = -1;

    g_return_val_if_fail (G_IS_OBJECT (object), -1);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_string:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the string value of the specified property.
 *
 * Return value: a newly allocated string
 **/
gchar*
katze_object_get_string (gpointer     object,
                         const gchar* property)
{
    gchar* value = NULL;

    g_return_val_if_fail (G_IS_OBJECT (object), NULL);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_object_get_object:
 * @object: a #GObject
 * @property: the name of the property to get
 *
 * Retrieve the object value of the specified property.
 *
 * Return value: (transfer none): an object
 **/
gpointer
katze_object_get_object (gpointer     object,
                         const gchar* property)
{
    GObject* value = NULL;

    g_return_val_if_fail (G_IS_OBJECT (object), NULL);
    /* FIXME: Check value type */

    g_object_get (object, property, &value, NULL);
    return value;
}

/**
 * katze_mkdir_with_parents:
 * @pathname: a pathname in the GLib file name encoding
 * @mode: permissions to use for newly created directories
 *
 * Create a directory if it doesn't already exist. Create intermediate
 * parent directories as needed, too.
 *
 * Returns: 0 if the directory already exists, or was successfully
 * created. Returns -1 if an error occurred, with errno set.
 *
 * Since: 0.2.1
 */
int
katze_mkdir_with_parents (const gchar* pathname,
                          int          mode)
{
    midori_paths_mkdir_with_parents (pathname, mode);
    return 0;
}

static void
katze_uri_entry_changed_cb (GtkWidget* entry,
                            GtkWidget* other_widget)
{
    const gchar* uri = gtk_entry_get_text (GTK_ENTRY (entry));
    gboolean valid = midori_uri_is_location (uri);
    if (!valid && g_object_get_data (G_OBJECT (entry), "allow_%s"))
        valid = uri && g_str_has_prefix (uri, "%s");
    if (!valid)
        valid = midori_uri_is_ip_address (uri);

    #if GTK_CHECK_VERSION (3, 2, 0)
    g_object_set_data (G_OBJECT (entry), "invalid", GINT_TO_POINTER (uri && *uri && !valid));
    gtk_widget_queue_draw (entry);
    #else
    if (uri && *uri && !valid)
    {
        GdkColor bg_color = { 0 };
        GdkColor fg_color = { 0 };
        gdk_color_parse ("#ef7070", &bg_color);
        gdk_color_parse ("#000", &fg_color);
        gtk_widget_modify_base (entry, GTK_STATE_NORMAL, &bg_color);
        gtk_widget_modify_text (entry, GTK_STATE_NORMAL, &fg_color);
    }
    else
    {
        gtk_widget_modify_base (entry, GTK_STATE_NORMAL, NULL);
        gtk_widget_modify_text (entry, GTK_STATE_NORMAL, NULL);
    }
    #endif

    if (other_widget != NULL)
        gtk_widget_set_sensitive (other_widget, valid);
}

#if GTK_CHECK_VERSION (3, 2, 0)
static gboolean
katze_uri_entry_draw_cb (GtkWidget* entry,
                         cairo_t*   cr,
                         GtkWidget* other_widget)
{
    const GdkRGBA color = { 0.9, 0., 0., 1. };
    double width = gtk_widget_get_allocated_width (entry);
    double height = gtk_widget_get_allocated_height (entry);

    if (!g_object_get_data (G_OBJECT (entry), "invalid"))
        return FALSE;

    /* FIXME: error-underline-color requires GtkTextView */
    gdk_cairo_set_source_rgba (cr, &color);

    pango_cairo_show_error_underline (cr, width * 0.15, height / 1.9,
        width * 0.75, height / 1.9 / 2);
    return TRUE;
}
#endif

/**
 * katze_uri_entry_new:
 * @other_widget: a #GtkWidget, or %NULL
 *
 * Creates an entry that validates the typed URI.
 *
 * If @other_widget is given, it will become insensitive if
 * the input is not a valid URI.
 *
 * Returns: (transfer full): a #GtkEntry
 *
 * Since: 0.3.6
 */
GtkWidget*
katze_uri_entry_new (GtkWidget* other_widget)
{
    GtkWidget* entry = gtk_entry_new ();
    #if GTK_CHECK_VERSION (3, 6, 0)
    gtk_entry_set_input_purpose (GTK_ENTRY (entry), GTK_INPUT_PURPOSE_URL);
    #endif

    gtk_entry_set_icon_from_gicon (GTK_ENTRY (entry), GTK_ENTRY_ICON_PRIMARY,
        g_themed_icon_new_with_default_fallbacks ("text-html-symbolic"));
    g_signal_connect (entry, "changed",
        G_CALLBACK (katze_uri_entry_changed_cb), other_widget);
    #if GTK_CHECK_VERSION (3, 2, 0)
    g_signal_connect_after (entry, "draw",
        G_CALLBACK (katze_uri_entry_draw_cb), other_widget);
    #endif
    return entry;
}

void
katze_widget_add_class (GtkWidget*   widget,
                        const gchar* class_name)
{
    #if GTK_CHECK_VERSION (3,0,0)
    GtkStyleContext* context = gtk_widget_get_style_context (widget);
    gtk_style_context_add_class (context, class_name);
    #endif
}

/**
 * katze_assert_str_equal:
 * @input: a string
 * @result: a string
 * @expected: a string
 *
 * Compares the two strings for equality, with verbose errors.
 *
 * Since: 0.4.3
 */
void
katze_assert_str_equal (const gchar* input,
                        const gchar* result,
                        const gchar* expected)
{
    if (g_strcmp0 (result, expected))
    {
        g_error ("Input: %s\nExpected: %s\nResult: %s",
                 input ? input : "NULL",
                 expected ? expected : "NULL",
                 result ? result : "NULL");
    }
}

void
katze_window_set_sensible_default_size (GtkWindow* window)
{
    GdkScreen* screen;
    GdkRectangle monitor;
    gint width, height;

    g_return_if_fail (GTK_IS_WINDOW (window));

    screen = gtk_window_get_screen (window);
    gdk_screen_get_monitor_geometry (screen, 0, &monitor);
    width = monitor.width / 1.7;
    height = monitor.height / 1.7;
    gtk_window_set_default_size (window, width, height);
    /* 700x100 is the approximate useful minimum dimensions */
    gtk_widget_set_size_request (GTK_WIDGET (window), 700, 100);
}

