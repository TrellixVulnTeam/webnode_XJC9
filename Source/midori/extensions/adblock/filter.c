/* filter.c generated by valac 0.20.1, the Vala compiler
 * generated from filter.vala, do not modify */

/*
 Copyright (C) 2009-2014 Christian Dywan <christian@twotoasts.de>
 Copyright (C) 2009-2012 Alexander Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>


#define ADBLOCK_TYPE_FEATURE (adblock_feature_get_type ())
#define ADBLOCK_FEATURE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ADBLOCK_TYPE_FEATURE, AdblockFeature))
#define ADBLOCK_FEATURE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), ADBLOCK_TYPE_FEATURE, AdblockFeatureClass))
#define ADBLOCK_IS_FEATURE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ADBLOCK_TYPE_FEATURE))
#define ADBLOCK_IS_FEATURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ADBLOCK_TYPE_FEATURE))
#define ADBLOCK_FEATURE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), ADBLOCK_TYPE_FEATURE, AdblockFeatureClass))

typedef struct _AdblockFeature AdblockFeature;
typedef struct _AdblockFeatureClass AdblockFeatureClass;
typedef struct _AdblockFeaturePrivate AdblockFeaturePrivate;

#define ADBLOCK_TYPE_DIRECTIVE (adblock_directive_get_type ())

#define ADBLOCK_TYPE_FILTER (adblock_filter_get_type ())
#define ADBLOCK_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ADBLOCK_TYPE_FILTER, AdblockFilter))
#define ADBLOCK_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), ADBLOCK_TYPE_FILTER, AdblockFilterClass))
#define ADBLOCK_IS_FILTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ADBLOCK_TYPE_FILTER))
#define ADBLOCK_IS_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ADBLOCK_TYPE_FILTER))
#define ADBLOCK_FILTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), ADBLOCK_TYPE_FILTER, AdblockFilterClass))

typedef struct _AdblockFilter AdblockFilter;
typedef struct _AdblockFilterClass AdblockFilterClass;
typedef struct _AdblockFilterPrivate AdblockFilterPrivate;

#define ADBLOCK_TYPE_OPTIONS (adblock_options_get_type ())
#define ADBLOCK_OPTIONS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ADBLOCK_TYPE_OPTIONS, AdblockOptions))
#define ADBLOCK_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), ADBLOCK_TYPE_OPTIONS, AdblockOptionsClass))
#define ADBLOCK_IS_OPTIONS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ADBLOCK_TYPE_OPTIONS))
#define ADBLOCK_IS_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ADBLOCK_TYPE_OPTIONS))
#define ADBLOCK_OPTIONS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), ADBLOCK_TYPE_OPTIONS, AdblockOptionsClass))

typedef struct _AdblockOptions AdblockOptions;
typedef struct _AdblockOptionsClass AdblockOptionsClass;
#define _g_object_unref0(var) ((var == NULL) ? NULL : (var = (g_object_unref (var), NULL)))
#define _g_hash_table_unref0(var) ((var == NULL) ? NULL : (var = (g_hash_table_unref (var), NULL)))
#define _g_free0(var) (var = (g_free (var), NULL))

typedef enum  {
	ADBLOCK_DIRECTIVE_ALLOW,
	ADBLOCK_DIRECTIVE_BLOCK
} AdblockDirective;

struct _AdblockFeature {
	GObject parent_instance;
	AdblockFeaturePrivate * priv;
};

struct _AdblockFeatureClass {
	GObjectClass parent_class;
	gboolean (*header) (AdblockFeature* self, const gchar* key, const gchar* value);
	gboolean (*parsed) (AdblockFeature* self, GFile* file);
	AdblockDirective* (*match) (AdblockFeature* self, const gchar* request_uri, const gchar* page_uri, GError** error);
	void (*clear) (AdblockFeature* self);
};

struct _AdblockFilter {
	AdblockFeature parent_instance;
	AdblockFilterPrivate * priv;
	GHashTable* rules;
};

struct _AdblockFilterClass {
	AdblockFeatureClass parent_class;
	void (*insert) (AdblockFilter* self, const gchar* sig, GRegex* regex);
	GRegex* (*lookup) (AdblockFilter* self, const gchar* sig);
	guint (*size) (AdblockFilter* self);
};

struct _AdblockFilterPrivate {
	AdblockOptions* optslist;
};


static gpointer adblock_filter_parent_class = NULL;

GType adblock_feature_get_type (void) G_GNUC_CONST;
GType adblock_directive_get_type (void) G_GNUC_CONST;
GType adblock_filter_get_type (void) G_GNUC_CONST;
GType adblock_options_get_type (void) G_GNUC_CONST;
#define ADBLOCK_FILTER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ADBLOCK_TYPE_FILTER, AdblockFilterPrivate))
enum  {
	ADBLOCK_FILTER_DUMMY_PROPERTY
};
void adblock_filter_insert (AdblockFilter* self, const gchar* sig, GRegex* regex);
static void adblock_filter_real_insert (AdblockFilter* self, const gchar* sig, GRegex* regex);
GRegex* adblock_filter_lookup (AdblockFilter* self, const gchar* sig);
static GRegex* adblock_filter_real_lookup (AdblockFilter* self, const gchar* sig);
guint adblock_filter_size (AdblockFilter* self);
static guint adblock_filter_real_size (AdblockFilter* self);
AdblockFilter* adblock_filter_construct (GType object_type, AdblockOptions* options);
AdblockFeature* adblock_feature_construct (GType object_type);
void adblock_feature_clear (AdblockFeature* self);
static void adblock_filter_real_clear (AdblockFeature* base);
static void _g_free0_ (gpointer var);
static void _g_regex_unref0_ (gpointer var);
gboolean adblock_filter_check_rule (AdblockFilter* self, GRegex* regex, const gchar* pattern, const gchar* request_uri, const gchar* page_uri, GError** error);
gchar* adblock_options_lookup (AdblockOptions* self, const gchar* sig);
void adblock_debug (const gchar* format, ...);
static void adblock_filter_finalize (GObject* obj);


static gpointer _g_regex_ref0 (gpointer self) {
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	return self ? g_regex_ref (self) : NULL;
#line 128 "filter.c"
}


static void adblock_filter_real_insert (AdblockFilter* self, const gchar* sig, GRegex* regex) {
	GHashTable* _tmp0_;
	const gchar* _tmp1_;
	gchar* _tmp2_;
	GRegex* _tmp3_;
	GRegex* _tmp4_;
#line 18 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_if_fail (sig != NULL);
#line 18 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_if_fail (regex != NULL);
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp0_ = self->rules;
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp1_ = sig;
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp2_ = g_strdup (_tmp1_);
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp3_ = regex;
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp4_ = _g_regex_ref0 (_tmp3_);
#line 19 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_hash_table_insert (_tmp0_, _tmp2_, _tmp4_);
#line 154 "filter.c"
}


void adblock_filter_insert (AdblockFilter* self, const gchar* sig, GRegex* regex) {
#line 18 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_if_fail (self != NULL);
#line 18 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	ADBLOCK_FILTER_GET_CLASS (self)->insert (self, sig, regex);
#line 163 "filter.c"
}


static GRegex* adblock_filter_real_lookup (AdblockFilter* self, const gchar* sig) {
	GRegex* result = NULL;
	GHashTable* _tmp0_;
	const gchar* _tmp1_;
	gconstpointer _tmp2_ = NULL;
	GRegex* _tmp3_;
#line 22 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (sig != NULL, NULL);
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp0_ = self->rules;
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp1_ = sig;
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp2_ = g_hash_table_lookup (_tmp0_, _tmp1_);
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp3_ = _g_regex_ref0 ((GRegex*) _tmp2_);
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	result = _tmp3_;
#line 23 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	return result;
#line 187 "filter.c"
}


GRegex* adblock_filter_lookup (AdblockFilter* self, const gchar* sig) {
#line 22 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (self != NULL, NULL);
#line 22 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	return ADBLOCK_FILTER_GET_CLASS (self)->lookup (self, sig);
#line 196 "filter.c"
}


static guint adblock_filter_real_size (AdblockFilter* self) {
	guint result = 0U;
	GHashTable* _tmp0_;
	guint _tmp1_ = 0U;
#line 27 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp0_ = self->rules;
#line 27 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp1_ = g_hash_table_size (_tmp0_);
#line 27 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	result = _tmp1_;
#line 27 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	return result;
#line 212 "filter.c"
}


guint adblock_filter_size (AdblockFilter* self) {
#line 26 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (self != NULL, 0U);
#line 26 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	return ADBLOCK_FILTER_GET_CLASS (self)->size (self);
#line 221 "filter.c"
}


static gpointer _g_object_ref0 (gpointer self) {
#line 31 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	return self ? g_object_ref (self) : NULL;
#line 228 "filter.c"
}


AdblockFilter* adblock_filter_construct (GType object_type, AdblockOptions* options) {
	AdblockFilter * self = NULL;
	AdblockOptions* _tmp0_;
	AdblockOptions* _tmp1_;
#line 30 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (options != NULL, NULL);
#line 30 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	self = (AdblockFilter*) adblock_feature_construct (object_type);
#line 31 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp0_ = options;
#line 31 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp1_ = _g_object_ref0 (_tmp0_);
#line 31 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_g_object_unref0 (self->priv->optslist);
#line 31 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	self->priv->optslist = _tmp1_;
#line 32 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	adblock_feature_clear ((AdblockFeature*) self);
#line 30 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	return self;
#line 252 "filter.c"
}


static void _g_free0_ (gpointer var) {
#line 36 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	var = (g_free (var), NULL);
#line 259 "filter.c"
}


static void _g_regex_unref0_ (gpointer var) {
#line 36 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	(var == NULL) ? NULL : (var = (g_regex_unref (var), NULL));
#line 266 "filter.c"
}


static void adblock_filter_real_clear (AdblockFeature* base) {
	AdblockFilter * self;
	GHashFunc _tmp0_;
	GEqualFunc _tmp1_;
	GHashTable* _tmp2_;
#line 35 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	self = (AdblockFilter*) base;
#line 36 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp0_ = g_str_hash;
#line 36 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp1_ = g_str_equal;
#line 36 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp2_ = g_hash_table_new_full (_tmp0_, _tmp1_, _g_free0_, _g_regex_unref0_);
#line 36 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_g_hash_table_unref0 (self->rules);
#line 36 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	self->rules = _tmp2_;
#line 287 "filter.c"
}


gboolean adblock_filter_check_rule (AdblockFilter* self, GRegex* regex, const gchar* pattern, const gchar* request_uri, const gchar* page_uri, GError** error) {
	gboolean result = FALSE;
	GRegex* _tmp0_;
	const gchar* _tmp1_;
	gboolean _tmp2_ = FALSE;
	gboolean _tmp3_;
	AdblockOptions* _tmp4_;
	const gchar* _tmp5_;
	gchar* _tmp6_ = NULL;
	gchar* opts;
	gboolean _tmp7_ = FALSE;
	const gchar* _tmp8_;
	gboolean _tmp11_;
	GRegex* _tmp19_;
	const gchar* _tmp20_ = NULL;
	const gchar* _tmp21_;
	GError * _inner_error_ = NULL;
#line 39 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (self != NULL, FALSE);
#line 39 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (regex != NULL, FALSE);
#line 39 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (pattern != NULL, FALSE);
#line 39 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (request_uri != NULL, FALSE);
#line 39 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_return_val_if_fail (page_uri != NULL, FALSE);
#line 40 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp0_ = regex;
#line 40 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp1_ = request_uri;
#line 40 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp2_ = g_regex_match_full (_tmp0_, _tmp1_, (gssize) (-1), 0, 0, NULL, &_inner_error_);
#line 40 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp3_ = _tmp2_;
#line 40 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	if (_inner_error_ != NULL) {
#line 40 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		g_propagate_error (error, _inner_error_);
#line 40 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		return FALSE;
#line 332 "filter.c"
	}
#line 40 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	if (!_tmp3_) {
#line 41 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		result = FALSE;
#line 41 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		return result;
#line 340 "filter.c"
	}
#line 43 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp4_ = self->priv->optslist;
#line 43 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp5_ = pattern;
#line 43 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp6_ = adblock_options_lookup (_tmp4_, _tmp5_);
#line 43 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	opts = _tmp6_;
#line 44 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp8_ = opts;
#line 44 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	if (_tmp8_ != NULL) {
#line 354 "filter.c"
		const gchar* _tmp9_;
		gboolean _tmp10_ = FALSE;
#line 44 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		_tmp9_ = opts;
#line 44 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		_tmp10_ = g_regex_match_simple (",third-party", _tmp9_, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY);
#line 44 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		_tmp7_ = _tmp10_;
#line 363 "filter.c"
	} else {
#line 44 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		_tmp7_ = FALSE;
#line 367 "filter.c"
	}
#line 44 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp11_ = _tmp7_;
#line 44 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	if (_tmp11_) {
#line 373 "filter.c"
		gboolean _tmp12_ = FALSE;
		const gchar* _tmp13_;
		gboolean _tmp18_;
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		_tmp13_ = page_uri;
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		if (_tmp13_ != NULL) {
#line 381 "filter.c"
			GRegex* _tmp14_;
			const gchar* _tmp15_;
			gboolean _tmp16_ = FALSE;
			gboolean _tmp17_;
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			_tmp14_ = regex;
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			_tmp15_ = page_uri;
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			_tmp16_ = g_regex_match_full (_tmp14_, _tmp15_, (gssize) (-1), 0, 0, NULL, &_inner_error_);
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			_tmp17_ = _tmp16_;
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			if (_inner_error_ != NULL) {
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
				g_propagate_error (error, _inner_error_);
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
				_g_free0 (opts);
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
				return FALSE;
#line 402 "filter.c"
			}
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			_tmp12_ = _tmp17_;
#line 406 "filter.c"
		} else {
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			_tmp12_ = FALSE;
#line 410 "filter.c"
		}
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		_tmp18_ = _tmp12_;
#line 46 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
		if (_tmp18_) {
#line 47 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			result = FALSE;
#line 47 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			_g_free0 (opts);
#line 47 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
			return result;
#line 422 "filter.c"
		}
	}
#line 48 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp19_ = regex;
#line 48 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp20_ = g_regex_get_pattern (_tmp19_);
#line 48 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_tmp21_ = request_uri;
#line 48 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	adblock_debug ("blocked by pattern regexp=%s -- %s", _tmp20_, _tmp21_, NULL);
#line 49 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	result = TRUE;
#line 49 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_g_free0 (opts);
#line 49 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	return result;
#line 439 "filter.c"
}


static void adblock_filter_class_init (AdblockFilterClass * klass) {
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	adblock_filter_parent_class = g_type_class_peek_parent (klass);
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	g_type_class_add_private (klass, sizeof (AdblockFilterPrivate));
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	ADBLOCK_FILTER_CLASS (klass)->insert = adblock_filter_real_insert;
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	ADBLOCK_FILTER_CLASS (klass)->lookup = adblock_filter_real_lookup;
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	ADBLOCK_FILTER_CLASS (klass)->size = adblock_filter_real_size;
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	ADBLOCK_FEATURE_CLASS (klass)->clear = adblock_filter_real_clear;
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	G_OBJECT_CLASS (klass)->finalize = adblock_filter_finalize;
#line 458 "filter.c"
}


static void adblock_filter_instance_init (AdblockFilter * self) {
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	self->priv = ADBLOCK_FILTER_GET_PRIVATE (self);
#line 465 "filter.c"
}


static void adblock_filter_finalize (GObject* obj) {
	AdblockFilter * self;
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, ADBLOCK_TYPE_FILTER, AdblockFilter);
#line 15 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_g_object_unref0 (self->priv->optslist);
#line 16 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	_g_hash_table_unref0 (self->rules);
#line 14 "/home/zrl/epiphany_webkit/Webkit2Browser/Source/midori/extensions/adblock/filter.vala"
	G_OBJECT_CLASS (adblock_filter_parent_class)->finalize (obj);
#line 479 "filter.c"
}


GType adblock_filter_get_type (void) {
	static volatile gsize adblock_filter_type_id__volatile = 0;
	if (g_once_init_enter (&adblock_filter_type_id__volatile)) {
		static const GTypeInfo g_define_type_info = { sizeof (AdblockFilterClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) adblock_filter_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (AdblockFilter), 0, (GInstanceInitFunc) adblock_filter_instance_init, NULL };
		GType adblock_filter_type_id;
		adblock_filter_type_id = g_type_register_static (ADBLOCK_TYPE_FEATURE, "AdblockFilter", &g_define_type_info, G_TYPE_FLAG_ABSTRACT);
		g_once_init_leave (&adblock_filter_type_id__volatile, adblock_filter_type_id);
	}
	return adblock_filter_type_id__volatile;
}


