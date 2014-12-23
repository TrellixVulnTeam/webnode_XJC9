/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "BrowserSettingsDialog.h"
//#include "BrowserCellRendererVariant.h"
#include "../midori/Certificate.h"

//lxx add +, 20141210
#if 1
#define FontNum 118
#define FontSizeNum 5
#define PageZoomNum 16
//lxx add fontNum = 118, fontSizeNum = 5, pageZoomNum = 16
const gchar* font[]  =    {"sans-serif", "AR PL UKai CN", "AR PL UKai HK", "AR PL UKai TW",
                            "AR PL UKai TW MBE", "AR PL UMing CN", "AR PL UMing HK", "AR PL UMing TW",
                            "AR PL UMing TW MBE", "Droid Sans", "文泉驿微米黑", "文泉驿正黑",
                            "文泉驿点阵正黑", "文泉驿等宽微米黑", "文泉驿等宽正黑", "方正书宋_GBK",
                            "方正仿宋_GBK", "方正姚体_GBK", "方正宋体S-超大字符集", "方正小标宋_GBK",
                            "方正楷体_GBK", "方正细黑——_GBK", "方正行楷_GBK", "方正超粗黑_GBK",
                            "方正隶书_GBK", "方正魏碑_GBK", "方正黑体_GBK", "serif",
                            "monospace", "Abyssinica SIL", "Bitstream Charter", "Century Schoolbook L",
                            "Courier 10 pitch", "DejaVu Sans", "DejaVu Sans Mono", "DejaVu Serif",
                            "Dingbats", "Droid Sans Mono", "Droid Serif", "FreeMono",
                            "FreeSans", "FreeSerif", "Garuda", "kacstArt",
                            "kacstBook", "kacstDecorative", "kacstDigital", "kacstFarsi",
                            "kacstLetter", "kacstNaskh", "kacstOffice", "kacstOne",
                            "kacstPen", "kacstPoster", "kacstQurn", "kacstScreen",
                            "kacstTitle", "kacstTitleL", "Kedage", "Khmer OS",
                            "Khmer OS System", "Kinnari", "LKLUG", "Liberation Mono",
                            "Liberation Sans", "Liberation Sans Narrow", "Liberation Serif",
                            "Lohit Bengali", "Lohit Gujarati", "Lohit Hindi", "Lohit Punjabi",
                            "Lohit Tamil", "MT Extra", "Mallige", "Meera",
                            "Mukti Narrow", "NanumGothic", "NanumMyeongjo", "Nimbus Mono L",
                            "Nimbus Roman No9 L", "Nimbus Sans L", "Norasi", "OpenSymbol",
                            "Padauk", "Padauk Book", "Phetsarath OT", "Pothana2000", 
                            "Purisa", "Rachana", "Rekha", "Saab", 
                            "Sawasdee", "Standard Symbols L", "Symbol", "TakaoPGothic",
                            "Tibetan Machine Uni", "Tlwg Typist", "Tlwg Typo", "TlwgMono",
                            "TlwgTypewriter", "URW Bookman L", "URW Chancery L", "URW Gothic L",
                            "URW Palladio L", "Ubuntu", "Ubuntu Condensed", "Ubuntu Mono",
                            "Umpush", "Vemana2000", "Waree", "Webdings",
                            "Wingdings", "Wingdings 2", "Wingdings 3", "gargi",
                            "mry_KacstQurn", "ori1Uni", "方正宋体S-超大字符集(SIP)"  
           
                          };


const  guint32 font_size[] = {9, 12, 16, 20, 24 
                           
                             };


const gdouble zoom_factor[] = {0.25, 0.33, 0.5, 0.67, 0.75, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0,
                               2.5, 3.0, 4.0, 5.0  
                                 
                              };
#endif   

static void onStartupCallback(GtkComboBox *widget, MidoriWebSettings *settings)
{
    int CurrentSelect = gtk_combo_box_get_active(widget); 
    g_object_set(settings,
             "load-on-startup", CurrentSelect,
             NULL);
}

static void openNewpageCallback(GtkComboBox *widget, MidoriWebSettings *settings)
{
    int CurrentSelect = gtk_combo_box_get_active(widget); 
    g_object_set(settings,
             "open-new-pages-in", CurrentSelect,
             NULL);
}

static void currentPageToHomePageCallback(GtkButton *button, MidoriWebSettings *settings) 
{
    gtk_entry_set_text(GTK_ENTRY(settings->entry1_general), settings->current_uri);
}

static void homePageCallback(GtkEntry *entry, MidoriWebSettings *settings) 
{
    const gchar * entry_content =  gtk_entry_get_text(GTK_ENTRY(entry/*settings->entry1_general*/));

    g_object_set(settings, "homepage", entry_content, NULL);
}

static void muchTabWarningCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "much_tab_warning", bvalue,
             NULL);
} 

static void showStatusBarCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "show-statusbar", bvalue,
             NULL);
} 

static void showBookmarkbarCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "show_bookmarkbar", bvalue,
             NULL);
} 

static void showMenubarCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "show_menubar", bvalue,
             NULL);
} 

static void defaultFontCallback(GtkComboBox *widget, MidoriWebSettings *settings)
{
    int CurrentSelect = gtk_combo_box_get_active(widget); 
    g_object_set(settings,
             "default-font-family", font[CurrentSelect],
             NULL);
}

gint getFontFamilyComboboxIndex(MidoriWebSettings *settings)
{
	int index = 0;
	gchar *strval;
	g_object_get(settings, "default-font-family", &strval, NULL);
	for(index = 0; index < FontNum; index++) 
	{
		if(!g_strcmp0(font[index], strval)) 
		{
			g_free(strval);
			return index;
	      }
	}
	g_free(strval);
	g_warning("undefined string: default-font-family  %s\n", strval);
	return -1;
}

static void defaultFontsizeCallback(GtkComboBox *widget, MidoriWebSettings *settings)
{ 
    int CurrentSelect = gtk_combo_box_get_active(widget); 
    g_object_set(settings,
             "default-font-size", font_size[CurrentSelect],
             NULL);
}

gint getFontSizeComboboxIndex(MidoriWebSettings *settings)
{
	int index = 0;
	int size;
	g_object_get(settings, "default-font-size", &size, NULL);
	for(index = 0; index < FontSizeNum; index++) 
	{
      if(font_size[index] == size) 
		{
			return index;   
		}
	}
	
	g_warning("undefined string: default-font-size\n");
	return -1;
}

gint getZoomLevelComboboxIndex(MidoriWebSettings *settings)
{
	int index = 0;
	gdouble dvalue;
	g_object_get(settings, "zoom-level", &dvalue, NULL);
	for(index = 0; index < PageZoomNum; index++) 
	{
      if(zoom_factor[index] == dvalue)
		{
			return index;   
		}
	}
	
	g_warning("undefined string: zoom-level %f\n", dvalue);
	return -1;
}

static void zoomTextOnlyCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 

    g_object_set(settings,
             "zoom-text-and-images", !bvalue,
             NULL);
}

static void pageZoomCallback(GtkComboBox *widget, MidoriWebSettings *settings)
{
    int CurrentSelect = gtk_combo_box_get_active(widget);
    g_object_set(settings,
             "zoom-level", zoom_factor[CurrentSelect],
             NULL);
}

static void showImageCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue;
    if(gtk_toggle_button_get_active(togglebutton)) {
      if((void *)settings->radiobutton1_content == (void *)togglebutton) {
        bvalue = TRUE;
      }
      else if((void *)settings->radiobutton2_content == (void *)togglebutton) {
        bvalue = FALSE;
      }
      else {
        printf("error showImageCallback\n"); 
        return;
      }
      g_object_set(settings,
               "auto_load_images", bvalue,
               NULL);
    }
} 

static void runJavascriptCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue;
    if(gtk_toggle_button_get_active(togglebutton)) {
      if((void *)settings->radiobutton3_content == (void *)togglebutton) {
        bvalue = TRUE;
      }
      else if((void *)settings->radiobutton4_content == (void *)togglebutton) {
        bvalue = FALSE;
      }
      else {
        printf("error runJavascriptCallback\n"); 
        return;
      }
      g_object_set(settings,
               "enable_scripts", bvalue,
               NULL);
    }
} 

static void pageContentCacheCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "page-content-cache", bvalue,
             NULL);
} 

static void historySettingCallback(GtkComboBox *widget, MidoriWebSettings *settings)
{
    int CurrentSelect = gtk_combo_box_get_active(widget); 
    g_object_set(settings,
             "history-setting", CurrentSelect,
             NULL);
}


static void clearBrowseRecordCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "clear-browse-record", bvalue,
             NULL);
} 


static void clearDownloadRecordCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "clear-download-record", bvalue,
             NULL);
} 


static void clearCookieAndOthersCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "clear-cookie-and-others", bvalue,
             NULL);
}

static void clearCachedImagesAndFilesCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "clear-cached-images-and-files", bvalue,
             NULL);
}

static void clearPasswordsCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "clear-passwords", bvalue,
             NULL);
}

static void cookieSettingCallback(GtkComboBox *widget, MidoriWebSettings *settings)
{
    int CurrentSelect = gtk_combo_box_get_active(widget); 
    g_object_set(settings,
             "cookie-setting", CurrentSelect,
             NULL);
}

static void trackLocationCallback(GtkComboBox *widget, MidoriWebSettings *settings)
{
    int CurrentSelect = gtk_combo_box_get_active(widget); 
    g_object_set(settings,
             "track-location", CurrentSelect,
             NULL);
}

//clear data callback 
static void clearDataCallback(GtkButton *button, MidoriWebSettings *settings) 
{
return;
    //clear browse record.
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings->checkbutton4_privacy))) {
    }

    //clear download record.
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings->checkbutton5_privacy))) {
    }

    //clear cookie and others
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings->checkbutton6_privacy))) {
        WebKitCookieManager* cookiemanager = webkit_web_context_get_cookie_manager(webkit_web_context_get_default());
        if(cookiemanager){
            webkit_cookie_manager_delete_all_cookies(cookiemanager);
        }
    }

    //clear cached images and files
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings->checkbutton7_privacy))) {
        webkit_web_context_clear_cache(webkit_web_context_get_default());
    }

    //clear passwords
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(settings->checkbutton8_privacy))) {
    }     
}

//luyue add
static void certificateManagerCallback(GtkToggleButton *button, MidoriWebSettings *settings)
{
   display_certificatemanager(); 
}

static void certificateRevocationCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(togglebutton); 
    g_object_set(settings,
             "certificate-revocation", bvalue,
             NULL);
}

static void mediaAccessCallback(GtkToggleButton *togglebutton, MidoriWebSettings *settings)
{
    int ivalue;
    if(gtk_toggle_button_get_active(togglebutton)) {
      if((void *)settings->radiobutton1_security == (void *)togglebutton) {
        ivalue = 0;
      }
      else if((void *)settings->radiobutton2_security == (void *)togglebutton) {
        ivalue = 1;
      }
      else {
        printf("error mediaAccessCallback\n"); 
        return;
      }
      g_object_set(settings,
               "media-access", ivalue,
               NULL);
    }
}

static void setCdosDefaultBrowserCallback(GtkButton *button, MidoriWebSettings *settings)
{
	system("xdg-settings set default-web-browser midori.desktop");//	system("xdg-settings set default-web-browser cuprumbrowser.desktop");
	printf("function setCuprumDefaultBrowserCallback has been called\n");
}

static void networkSettingCallback(GtkButton *button, MidoriWebSettings *settings)
{
	system("/usr/bin/cinnamon-settings network");
	printf("function networkSettingCallback has been called\n");
}

static void do_reset_browser()
{
	if(DeletePreferencesFile())
	{
		printf("PreferencesFile has been deleted\n");
		return;
	};
//删除历史记录的
//删除书签的
	printf("do reset browser func has been called\n");
}

static void resetNetworkSettingCallback(GtkButton *button, MidoriWebSettings *settings)
{
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                                                                    "该操作将重置您的主页、将您的常规设置、\n隐私和安全等设置恢复为原始默认设置，还\n将删除您的书签、历史记录以及临时数据和\n缓存数据（例如Cookie）。");
	gtk_window_set_title (GTK_WINDOW (dialog), "确实要重置浏览器吗？");
	gint result = gtk_dialog_run(GTK_DIALOG (dialog));
	if(result == GTK_RESPONSE_CANCEL)
	{
		gtk_widget_destroy(dialog);
	}
	else if(result == GTK_RESPONSE_OK)
	{
		do_reset_browser();
		gtk_widget_destroy(dialog);
	}
}

static void alterDownloadSaveCatalogCallback(GtkButton *button, MidoriWebSettings *settings)
{
	GtkWidget *dialog;  
	GtkWidget *entry;

    ///创建文件选择对话框
	dialog = gtk_file_chooser_dialog_new ("保存目录",
							NULL,/*gtk_widget_get_toplevel(GTK_WINDOW(button)),*/
							GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,//GTK_FILE_CHOOSER_ACTION_SAVE, 
							/*GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,*/
							GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
							NULL);

	entry = (GtkEntry*)settings->entry1_advanced;

    ///文件选择类型过滤

    //设置当前文件夹
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (dialog),"/home/lianxx/下载");

	gtk_window_set_position(GTK_WINDOW(dialog),GTK_WIN_POS_CENTER_ON_PARENT);

    ///等待用户的动作
	if(gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
        ///取得用户所选文件的路径
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

        ///把文件路径显示到输入条上
		gtk_entry_set_text(GTK_ENTRY(entry),filename);
	}

	gtk_widget_destroy(dialog);
}

static void downloadPathCallback(GtkEntry *entry, MidoriWebSettings *settings) 
{
    const gchar * entry_content =  gtk_entry_get_text(GTK_ENTRY(entry));

    g_object_set(settings, "download-folder", entry_content, NULL);
	 g_free (entry_content);
}

static void askEverytimeBeforeDownloadCallback(GtkToggleButton *button, MidoriWebSettings *settings)
{
    bool bvalue = gtk_toggle_button_get_active(button); 
    g_object_set(settings,
             "ask_every_time_before_download_file", bvalue,
             NULL);
}

GtkWidget * browser_settings_window_new(MidoriWebSettings *settings)
{

	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *table;
	GtkWidget *notebook;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *widget;
	GtkGrid *grid;
	GSList *group;
	int i;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL/*GTK_WINDOW_POPUP*/);
	gtk_window_set_resizable (window, FALSE);

	gtk_window_set_title (GTK_WINDOW (window), "设置");
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	table = gtk_table_new (3, 6, FALSE);
	gtk_container_add (GTK_CONTAINER (window), table);
/* 创建一个新的笔记本,将标签页放在顶部 */
	notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);

	gtk_table_attach_defaults (GTK_TABLE (table), notebook, 0, 6, 0, 1);
//	gtk_widget_show (notebook);

/* 在笔记本后面追加几个页面 */
//general
	frame = gtk_frame_new (NULL);
	grid = (GtkGrid*)gtk_grid_new();//创建网格

	label = gtk_label_new("启动：");
	gtk_grid_attach( grid, label, 0, 0, 1, 1);
	widget = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "显示空白页");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "显示我的主页");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "显示上次打开的窗口和标签页");
	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(onStartupCallback), settings);
	gint ivalue = katze_object_get_int (settings, "load-on-startup");
	switch(ivalue) {
	case 0://blank page
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
		break;
	case 1://homepage
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
		break;
	case 2://last opened pages
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 2);
		break;
	default:
		printf("error PROP_ON_STARTUP ivalue = %i\n", ivalue);
		break;
	}
	gtk_grid_attach( grid, widget, 1, 0, 2, 1);
	widget = gtk_label_new("主页：");
	gtk_grid_attach( grid, widget, 0, 2, 1, 1);
	widget = gtk_entry_new();
	gtk_entry_set_max_length ((GtkEntry*)widget, 300);
	settings->entry1_general = GTK_WIDGET(widget);

	gchar *strval = katze_object_get_string(settings, "homepage");
	gtk_entry_set_text(GTK_ENTRY(widget), strval);
	g_free (strval);

	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(homePageCallback), settings);
	gtk_grid_attach( grid, widget, 1, 2, 1, 1);
	button = gtk_button_new_with_label("使用当前页面");
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(currentPageToHomePageCallback), settings);
	gtk_grid_attach(grid, button, 2, 2, 1, 1);

	widget = gtk_label_new("标签：");
	gtk_grid_attach(grid, widget, 0, 3, 1, 1);

	widget = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "始终以新标签页打开网页");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "始终以新窗口打开网页");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "始终在当前标签页打开网页");
	gtk_grid_attach(grid, widget, 1, 3, 2, 1);
	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(openNewpageCallback), settings);
	ivalue = katze_object_get_int(settings, "open-new-pages-in");
	switch(ivalue) {
	case 0:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
		break;
	case 1:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
      break;
	case 2:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 2);
		break;
    default:
		printf("error PROP_OPEN_NEWPAGE ivalue = %i\n", ivalue);
		break;
	}

	button = gtk_check_button_new_with_label("打开多个标签可能致使浏览器速度缓慢时警告我");
	bool bvalue = katze_object_get_boolean(settings, "much_tab_warning");
	if(TRUE == bvalue)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
	g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(muchTabWarningCallback), settings);
	gtk_grid_attach(grid, button, 1, 4, 2, 1);

	widget = gtk_label_new("外观：");
	gtk_grid_attach(grid,widget,0,5,1,1);
	button = gtk_check_button_new_with_label("显示状态栏");
	g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(showStatusBarCallback), settings);
	bvalue = katze_object_get_boolean(settings, "show-statusbar");
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE); 
	gtk_grid_attach(grid,button,1,6,1,1);

	button = gtk_check_button_new_with_label("显示书签栏");
	g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(showBookmarkbarCallback), settings);
	bvalue = katze_object_get_boolean(settings, "show_bookmarkbar");
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
	gtk_grid_attach(grid,button,1,7,1,1);

	button = gtk_check_button_new_with_label("显示菜单栏");
	g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(showMenubarCallback), settings);
	bvalue = katze_object_get_boolean(settings, "show_menubar");
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
	gtk_grid_attach(grid,button,1,8,1,1);

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(grid));	

	label = gtk_label_new ("常规");
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

//content
	frame = gtk_frame_new (NULL);
	grid = (GtkGrid*)gtk_grid_new();//创建网格

	widget = gtk_label_new("字体：");
	gtk_grid_attach(grid,widget,0,1,1,1);
	widget = gtk_label_new("默认字体：");
	gtk_grid_attach(grid,widget,1,2,1,1);

	widget = gtk_combo_box_text_new();
	for(i = 0; i < FontNum; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), font[i]);
	}
	gint index = getFontFamilyComboboxIndex(settings);
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), index);
   g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(defaultFontCallback), settings);
	gtk_grid_attach(grid,widget,2,2,1,1);

	widget = gtk_label_new("字号：");
	gtk_grid_attach(grid,widget,1,3,1,1);

	widget = gtk_combo_box_text_new();
	for(i = 0; i < FontSizeNum; i++)
	{
		char ic[10] = {0};
		sprintf(ic, "%d", font_size[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), ic);
	}
	index = getFontSizeComboboxIndex(settings);
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), index);
   g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(defaultFontsizeCallback), settings);
	gtk_grid_attach(grid,widget,2,3,1,1);

	widget = gtk_label_new("网页缩放：");
	gtk_grid_attach(grid,widget,0,4,1,1);

	widget = gtk_label_new("缩放比例：");
	gtk_grid_attach(grid,widget,1,5,1,1);

	widget = gtk_combo_box_text_new();
	for(i = 0; i < PageZoomNum; i++)
	{
		char ic[10] = {0};
		sprintf(ic, "%2.0f%%", zoom_factor[i] * 100);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), ic);
	}
	index = getZoomLevelComboboxIndex(settings);
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), index);
   g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(pageZoomCallback), settings);
	gtk_grid_attach(grid,widget,2,5,1,1);

	button = gtk_check_button_new_with_label("仅对文本缩放");
   bvalue = 1;
   g_object_get(settings, "zoom-text-and-images", &bvalue, NULL);
	if(!bvalue)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(zoomTextOnlyCallback), settings);
	gtk_grid_attach(grid,button,1,6,1,1);

	widget = gtk_label_new("图片：");
	gtk_grid_attach(grid,widget,0,7,1,1);

	button = gtk_radio_button_new_with_label (NULL, "显示所有图片（推荐）");
	settings->radiobutton1_content = GTK_WIDGET(button);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(showImageCallback), settings);
	gtk_grid_attach(grid,button,1,8,2,1);
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	button = gtk_radio_button_new_with_label (group, "不显示任何图片");
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(showImageCallback), settings);
	settings->radiobutton2_content = GTK_WIDGET(button);
	g_object_get(settings, "auto_load_images", &bvalue, NULL);   
   if(TRUE == bvalue) 
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(settings->radiobutton1_content), TRUE);     
	else
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(settings->radiobutton2_content), TRUE); 
	gtk_grid_attach(grid,button,1,9,2,1);

	widget = gtk_label_new("JavaScript：");
	gtk_grid_attach(grid,widget,0,10,1,1);
	button = gtk_radio_button_new_with_label (NULL, "允许所有网站运行JavaScript（推荐）");
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(runJavascriptCallback), settings);
	settings->radiobutton3_content = GTK_WIDGET(button);
	gtk_grid_attach(grid,button,1,11,2,1);
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	button = gtk_radio_button_new_with_label (group, "不允许任何网站运行JavaScript");
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(runJavascriptCallback), settings);
	settings->radiobutton4_content = GTK_WIDGET(button);
	g_object_get(settings, "enable_scripts", &bvalue, NULL);   
   if(TRUE == bvalue) 
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(settings->radiobutton3_content), TRUE);     
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(settings->radiobutton4_content), TRUE); 
	gtk_grid_attach(grid,button,1,12,2,1);

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(grid));

	label = gtk_label_new ("内容");
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

//privacy
	frame = gtk_frame_new (NULL);
	grid = (GtkGrid*)gtk_grid_new();//创建网格

	widget = gtk_label_new("密码：");
	gtk_grid_attach(grid,widget,0,0,1,1);

	button = gtk_check_button_new_with_label("记住网站密码");
	gtk_grid_attach(grid,button,1,1,1,1);

	button = gtk_check_button_new_with_label("使用主密码");
	gtk_grid_attach(grid,button,1,2,1,1);

	button = gtk_button_new_with_label("　修改主密码　");
	gtk_grid_attach(grid,button,2,1,1,1);

	button = gtk_button_new_with_label("　已保存密码　");
	gtk_grid_attach(grid,button,2,2,1,1);

	widget = gtk_label_new("缓存：");
	gtk_grid_attach(grid,widget,0,3,1,1);

	button = gtk_check_button_new_with_label("开启网络内容缓存");
	g_object_get(settings, "page-content-cache", &bvalue, NULL);
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE); 
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(pageContentCacheCallback), settings);
	gtk_grid_attach(grid,button,1,4,1,1);

	widget = gtk_label_new("清除数据：");
	gtk_grid_attach(grid,widget,0,5,1,1);

	button = gtk_check_button_new_with_label("浏览记录");
	settings->checkbutton4_privacy = GTK_WIDGET(button);
	g_object_get(settings, "clear-browse-record", &bvalue, NULL);
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE); 
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(clearBrowseRecordCallback), settings);
	gtk_grid_attach(grid,button,1,6,1,1);

	button = gtk_check_button_new_with_label("下载记录");
	settings->checkbutton5_privacy = GTK_WIDGET(button);
	g_object_get(settings, "clear-download-record", &bvalue, NULL);
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE); 
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(clearDownloadRecordCallback), settings);
	gtk_grid_attach(grid, button, 2, 6, 1, 1);

	button = gtk_check_button_new_with_label("缓存的图片和文件");
	settings->checkbutton7_privacy = GTK_WIDGET(button);
	g_object_get(settings, "clear-cached-images-and-files", &bvalue, NULL);
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE); 
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(clearCachedImagesAndFilesCallback), settings);
	gtk_grid_attach(grid, button, 1, 7, 1, 1);

	button = gtk_check_button_new_with_label("密码");
	settings->checkbutton8_privacy = GTK_WIDGET(button);
	g_object_get(settings, "clear-passwords", &bvalue, NULL);
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE); 
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(clearPasswordsCallback), settings);
	gtk_grid_attach(grid, button, 2, 7, 1, 1);

	button = gtk_check_button_new_with_label("Cookie及其它网站和插件数据");
	settings->checkbutton6_privacy = GTK_WIDGET(button);
	g_object_get(settings, "clear-cookie-and-others", &bvalue, NULL);
	if(TRUE == bvalue)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE); 
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(clearCookieAndOthersCallback), settings);
	gtk_grid_attach(grid, button, 1, 8, 2, 1);

	button = gtk_button_new_with_label("　　清除　　");
   g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(clearDataCallback), settings);
	gtk_grid_attach(grid, button, 3, 9, 1, 1);


	widget = gtk_label_new("历史记录：");
	gtk_grid_attach(grid, widget, 0, 10, 1, 1);

	widget = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "记录历史");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "不记录历史");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "使用自定义历史记录设置");
	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(historySettingCallback), settings);
	ivalue = katze_object_get_int(settings, "history-setting");
	switch(ivalue) {
	case 0:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
		break;
	case 1:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
      break;
	case 2:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 2);
		break;
    default:
		printf("error PROP_OPEN_NEWPAGE ivalue = %i\n", ivalue);
		break;
	}
	gtk_grid_attach(grid, widget, 1, 11, 2, 1);


	widget = gtk_label_new("Cookie：");
	gtk_grid_attach(grid, widget, 0, 12, 1, 1);

	widget = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "允许设置本地数据（推荐）");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "仅将本地数据保留到您退出浏览器为止");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "阻止网站设置任何数据");
	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(cookieSettingCallback), settings);
	ivalue = katze_object_get_int(settings, "cookie-setting");
	switch(ivalue) {
	case 0:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
		break;
	case 1:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
      break;
	case 2:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 2);
		break;
    default:
		printf("error PROP_OPEN_NEWPAGE ivalue = %i\n", ivalue);
		break;
	}
	gtk_grid_attach(grid, widget, 1, 13, 2, 1);

	widget = gtk_label_new("位置：");
	gtk_grid_attach(grid, widget, 0, 14, 1, 1);

	widget = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "允许所有网站跟踪您所在的位置");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "当网站要跟踪您所在的位置时询问您（推荐）");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "不允许任何网站跟踪您所在的位置");
	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(trackLocationCallback), settings);
	ivalue = katze_object_get_int(settings, "track-location");
	switch(ivalue) {
	case 0:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
		break;
	case 1:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
      break;
	case 2:
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 2);
		break;
    default:
		printf("error PROP_OPEN_NEWPAGE ivalue = %i\n", ivalue);
		break;
	}
	gtk_grid_attach(grid, widget, 1, 15, 2, 1);

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(grid));

	label = gtk_label_new ("隐私");
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

//security
	frame = gtk_frame_new (NULL);
	grid = (GtkGrid*)gtk_grid_new();//创建网格

	widget = gtk_label_new("HTTPS/SSL:");
	gtk_grid_attach(grid, widget, 0, 0, 1, 1);

	button = gtk_button_new_with_label("管理证书");
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(certificateManagerCallback), settings);
	gtk_grid_attach(grid, button, 1, 1, 1, 1);

	button = gtk_check_button_new_with_label("检查服务器证书吊销状态");
	g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(certificateRevocationCallback), settings);
	gtk_grid_attach(grid, button, 1, 2, 2, 1);

	widget = gtk_label_new("媒体：");
	gtk_grid_attach(grid, widget, 0, 3, 1, 1);

	widget = gtk_label_new("麦克风：");
	gtk_grid_attach(grid, widget,1, 4, 1, 1);

	widget = gtk_combo_box_text_new();
	gtk_grid_attach(grid, widget, 2, 4, 1, 1);

	widget = gtk_label_new("相机：");
	gtk_grid_attach(grid, widget, 1, 5, 1, 1);

	widget = gtk_combo_box_text_new();
	gtk_grid_attach(grid, widget, 2, 5, 1, 1);

	button = gtk_radio_button_new_with_label (NULL, "当网站要求使用您的摄像头和麦克风时询问您（推荐）");
   settings->radiobutton1_security = GTK_WIDGET(button);
	g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(mediaAccessCallback), settings);
	gtk_grid_attach(grid, button, 1, 6, 5, 1);
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	button = gtk_radio_button_new_with_label (group, "不允许网络使用您的摄像头和麦克风");
	settings->radiobutton2_security = GTK_WIDGET(button);
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(mediaAccessCallback), settings);
	g_object_get(settings, "media-access", &ivalue, NULL);   
   switch(ivalue) 
	{
   case 0:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(settings->radiobutton1_security), TRUE);
		break;
	case 1:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(settings->radiobutton2_security), TRUE);
		break;
	default:
		printf("error PROP_MEDIA_ACCESS ivalue = %i\n", ivalue);
		break;
    } 
	gtk_grid_attach(grid, button, 1, 7, 5, 1);

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(grid));

	label = gtk_label_new ("安全");
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

//advanced
	frame = gtk_frame_new (NULL);
	grid = (GtkGrid*)gtk_grid_new();//创建网格

	widget = gtk_label_new("默认浏览器：");
	gtk_grid_attach(grid, widget, 0, 0, 1, 1);

	button = gtk_button_new_with_label("将cdosbrowser设置为默认浏览器");
   g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(setCdosDefaultBrowserCallback), settings);
	gtk_grid_attach(grid, button, 1, 1, 2, 1);

	widget = gtk_label_new("网络：");
	gtk_grid_attach(grid, widget, 0, 2, 1, 1);

	button = gtk_button_new_with_label("网络设置");
   g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(networkSettingCallback), settings);
	gtk_grid_attach(grid, button, 1, 3, 1, 1);

	widget = gtk_label_new("重置浏览器：");
	gtk_grid_attach(grid, widget, 0, 4, 1, 1);

	widget = gtk_label_new("将浏览器设置恢复为默认设置");
	gtk_grid_attach(grid, widget, 1, 5, 2, 1);

	button = gtk_button_new_with_label("重置浏览器设置");
   g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(resetNetworkSettingCallback), settings);
	gtk_grid_attach(grid, button, 1, 6, 2, 1);

	widget = gtk_label_new("下载：");
	gtk_grid_attach(grid, widget, 0, 7, 1, 1);

	widget = gtk_label_new("下载内容保存位置：");
	gtk_grid_attach(grid, widget, 1, 8, 1, 1);

	widget = gtk_entry_new();
   settings->entry1_advanced = GTK_WIDGET(widget);
	gtk_entry_set_icon_from_icon_name(GTK_ENTRY(widget), GTK_ENTRY_ICON_PRIMARY, "folder");
	gchar *path = katze_object_get_string(settings, "download-folder");
   gtk_entry_set_text(GTK_ENTRY(widget), path);
   g_free (path);
	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(downloadPathCallback), settings);
	gtk_grid_attach(grid, widget, 2, 8, 2, 1);

	button = gtk_button_new_with_label("更改...");
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(alterDownloadSaveCatalogCallback), settings);
	gtk_grid_attach(grid, button, 4, 8, 1, 1);

	button = gtk_check_button_new_with_label("下载前询问每个文件的保存位置");
	bvalue = katze_object_get_boolean(settings, "ask_every_time_before_download_file");
	if(TRUE == bvalue) 
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);     
	else 
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE); 
   g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(askEverytimeBeforeDownloadCallback), settings);
	gtk_grid_attach(grid, button, 1, 9, 3, 1);

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(grid));

	label = gtk_label_new ("高级");
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

	gtk_widget_show_all(window);

	return window;
}

/*
void InitMidoriSettingsWithFile(MidoriWebSettings* settings)
{ 
    base::FilePath config_file;
    base::FilePath home = base::GetHomeDir();
    if (!home.empty()) {
      config_file = home.Append(".config/preferences");
    } else {
      std::string tmp_file = "/tmp/preferences";
      base::FilePath tmp(tmp_file);
      config_file = tmp;
    }
    settings->priv->user_pref_store_.reset(new JsonPrefStore(config_file));
    //if config_file exist, we also need to check the value we have read from the config_file. sunhaiming add.
    if (settings->priv->user_pref_store_->DoReading() && CheckReadValue(settings)) {
      ReSetProperty(settings);        
    }
    else {
      settings->priv->user_pref_store_->ResetJsonValue();  //clear DictionaryValue. sunhaiming add. 
      SaveInitValueToFile(settings);
    }
}
*/

