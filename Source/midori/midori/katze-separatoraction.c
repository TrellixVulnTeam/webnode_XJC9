/* katze-separatoraction.c generated by valac 0.20.1, the Vala compiler
 * generated from katze-separatoraction.vala, do not modify */

/*
 Copyright (C) 2009-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>


#define KATZE_TYPE_SEPARATOR_ACTION (katze_separator_action_get_type ())
#define KATZE_SEPARATOR_ACTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), KATZE_TYPE_SEPARATOR_ACTION, KatzeSeparatorAction))
#define KATZE_SEPARATOR_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), KATZE_TYPE_SEPARATOR_ACTION, KatzeSeparatorActionClass))
#define KATZE_IS_SEPARATOR_ACTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), KATZE_TYPE_SEPARATOR_ACTION))
#define KATZE_IS_SEPARATOR_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), KATZE_TYPE_SEPARATOR_ACTION))
#define KATZE_SEPARATOR_ACTION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), KATZE_TYPE_SEPARATOR_ACTION, KatzeSeparatorActionClass))

typedef struct _KatzeSeparatorAction KatzeSeparatorAction;
typedef struct _KatzeSeparatorActionClass KatzeSeparatorActionClass;
typedef struct _KatzeSeparatorActionPrivate KatzeSeparatorActionPrivate;
#define _g_object_unref0(var) ((var == NULL) ? NULL : (var = (g_object_unref (var), NULL)))

struct _KatzeSeparatorAction {
	GtkAction parent_instance;
	KatzeSeparatorActionPrivate * priv;
};

struct _KatzeSeparatorActionClass {
	GtkActionClass parent_class;
};

struct _KatzeSeparatorActionPrivate {
	GtkMenuItem* menuitem;
	GtkToolItem* toolitem;
};


static gpointer katze_separator_action_parent_class = NULL;

GType katze_separator_action_get_type (void) G_GNUC_CONST;
#define KATZE_SEPARATOR_ACTION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), KATZE_TYPE_SEPARATOR_ACTION, KatzeSeparatorActionPrivate))
enum  {
	KATZE_SEPARATOR_ACTION_DUMMY_PROPERTY
};
static GtkWidget* katze_separator_action_real_create_menu_item (GtkAction* base);
static GtkWidget* katze_separator_action_real_create_tool_item (GtkAction* base);
KatzeSeparatorAction* katze_separator_action_new (void);
KatzeSeparatorAction* katze_separator_action_construct (GType object_type);
static void katze_separator_action_finalize (GObject* obj);


static GtkWidget* katze_separator_action_real_create_menu_item (GtkAction* base) {
	KatzeSeparatorAction * self;
	GtkWidget* result = NULL;
	GtkSeparatorMenuItem* _tmp0_;
	GtkMenuItem* _tmp1_;
#line 17 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self = (KatzeSeparatorAction*) base;
#line 18 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	_tmp0_ = (GtkSeparatorMenuItem*) gtk_separator_menu_item_new ();
#line 18 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	g_object_ref_sink (_tmp0_);
#line 18 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	_g_object_unref0 (self->priv->menuitem);
#line 18 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self->priv->menuitem = (GtkMenuItem*) _tmp0_;
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	_tmp1_ = self->priv->menuitem;
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	result = (GtkWidget*) _tmp1_;
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	return result;
#line 83 "katze-separatoraction.c"
}


static GtkWidget* katze_separator_action_real_create_tool_item (GtkAction* base) {
	KatzeSeparatorAction * self;
	GtkWidget* result = NULL;
	GtkSeparatorToolItem* _tmp0_;
	GtkToolItem* _tmp1_;
#line 22 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self = (KatzeSeparatorAction*) base;
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	_tmp0_ = (GtkSeparatorToolItem*) gtk_separator_tool_item_new ();
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	g_object_ref_sink (_tmp0_);
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	_g_object_unref0 (self->priv->toolitem);
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self->priv->toolitem = (GtkToolItem*) _tmp0_;
#line 24 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	_tmp1_ = self->priv->toolitem;
#line 24 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	result = (GtkWidget*) _tmp1_;
#line 24 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	return result;
#line 108 "katze-separatoraction.c"
}


KatzeSeparatorAction* katze_separator_action_construct (GType object_type) {
	KatzeSeparatorAction * self = NULL;
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self = (KatzeSeparatorAction*) g_object_new (object_type, NULL);
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	return self;
#line 118 "katze-separatoraction.c"
}


KatzeSeparatorAction* katze_separator_action_new (void) {
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	return katze_separator_action_construct (KATZE_TYPE_SEPARATOR_ACTION);
#line 125 "katze-separatoraction.c"
}


static void katze_separator_action_class_init (KatzeSeparatorActionClass * klass) {
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	katze_separator_action_parent_class = g_type_class_peek_parent (klass);
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	g_type_class_add_private (klass, sizeof (KatzeSeparatorActionPrivate));
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	GTK_ACTION_CLASS (klass)->create_menu_item = katze_separator_action_real_create_menu_item;
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	GTK_ACTION_CLASS (klass)->create_tool_item = katze_separator_action_real_create_tool_item;
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	G_OBJECT_CLASS (klass)->finalize = katze_separator_action_finalize;
#line 140 "katze-separatoraction.c"
}


static void katze_separator_action_instance_init (KatzeSeparatorAction * self) {
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self->priv = KATZE_SEPARATOR_ACTION_GET_PRIVATE (self);
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self->priv->menuitem = NULL;
#line 15 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self->priv->toolitem = NULL;
#line 151 "katze-separatoraction.c"
}


static void katze_separator_action_finalize (GObject* obj) {
	KatzeSeparatorAction * self;
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, KATZE_TYPE_SEPARATOR_ACTION, KatzeSeparatorAction);
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	_g_object_unref0 (self->priv->menuitem);
#line 15 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	_g_object_unref0 (self->priv->toolitem);
#line 13 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/katze/katze-separatoraction.vala"
	G_OBJECT_CLASS (katze_separator_action_parent_class)->finalize (obj);
#line 165 "katze-separatoraction.c"
}


GType katze_separator_action_get_type (void) {
	static volatile gsize katze_separator_action_type_id__volatile = 0;
	if (g_once_init_enter (&katze_separator_action_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = { sizeof (KatzeSeparatorActionClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) katze_separator_action_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (KatzeSeparatorAction), 0, (GInstanceInitFunc) katze_separator_action_instance_init, NULL };
		GType katze_separator_action_type_id;
		katze_separator_action_type_id = g_type_register_static (GTK_TYPE_ACTION, "KatzeSeparatorAction", &g_define_type_info, 0);
		g_once_init_leave (&katze_separator_action_type_id__volatile, katze_separator_action_type_id);
	}
	return katze_separator_action_type_id__volatile;
}


