/*	viewer.c
	Copyright (C) 2004-2006 Mark Tyler and Dmitry Groshev

	This file is part of mtPaint.

	mtPaint is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	mtPaint is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with mtPaint in the file COPYING.
*/

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>

#if GTK_MAJOR_VERSION == 1
	#include <gdk/gdkx.h>
	#include <gdk_imlib.h>
#endif

#include "global.h"

#include "memory.h"
#include "mainwindow.h"
#include "viewer.h"
#include "canvas.h"
#include "mygtk.h"
#include "inifile.h"
#include "layer.h"
#include "toolbar.h"


gboolean
	view_showing = FALSE,
	allow_cline = FALSE, view_update_pending = FALSE;
float vw_zoom = 1;


////	COMMAND LINE WINDOW


GtkWidget *cline_window = NULL;


static gint viewer_keypress( GtkWidget *widget, GdkEventKey *event )
{							// Used by command line window
	if ( check_zoom_keys(event) ) return TRUE;		// Check HOME/zoom keys

	return FALSE;
}

gint delete_cline( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	int x, y, width, height;

	gdk_window_get_size( cline_window->window, &width, &height );
	gdk_window_get_root_origin( cline_window->window, &x, &y );
	
	inifile_set_gint32("cline_x", x );
	inifile_set_gint32("cline_y", y );
	inifile_set_gint32("cline_w", width );
	inifile_set_gint32("cline_h", height );

	gtk_widget_destroy(cline_window);
	men_item_state(menu_cline, TRUE);
	cline_window = NULL;
	allow_cline = TRUE;

	return FALSE;
}

void cline_select(GtkWidget *clist, gint row, gint col, GdkEvent *event, gpointer *pointer)
{
	int i = row + file_arg_start, change = 0;

	if ( strcmp( global_argv[i], mem_filename ) != 0 )
	{
		if ( layers_total==0 )
			change = check_for_changes();
		else
			change = check_layers_for_changes();
		if ( change == 2 || change == -10 )
			do_a_load(global_argv[i]);				// Load requested file
	}
}

void pressed_cline( GtkMenuItem *menu_item, gpointer user_data )
{
	int i;
	GtkWidget *vbox1, *button_close, *scrolledwindow, *col_list;
	GtkAccelGroup* ag = gtk_accel_group_new();
	gchar *item[1];
	char txt[64], txt2[600];

	men_item_state(menu_cline, FALSE);
	allow_cline = FALSE;

	snprintf(txt, 60, _("%i Files on Command Line"), files_passed );
	cline_window = add_a_window( GTK_WINDOW_TOPLEVEL, txt, GTK_WIN_POS_NONE, FALSE );
	gtk_widget_set_usize(cline_window, 100, 100);
	gtk_window_set_default_size( GTK_WINDOW(cline_window),
		inifile_get_gint32("cline_w", 250 ), inifile_get_gint32("cline_h", 400 ) );
	gtk_widget_set_uposition( cline_window,
		inifile_get_gint32("cline_x", 0 ), inifile_get_gint32("cline_y", 0 ) );

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (cline_window), vbox1);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show(scrolledwindow);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolledwindow),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	col_list = gtk_clist_new(1);
	gtk_clist_set_column_auto_resize( GTK_CLIST(col_list), 0, TRUE );
	gtk_clist_set_selection_mode( GTK_CLIST(col_list), GTK_SELECTION_SINGLE );

	item[0] = txt2;
	for ( i=file_arg_start; i<(file_arg_start + files_passed); i++ )
	{
#if GTK_MAJOR_VERSION == 2
		cleanse_txt( txt2, global_argv[i] );			// Clean up non ASCII chars
#else
		strcpy( txt2, global_argv[i] );
#endif
		gtk_clist_set_selectable ( GTK_CLIST(col_list),
			gtk_clist_append(GTK_CLIST (col_list), item), TRUE );
	}
	gtk_container_add ( GTK_CONTAINER(scrolledwindow), col_list );
	gtk_widget_show(col_list);
	gtk_signal_connect(GTK_OBJECT(col_list), "select_row", GTK_SIGNAL_FUNC(cline_select), NULL);

	button_close = add_a_button(_("Close"), 5, vbox1, FALSE);
	gtk_signal_connect_object( GTK_OBJECT(button_close), "clicked",
			GTK_SIGNAL_FUNC(delete_cline), GTK_OBJECT(cline_window));
	gtk_widget_add_accelerator (button_close, "clicked", ag, GDK_Escape, 0, (GtkAccelFlags) 0);

	gtk_signal_connect_object (GTK_OBJECT (cline_window), "delete_event",
		GTK_SIGNAL_FUNC (delete_cline), NULL);
	gtk_signal_connect_object (GTK_OBJECT (cline_window), "key_press_event",
		GTK_SIGNAL_FUNC (viewer_keypress), GTK_OBJECT (cline_window));

	gtk_widget_show(cline_window);
	gtk_window_add_accel_group(GTK_WINDOW (cline_window), ag);
	gtk_window_set_transient_for( GTK_WINDOW(cline_window), GTK_WINDOW(main_window) );
}



///	HELP WINDOW

GtkWidget *help_window;

gint click_help_end( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	men_item_state( menu_help, TRUE );	// Make sure the user can only open 1 help window
	gtk_widget_destroy( help_window );

	return FALSE;
}

void pressed_help( GtkMenuItem *menu_item, gpointer user_data )
{
#include "help.c"

	GtkAccelGroup* ag;
	GtkWidget *table,*notebook, *frame, *label, *button, *box1, *box2,
		*scrolled_window1, *viewport1, *label2;
#if GTK_MAJOR_VERSION == 2
	GtkStyle *style;
#endif

	int i;
	char txt[64];

	ag = gtk_accel_group_new();

	men_item_state( menu_help, FALSE );	// Make sure the user can only open 1 help help_window

	help_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_container_set_border_width (GTK_CONTAINER (help_window), 4);
	gtk_window_set_position (GTK_WINDOW (help_window), GTK_WIN_POS_CENTER);
	snprintf(txt, 60, "%s - %s", VERSION, _("About"));
	gtk_window_set_title (GTK_WINDOW (help_window), txt);
	gtk_widget_set_usize (help_window, -2, 400);

	box1 = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (help_window), box1);
	gtk_widget_show (box1);

	box2 = gtk_vbox_new (FALSE, 10);
	gtk_container_set_border_width (GTK_CONTAINER (box2), 1);
	gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
	gtk_widget_show (box2);

	table = gtk_table_new (3, 6, FALSE);
	gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);

	notebook = gtk_notebook_new ();
	gtk_table_attach_defaults (GTK_TABLE (table), notebook, 0, 6, 0, 1);
	gtk_widget_show (notebook);

	for (i=0; i<help_page_count; i++)
	{
		frame = gtk_frame_new (NULL);
		gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
		gtk_widget_show (frame);

		scrolled_window1 = gtk_scrolled_window_new (NULL, NULL);
		gtk_widget_show (scrolled_window1);
		gtk_container_add (GTK_CONTAINER (frame), scrolled_window1);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window1), 
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

		viewport1 = gtk_viewport_new (NULL, NULL);
		gtk_widget_show (viewport1);
		gtk_container_add (GTK_CONTAINER (scrolled_window1), viewport1);

		label2 = gtk_label_new (help_page_contents[i]);
#if GTK_MAJOR_VERSION == 2
		if ( i == 1 || i == 2 )	// Keyboard/Mouse shortcuts tab only
		{
			style = gtk_style_copy (gtk_widget_get_style (label2));
			style->font_desc = pango_font_description_from_string("Monospace 9");
						// Courier also works
			gtk_widget_set_style (label2, style);
		}
#endif
		gtk_widget_set_usize( label2, 380, -2 );		// Set minimum width/height
		gtk_widget_show (label2);

		gtk_container_add (GTK_CONTAINER (viewport1), label2);
		GTK_WIDGET_SET_FLAGS (label2, GTK_CAN_FOCUS);
#if GTK_MAJOR_VERSION == 2
		gtk_label_set_selectable(GTK_LABEL (label2), TRUE);
#endif
		gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);
		gtk_label_set_line_wrap (GTK_LABEL (label2), TRUE);
		gtk_misc_set_alignment (GTK_MISC (label2), 0, 0);
		gtk_misc_set_padding (GTK_MISC (label2), 5, 5);

		label = gtk_label_new (help_page_titles[i]);
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);
	}

	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_LEFT);

	box2 = gtk_vbox_new (FALSE, 10);
	gtk_container_set_border_width (GTK_CONTAINER (box2), 1);
	gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
	gtk_widget_show (box2);

	button = gtk_button_new_with_label (_("Close"));
	gtk_widget_add_accelerator (button, "clicked", ag, GDK_KP_Enter, 0, (GtkAccelFlags) 0);
	gtk_widget_add_accelerator (button, "clicked", ag, GDK_Return, 0, (GtkAccelFlags) 0);
	gtk_widget_add_accelerator (button, "clicked", ag, GDK_Escape, 0, (GtkAccelFlags) 0);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

	gtk_signal_connect_object(GTK_OBJECT(button), "clicked", (GtkSignalFunc) click_help_end,
		GTK_OBJECT(help_window));
	gtk_signal_connect_object (GTK_OBJECT (help_window), "delete_event",
		GTK_SIGNAL_FUNC (click_help_end), NULL);

	gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
	gtk_widget_show (button);

	gtk_widget_show (table);
	gtk_window_set_default_size( GTK_WINDOW(help_window), 600, 2 );
	gtk_widget_show (help_window);
	gtk_window_add_accel_group(GTK_WINDOW (help_window), ag);
}



///	PAN WINDOW


GtkWidget *pan_window, *draw_pan=NULL;
int pan_w, pan_h;
unsigned char *pan_rgb;

void draw_pan_thumb(int x1, int y1, int x2, int y2)
{
	int i, j, k, ix, iy;
	unsigned char pix, *wrk_image = mem_img[CHN_IMAGE];

	if ( pan_rgb == NULL ) return;		// Needed to stop segfault

	for ( j=0; j<pan_h; j++ )		// Create thumbnail
	{
		iy = ((float) j)/pan_h * mem_height;
		for ( i=0; i<pan_w; i++ )
		{
			ix = ((float) i)/pan_w * mem_width;
			if (mem_img_bpp == 3)
			{
			   for ( k=0; k<3; k++ )
				pan_rgb[ k + 3*(i + j*pan_w) ] = wrk_image[ k + 3*(ix + iy*mem_width) ];
			}
			else
			{
				pix = wrk_image[ ix + iy*mem_width ];
				pan_rgb[ 0 + 3*(i + j*pan_w) ] = mem_pal[pix].red;
				pan_rgb[ 1 + 3*(i + j*pan_w) ] = mem_pal[pix].green;
				pan_rgb[ 2 + 3*(i + j*pan_w) ] = mem_pal[pix].blue;
			}
		}
	}

	x2 = (x1 + x2)/can_zoom;
	y2 = (y1 + y2)/can_zoom;
	x1 = x1 / can_zoom;
	y1 = y1 / can_zoom;

	mtMAX(x1, x1, 0)
	mtMAX(y1, y1, 0)
	mtMIN(x1, x1, mem_width-1)
	mtMIN(y1, y1, mem_height-1)
	mtMAX(x2, x2, 0)
	mtMAX(y2, y2, 0)
	mtMIN(x2, x2, mem_width-1)
	mtMIN(y2, y2, mem_height-1)

	// Convert real image coords to thumbnail coords

	x1 = (((float) x1)/mem_width) * pan_w;
	y1 = (((float) y1)/mem_height) * pan_h;
	x2 = (((float) x2)/mem_width) * pan_w;
	y2 = (((float) y2)/mem_height) * pan_h;

	j=0;
	for ( i=x1; i<=x2; i++ )
	{
		for ( k=0; k<3; k++ )
		{
			pan_rgb[ k + 3*(i + y1*pan_w) ] = 255 * ( (j/4) % 2 );
			pan_rgb[ k + 3*(i + y2*pan_w) ] = 255 * ( (j/4) % 2 );
		}
		j++;
	}
	j=0;
	for ( i=y1; i<=y2; i++ )
	{
		for ( k=0; k<3; k++ )
		{
			pan_rgb[ k + 3*(x1 + i*pan_w) ] = 255 * ( (j/4) % 2 );
			pan_rgb[ k + 3*(x2 + i*pan_w) ] = 255 * ( (j/4) % 2 );
		}
		j++;
	}

	if ( draw_pan != NULL )
		gdk_draw_rgb_image( draw_pan->window, draw_pan->style->black_gc,
				0, 0, pan_w, pan_h, GDK_RGB_DITHER_NONE,
				pan_rgb, pan_w*3 );
}

void pan_thumbnail()		// Create thumbnail and selection box
{
	GtkAdjustment *hori, *vert;

	while (gtk_events_pending()) gtk_main_iteration();
		// Update main window first to get new scroll positions if necessary

	hori = gtk_scrolled_window_get_hadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );
	vert = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );

	draw_pan_thumb(hori->value, vert->value, hori->page_size, vert->page_size);
}

gint delete_pan( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	if ( pan_rgb != NULL ) free(pan_rgb);
	pan_rgb = NULL;				// Needed to stop segfault
	gtk_widget_destroy(pan_window);

	return FALSE;
}

gint key_pan( GtkWidget *widget, GdkEventKey *event )
{
	int nv_h, nv_v, arrow_key = 0;
	GtkAdjustment *hori, *vert;

	if ( !check_zoom_keys_real(event) )
	{
		switch (event->keyval)
		{
			case GDK_KP_Left:
			case GDK_Left:		arrow_key = 1; break;
			case GDK_KP_Right:
			case GDK_Right:		arrow_key = 2; break;
			case GDK_KP_Up:
			case GDK_Up:		arrow_key = 3; break;
			case GDK_KP_Down:
			case GDK_Down:		arrow_key = 4; break;
		}

		if ( arrow_key == 0 && event->keyval!=65505 && event->keyval!=65507 )
		{		// Xine sends 6550x key values so don't delete on this
			delete_pan(NULL, NULL, NULL);
		}
		else
		{
			hori = gtk_scrolled_window_get_hadjustment(
				GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );
			vert = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );

			nv_h = hori->value;
			nv_v = vert->value;

			if ( arrow_key == 1 ) nv_h -= hori->page_size/4;
			if ( arrow_key == 2 ) nv_h += hori->page_size/4;
			if ( arrow_key == 3 ) nv_v -= vert->page_size/4;
			if ( arrow_key == 4 ) nv_v += vert->page_size/4;

			if ( (nv_h + hori->page_size) > hori->upper ) nv_h = hori->upper - hori->page_size;
			if ( (nv_v + vert->page_size) > vert->upper ) nv_v = vert->upper - vert->page_size;
			mtMAX(nv_h, nv_h, 0)
			mtMAX(nv_v, nv_v, 0)
			hori->value = nv_h;
			vert->value = nv_v;
			gtk_adjustment_value_changed( hori );
			gtk_adjustment_value_changed( vert );

			pan_thumbnail();	// Update selection box
		}
	}
	else pan_thumbnail();	// Update selection box as user may have zoomed in/out

	return TRUE;
}

void pan_button(int mx, int my, int button)
{
	int nv_h, nv_v;
	float cent_x, cent_y;
	GtkAdjustment *hori, *vert;

	if ( button == 1 )	// Left click = pan window
	{
		hori = gtk_scrolled_window_get_hadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );
		vert = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );

		cent_x = ((float) mx) / pan_w;
		cent_y = ((float) my) / pan_h;

		nv_h = mem_width*can_zoom*cent_x - hori->page_size/2;
		nv_v = mem_height*can_zoom*cent_y - vert->page_size/2;

		if ( (nv_h + hori->page_size) > hori->upper ) nv_h = hori->upper - hori->page_size;
		if ( (nv_v + vert->page_size) > vert->upper ) nv_v = vert->upper - vert->page_size;
		mtMAX(nv_h, nv_h, 0)
		mtMAX(nv_v, nv_v, 0)

		hori->value = nv_h;
		vert->value = nv_v;
		gtk_adjustment_value_changed( hori );
		gtk_adjustment_value_changed( vert );

		draw_pan_thumb(nv_h, nv_v, hori->page_size, vert->page_size);
	}
	if ( button == 3 )	// Right click = kill window
		delete_pan(NULL, NULL, NULL);
}

static gint click_pan( GtkWidget *widget, GdkEventButton *event )
{
	pan_button(event->x, event->y, event->button);

	return FALSE;
}

gint pan_motion( GtkWidget *widget, GdkEventMotion *event )
{
	int x, y, button = 0;
	GdkModifierType state;

	if (event->is_hint) gdk_window_get_pointer (event->window, &x, &y, &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

	if (state & GDK_BUTTON1_MASK) button = 1;
	if (state & GDK_BUTTON3_MASK) button = 3;

	pan_button(x, y, button);

	return TRUE;
}

static gint expose_pan( GtkWidget *widget, GdkEventExpose *event )
{
	gdk_draw_rgb_image( widget->window, widget->style->black_gc,
				event->area.x, event->area.y, event->area.width, event->area.height,
				GDK_RGB_DITHER_NONE,
				pan_rgb + 3*( event->area.x + pan_w*event->area.y ),
				pan_w*3
				);
	return FALSE;
}

void pressed_pan( GtkMenuItem *menu_item, gpointer user_data )
{
	int max_pan = inifile_get_gint32("panSize", 128 );
	float rat_x, rat_y;

	draw_pan = NULL;	// Needed by draw_pan_thumb above

	rat_x = max_pan / ((float) mem_width);
	rat_y = max_pan / ((float) mem_height);

	if ( rat_x > rat_y )
	{
		pan_w = rat_y * mem_width;
		pan_h = max_pan;
	}
	else
	{
		pan_w = max_pan;
		pan_h = rat_x * mem_height;
	}
	mtMAX(pan_w, pan_w, 1)
	mtMAX(pan_h, pan_h, 1)

	pan_rgb = grab_memory( 3*pan_w*pan_h, 0 );

	pan_thumbnail();

	pan_window = add_a_window( GTK_WINDOW_POPUP, _("Pan Window"), GTK_WIN_POS_MOUSE, TRUE );
	gtk_container_set_border_width (GTK_CONTAINER (pan_window), 2);

	draw_pan = gtk_drawing_area_new();
	gtk_widget_set_usize( draw_pan, pan_w, pan_h );
	gtk_container_add (GTK_CONTAINER (pan_window), draw_pan);
	gtk_widget_show( draw_pan );
	gtk_signal_connect_object( GTK_OBJECT(draw_pan), "expose_event",
		GTK_SIGNAL_FUNC (expose_pan), GTK_OBJECT(draw_pan) );
	gtk_signal_connect_object( GTK_OBJECT(draw_pan), "button_press_event",
		GTK_SIGNAL_FUNC (click_pan), GTK_OBJECT(draw_pan) );
	gtk_signal_connect_object( GTK_OBJECT(draw_pan), "motion_notify_event",
		GTK_SIGNAL_FUNC (pan_motion), GTK_OBJECT(draw_pan) );
	gtk_widget_set_events (draw_pan, GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect_object (GTK_OBJECT (pan_window), "key_press_event",
		GTK_SIGNAL_FUNC (key_pan), NULL);

	gtk_widget_set_events (draw_pan, GDK_ALL_EVENTS_MASK);

	gtk_widget_show (pan_window);
}



////	VIEW WINDOW

static int vw_width, vw_height, vw_last_x, vw_last_y, vw_move_layer;
#if GTK_MAJOR_VERSION == 1
static int old_split_pos = -1;
#endif

GtkWidget *vw_drawing = NULL;
gboolean vw_focus_on = FALSE;

static GtkWidget *vw_scrolledwindow;
static gboolean view_first_move = TRUE;

void render_layers( unsigned char *rgb, int px, int py, int pw, int ph,
	double czoom, int lr0, int lr1 )
{
	chanlist tlist;
	png_color *pal;
	unsigned char *tmp, **img;
	int i, j, ii, jj, ll, wx0, wy0, wx1, wy1, xof, xpm, opac, bpp;
	int ddx, ddy, mx, mw, my, mh;
	int pw2 = pw, ph2 = ph, dx = 0, dy = 0, pw3 = pw * 3;
	int zoom = 1, scale = 1;

	if (czoom < 1.0) zoom = rint(1.0 / czoom);
	else scale = rint(czoom);

	/* Align on selected layer if needed */
	if (layers_total && layer_selected && (zoom > 1))
	{
		dx = layer_table[layer_selected].x % zoom;
		if (dx < 0) dx += zoom;
		dy = layer_table[layer_selected].y % zoom;
		if (dy < 0) dy += zoom;
	}

	/* Apply background bounds if needed */
	if (layers_pastry_cut)
	{
		if (px < 0)
		{
			rgb -= px * 3;
			pw2 += px;
			px = 0;
		}
		if (py < 0)
		{
			rgb -= py * pw3;
			ph2 += py;
			py = 0;
		}
		i = mem_width;
		j = mem_height;
		if (layers_total && layer_selected)
		{
			i = layer_table[0].image->mem_width;
			j = layer_table[0].image->mem_height;
		}
		i = ((i - dx) / zoom) * scale;
		j = ((j - dy) / zoom) * scale;
		if (pw2 > i) pw2 = i;
		if (ph2 > j) ph2 = j;
		if ((pw2 <= 0) || (ph2 <= 0)) return;
	}
	xof = px % scale;
	if (xof < 0) xof += scale;
	memset(tlist, 0, sizeof(chanlist));

	/* Get image-space bounds */
	i = px % scale < 0 ? 1 : 0;
	j = py % scale < 0 ? 1 : 0;
	wx0 = (px / scale) * zoom + dx - i;
	wy0 = (py / scale) * zoom + dy - j;
	wx1 = px + pw2 - 1;
	wy1 = py + ph2 - 1;
	i = wx1 % scale < 0 ? 1 : 0;
	j = wy1 % scale < 0 ? 1 : 0;
	wx1 = (wx1 / scale) * zoom + dx - i;
	wy1 = (wy1 / scale) * zoom + dy - j;

	for (ll = 0; ll <= layers_total; ll++)
	{
		if (ll && !layer_table[ll].visible) continue;
		i = layer_table[ll].x;
		j = layer_table[ll].y;
		if (!ll) i = j = 0;
		if (ll == layer_selected)
		{
			ii = i + mem_width;
			jj = j + mem_height;
		}
		else
		{
			ii = i + layer_table[ll].image->mem_width;
			jj = j + layer_table[ll].image->mem_height;
		}
		if ((i > wx1) || (j > wy1) || (ii <= wx0) || (jj <= wy0))
			continue;
		ddx = ddy = mx = my = 0;
		if (zoom > 1)
		{
			if (i > wx0) mx = (i - wx0 + zoom - 1) / zoom;
			if (j > wy0) my = (j - wy0 + zoom - 1) / zoom;
			ddx = wx0 + mx * zoom - i;
			ddy = wy0 + my * zoom - j;
			if (ii - 1 >= wx1) mw = pw2 - mx;
			else mw = (ii - i - ddx) / zoom;
			if (jj - 1 >= wy1) mh = ph2 - my;
			else mh = (jj - j - ddy) / zoom;
			if ((mw <= 0) || (mh <= 0)) continue;
		}
		else
		{
			if (i > wx0) mx = i * scale - px;
			else ddx = wx0 - i;
			if (j > wy0) my = j * scale - py;
			else ddy = wy0 - j;
			if (ii - 1 >= wx1) mw = pw2 - mx;
			else mw = ii * scale - px - mx;
			if (jj - 1 >= wy1) mh = ph2 - my;
			else mh = jj * scale - py - my;
		}
		tmp = rgb + (my * pw + mx) * 3;
		xpm = -1;
		opac = 255;
		if (ll)
		{
			opac = (layer_table[ll].opacity * 255 + 50) / 100;
			if (layer_table[ll].use_trans) xpm = layer_table[ll].trans;
		}
		if (ll == layer_selected)
		{
			bpp = mem_img_bpp;
			pal = mem_pal;
			img = mem_img;
		}
		else
		{
			bpp = layer_table[ll].image->mem_img_bpp;
			pal = layer_table[ll].image->mem_pal;
			img = layer_table[ll].image->mem_img;
		}
		setup_row(xof + mx, mw, czoom, ii - i, xpm, opac, bpp, pal);
		i = (py + my) % scale;
		if (i < 0) i += scale;
		mh = mh * zoom + i;
		for (j = -1; i < mh; i += zoom , tmp += pw3)
		{
			if (i / scale == j)
			{
				memcpy(tmp, tmp - pw3, mw * 3);
				continue;
			}
			j = i / scale;
			render_row(tmp, img, ddx, ddy + j, tlist);
			if (ll != layer_selected) continue;
			overlay_row(tmp, img, ddx, ddy + j, tlist);
		}
	}
}

void view_render_rgb( unsigned char *rgb, int px, int py, int pw, int ph, double czoom )
{
	if (!rgb) return; /* Paranoia */
	render_layers(rgb, px, py, pw, ph, czoom, 0, layers_total);
}

void vw_focus_view()						// Focus view window to main window
{
	int nv_h, nv_v, px, py;
	float main_h = 0.5, main_v = 0.5;
	GtkAdjustment *hori, *vert;

	if ( vw_drawing == NULL ) return;			// Bail out if not visible
	if ( !vw_focus_on ) return;				// Only focus if user wants to

	hori = gtk_scrolled_window_get_hadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );
	vert = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );

	if ( hori->page_size > mem_width*can_zoom ) main_h = 0.5;
	else main_h = ( hori->value + hori->page_size/2 ) / (mem_width*can_zoom);

	if ( vert->page_size > mem_height*can_zoom ) main_v = 0.5;
	else main_v = ( vert->value + vert->page_size/2 ) / (mem_height*can_zoom);

	if ( layers_total > 0 )
	{
		if ( layer_selected != 0 )
		{		// If we are editing a layer above the background make adjustments
			px = main_h * (mem_width - 1) + layer_table[layer_selected].x;
			py = main_v * (mem_height - 1) + layer_table[layer_selected].y;
			mtMAX( px, px, 0)
			mtMAX( py, py, 0)
			mtMIN( px, px, layer_table[0].image->mem_width - 1)
			mtMIN( py, py, layer_table[0].image->mem_height - 1)
			if ( px == 0 )
				main_h = 0;	// Traps division by zero if width=1
			else
				main_h = ((float) px ) / ( layer_table[0].image->mem_width - 1 );

			if ( py == 0 )
				main_v = 0;	// Traps division by zero if width=1
			else
				main_v = ((float) py ) / ( layer_table[0].image->mem_height - 1 );
		}
	}

	hori = gtk_scrolled_window_get_hadjustment( GTK_SCROLLED_WINDOW(vw_scrolledwindow) );
	vert = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(vw_scrolledwindow) );

	if ( hori->page_size < vw_width )
	{
		nv_h = vw_width*main_h - hori->page_size/2;
		if ( (nv_h + hori->page_size) > vw_width ) nv_h = vw_width - hori->page_size;
		mtMAX( nv_h, nv_h, 0 )
	}
	else	nv_h = 0;

	if ( vert->page_size < vw_height )
	{
		nv_v = vw_height*main_v - vert->page_size/2;
		if ( (nv_v + vert->page_size) > vw_height )
			nv_v = vw_height - vert->page_size;
		mtMAX( nv_v, nv_v, 0 )
	}
	else	nv_v = 0;

	hori->value = nv_h;
	vert->value = nv_v;

	gtk_adjustment_value_changed( hori );		// Update position of view window scrollbars
	gtk_adjustment_value_changed( vert );
}


gboolean vw_configure( GtkWidget *widget, GdkEventConfigure *event )
{
	int ww = widget->allocation.width, wh = widget->allocation.height
		, new_margin_x = margin_view_x, new_margin_y = margin_view_y
		;

	if ( canvas_image_centre )
	{
		if ( ww > vw_width ) new_margin_x = (ww - vw_width) / 2;
		else new_margin_x = 0;

		if ( wh > vw_height ) new_margin_y = (wh - vw_height) / 2;
		else new_margin_y = 0;
	}
	else
	{
		new_margin_x = 0;
		new_margin_y = 0;
	}

	if ( new_margin_x != margin_view_x || new_margin_y != margin_view_y )
	{
		gtk_widget_queue_draw(vw_drawing);
			// Force redraw of whole canvas as the margin has shifted
		margin_view_x = new_margin_x;
		margin_view_y = new_margin_y;
	}

	return TRUE;
}

void vw_align_size( float new_zoom )
{
	int sw = mem_width, sh = mem_height;

	vw_zoom = new_zoom;

	if ( layers_total>0 && layer_selected!=0 )
	{
		sw = layer_table[0].image->mem_width;
		sh = layer_table[0].image->mem_height;
	}

	sw *= vw_zoom;
	sh *= vw_zoom;

	if ( vw_width != sw || vw_height != sh )
	{
		vw_width = sw;
		vw_height = sh;
		gtk_widget_set_usize( vw_drawing, vw_width, vw_height );
	}
	vw_focus_view();
}

void vw_repaint( int px, int py, int pw, int ph )
{
	unsigned char *rgb;

	mtMAX(px, px, 0)
	mtMAX(py, py, 0)
	if ( pw<=0 || ph<=0 ) return;

	rgb = grab_memory( pw*ph*3, mem_background );

	if ( rgb != NULL )
	{

		view_render_rgb( rgb, px - margin_view_x, py - margin_view_y, pw, ph, vw_zoom );
		gdk_draw_rgb_image ( vw_drawing->window, vw_drawing->style->black_gc,
			px, py, pw, ph, GDK_RGB_DITHER_NONE, rgb, pw*3 );

		free( rgb );
	}
}

static gint vw_expose( GtkWidget *widget, GdkEventExpose *event )
{
	int px, py, pw, ph;

	px = event->area.x;
	py = event->area.y;
	pw = event->area.width;
	ph = event->area.height;

	vw_repaint( px, py, pw, ph );

	return FALSE;
}

void vw_update_area( int x, int y, int w, int h )	// Update x,y,w,h area of current image
{
	int zoom, scale, dx = 0, dy = 0;

	if ( vw_drawing == NULL ) return;
	
	if ( layer_selected > 0 )
	{
		x += (dx = layer_table[layer_selected].x);
		y += (dy = layer_table[layer_selected].y);
	}

	if (vw_zoom < 1.0)
	{
		zoom = rint(1.0 / vw_zoom);
		dx %= zoom;
		x -= dx + (dx < 0 ? zoom : 0);
		dy %= zoom;
		y -= dy + (dy < 0 ? zoom : 0);
		w += x - 1;
		h += y - 1;
		w = w < 0 ? -((zoom - w - 1) / zoom) : w / zoom;
		h = h < 0 ? -((zoom - h - 1) / zoom) : h / zoom;
		x = x < 0 ? -(-x / zoom) : (x + zoom - 1) / zoom;
		y = y < 0 ? -(-y / zoom) : (y + zoom - 1) / zoom;
		w -= x - 1;
		h -= y - 1;
		if ((w <= 0) || (h <= 0)) return;
	}
	else
	{
		scale = rint(vw_zoom);
		x *= scale;
		y *= scale;
		w *= scale;
		h *= scale;
	}

	gtk_widget_queue_draw_area(vw_drawing, x, y, w, h);
}

static void vw_mouse_event(int x, int y, guint state, guint button)
{
	unsigned char *rgb, **img;
	int dx, dy, i, lx, ly, lw, lh, bpp, tpix, ppix, ofs;
	int zoom = 1, scale = 1;
	png_color *pal;

	if (!button || !layers_total)
	{
		view_first_move = TRUE;
		return;
	}

	if (vw_zoom < 1.0) zoom = rint(1.0 / vw_zoom);
	else scale = rint(vw_zoom);

	x = ((x - margin_view_x) / scale) * zoom;
	y = ((y - margin_view_y) / scale) * zoom;

	/* Align on selected layer if needed */
	if (layer_selected && (zoom > 1))
	{
		dx = layer_table[layer_selected].x % zoom;
		dy = layer_table[layer_selected].y % zoom;
		x -= dx + (dx < 0 ? zoom : 0);
		y -= dy + (dy < 0 ? zoom : 0);
	}

	if ( !view_first_move )
	{
		if ( vw_move_layer > 0 )
		{
			dx = x - vw_last_x;
			dy = y - vw_last_y;
			move_layer_relative(vw_move_layer, dx, dy);
		}
	}
	else
	{
		vw_move_layer = -1;		// Which layer has the user clicked?
		for (i = layers_total; i > 0; i--)
		{
			lx = layer_table[i].x;
			ly = layer_table[i].y;
			if ( i == layer_selected )
			{
				lw = mem_width;
				lh = mem_height;
				bpp = mem_img_bpp;
				img = mem_img;
				pal = mem_pal;
			}
			else
			{
				lw = layer_table[i].image->mem_width;
				lh = layer_table[i].image->mem_height;
				bpp = layer_table[i].image->mem_img_bpp;
				img = layer_table[i].image->mem_img;
				pal = layer_table[i].image->mem_pal;
			}
			rgb = img[CHN_IMAGE];

			/* Is click within layer box? */
			if ( x>=lx && x<(lx + lw) && y>=ly && y<(ly + lh) &&
				layer_table[i].visible )
			{
				ofs = (x-lx) + lw*(y-ly);
				/* Is click on a non transparent pixel? */
				if (img[CHN_ALPHA])
				{
					if (img[CHN_ALPHA][ofs] < (bpp == 1 ? 255 : 1))
						continue;
				}
				if ( layer_table[i].use_trans )
				{
					tpix = layer_table[i].trans;
					if (bpp == 1) ppix = rgb[ofs];
					else
					{
						tpix = PNG_2_INT(pal[tpix]);
						ppix = MEM_2_INT(rgb, ofs * 3);
					}
					if (tpix == ppix) continue;
				}
				vw_move_layer = i;
				break;
			}
		}
		ppix = vw_move_layer;
		if (ppix < 0) ppix = 0; /* Select background */
		layer_choose(ppix);
	}
	view_first_move = FALSE;
	vw_last_x = x;
	vw_last_y = y;
}

static gint view_window_motion( GtkWidget *widget, GdkEventMotion *event )
{
	int x, y;
	GdkModifierType state;
	guint button = 0;

	if (event->is_hint) gdk_window_get_pointer (event->window, &x, &y, &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

	if (state & GDK_BUTTON2_MASK) button = 2;
	if (state & GDK_BUTTON3_MASK) button = 3;
	if (state & GDK_BUTTON1_MASK) button = 1;
	if ( (state & GDK_BUTTON1_MASK) && (state & GDK_BUTTON3_MASK) ) button = 13;
	vw_mouse_event( x, y, state, button );

	return TRUE;
}

static gint view_window_click( GtkWidget *widget, GdkEventButton *event )
{
	vw_mouse_event( event->x, event->y, event->state, event->button );

	return TRUE;
}

void view_show()
{
	gtk_widget_show(vw_scrolledwindow);		// Not good in GTK+1!
	view_showing = TRUE;
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(menu_view[0]), TRUE );
	toolbar_viewzoom(TRUE);
#if GTK_MAJOR_VERSION == 1
	if ( old_split_pos >= 0 )
	{
		gtk_paned_set_position( GTK_PANED(main_vsplit), old_split_pos + old_split_pos/105 + 3 );
	}
	gtk_paned_set_gutter_size( GTK_PANED(main_vsplit), 6 );
	gtk_paned_set_handle_size( GTK_PANED(main_vsplit), 10 );
#endif
}

void view_hide()
{
	gtk_widget_hide(vw_scrolledwindow);		// Not good in GTK+1!
	view_showing = FALSE;
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(menu_view[0]), FALSE );
	toolbar_viewzoom(FALSE);
#if GTK_MAJOR_VERSION == 1
	old_split_pos = GTK_PANED(main_vsplit)->handle_xpos;
	if ( old_split_pos >= 0 )
	{
		gtk_paned_set_position( GTK_PANED(main_vsplit), 10000 );
	}
	gtk_paned_set_gutter_size( GTK_PANED(main_vsplit), 0 );
	gtk_paned_set_handle_size( GTK_PANED(main_vsplit), 0 );
#endif
}


void pressed_centralize( GtkMenuItem *menu_item, gpointer user_data )
{
	canvas_image_centre = GTK_CHECK_MENU_ITEM(menu_item)->active;
	inifile_set_gboolean( "imageCentre", canvas_image_centre );
	force_main_configure();		// Force configure of main window - for centalizing code
	update_all_views();
}

void pressed_view_focus( GtkMenuItem *menu_item, gpointer user_data )
{
	vw_focus_on = GTK_CHECK_MENU_ITEM(menu_item)->active;
	inifile_set_gboolean("view_focus", vw_focus_on );
	vw_focus_view();
}

void pressed_view( GtkMenuItem *menu_item, gpointer user_data )
{
	if ( GTK_CHECK_MENU_ITEM(menu_item)->active ) view_show();
	else view_hide();
}

void init_view( GtkWidget *canvas, GtkWidget *scroll )
{
	vw_focus_on = inifile_get_gboolean("view_focus", TRUE );
	vw_width = 1;
	vw_height = 1;

	view_showing = FALSE;
	vw_scrolledwindow = scroll;
	vw_drawing = canvas;

	gtk_signal_connect_object( GTK_OBJECT(vw_drawing), "configure_event",
		GTK_SIGNAL_FUNC (vw_configure), GTK_OBJECT(vw_drawing) );
	gtk_signal_connect_object( GTK_OBJECT(vw_drawing), "expose_event",
		GTK_SIGNAL_FUNC (vw_expose), GTK_OBJECT(vw_drawing) );

	gtk_signal_connect_object( GTK_OBJECT(vw_drawing), "button_press_event",
		GTK_SIGNAL_FUNC (view_window_click), NULL );
	gtk_signal_connect_object( GTK_OBJECT(vw_drawing), "motion_notify_event",
		GTK_SIGNAL_FUNC (view_window_motion), NULL );
	gtk_widget_set_events (vw_drawing, GDK_ALL_EVENTS_MASK);
}



////	TAKE SCREENSHOT

void screen_copy_pixels( unsigned char *rgb, int w, int h )
{
	mem_pal_load_def();
	if (mem_new( w, h, 3 )) return;
	memcpy(mem_img[CHN_IMAGE], rgb, w * h * 3);
}

gboolean grab_screen()
{
	int width = gdk_screen_width(), height = gdk_screen_height();

#if GTK_MAJOR_VERSION == 1
	GdkImlibImage *screenshot = NULL;

	screenshot = gdk_imlib_create_image_from_drawable( (GdkWindow *)&gdk_root_parent,
			NULL, 0, 0, width, height );
	if ( screenshot != NULL )
	{
		screen_copy_pixels( screenshot->rgb_data, width, height );
		gdk_imlib_kill_image( screenshot );
#endif
#if GTK_MAJOR_VERSION == 2
	GdkPixbuf *screenshot = NULL;

	screenshot = gdk_pixbuf_get_from_drawable (NULL, gdk_get_default_root_window(), NULL,
			0, 0, 0, 0, width, height);

	if ( screenshot != NULL )
	{
		screen_copy_pixels( gdk_pixbuf_get_pixels( screenshot ), width, height );
		g_object_unref( screenshot );
#endif
	} else return FALSE;

	return TRUE;
}


////	TEXT TOOL

GtkWidget *text_window, *text_font_window, *text_toggle[3], *text_spin[2];

#define PAD_SIZE 4

gint render_text( GtkWidget *widget )
{
	GdkImage *t_image = NULL;
	GdkPixmap *text_pixmap;
	gboolean antialias[3] = {
			inifile_get_gboolean( "fontAntialias", FALSE ),
			inifile_get_gboolean( "fontAntialias1", FALSE ),
			inifile_get_gboolean( "fontAntialias2", FALSE )
			};
	unsigned char *source, *dest, *dest2, *pat_off, r, g, b, pix_and = 255, v;
	int width, height, i, j, bpp, back;

#if GTK_MAJOR_VERSION == 2
	int tx = PAD_SIZE, ty = PAD_SIZE;
#if GTK_MINOR_VERSION >= 6
	int w2, h2;
	float degs, angle;
	PangoMatrix matrix = PANGO_MATRIX_INIT;
#endif
	PangoContext *context;
	PangoLayout *layout;
	PangoFontDescription *font_desc;

	context = gtk_widget_create_pango_context (widget);
	layout = pango_layout_new( context );
	font_desc = pango_font_description_from_string( inifile_get( "lastTextFont", "" ) );

	pango_layout_set_text( layout, inifile_get( "textString", "" ), -1 );
	pango_layout_set_font_description( layout, font_desc );

#if GTK_MINOR_VERSION >= 6
	degs = inifile_get_gfloat( "fontAngle", 0.0 );
	angle = G_PI*degs/180;

	if ( antialias[2] )		// Rotation Toggle
	{
		pango_matrix_rotate (&matrix, degs);
		pango_context_set_matrix (context, &matrix);
		pango_layout_context_changed( layout );
	}
#endif
	pango_layout_get_pixel_size( layout, &width, &height );
	pango_font_description_free( font_desc );

#if GTK_MINOR_VERSION >= 6
	if ( antialias[2] )		// Rotation Toggle
	{
		w2 = abs(width * cos(angle)) + abs(height * sin(angle));
		h2 = abs(width * sin(angle)) + abs(height * cos(angle));
		width = w2;
		height = h2;
	}
#endif
	width += PAD_SIZE*2;
	height += PAD_SIZE*2;

	text_pixmap = gdk_pixmap_new(widget->window, width, height, -1);

	gdk_draw_rectangle (text_pixmap, widget->style->white_gc, TRUE, 0, 0, width, height);
	gdk_draw_layout( text_pixmap, widget->style->black_gc, tx, ty, layout );

	t_image = gdk_image_get( text_pixmap, 0, 0, width, height );
	bpp = t_image->bpp;
#endif
#if GTK_MAJOR_VERSION == 1
	GdkFont *t_font = gdk_font_load( inifile_get( "lastTextFont", "" ) );
	int lbearing, rbearing, f_width, ascent, descent;


	gdk_string_extents( t_font, inifile_get( "textString", "" ),
		&lbearing, &rbearing, &f_width, &ascent, &descent );

	width = rbearing - lbearing + PAD_SIZE*2;
	height = ascent + descent + PAD_SIZE*2;

	text_pixmap = gdk_pixmap_new(widget->window, width, height, -1);
	gdk_draw_rectangle (text_pixmap, widget->style->white_gc, TRUE, 0, 0, width, height);

	gdk_draw_string( text_pixmap, t_font, widget->style->black_gc,
			PAD_SIZE - lbearing, ascent + PAD_SIZE, inifile_get( "textString", "" ) );

	t_image = gdk_image_get( text_pixmap, 0, 0, width, height );
	bpp = ((int) t_image->bpp) / 8;
	gdk_font_unref( t_font );
#endif
	if ( bpp == 2 ) pix_and = 31;
	if ( bpp < 2 ) pix_and = 1;

	back = inifile_get_gint32( "fontBackground", 0 );
	r = mem_pal[back].red;
	g = mem_pal[back].green;
	b = mem_pal[back].blue;

	if ( t_image != NULL )
	{
		if ( mem_clipboard != NULL ) free( mem_clipboard );	// Lose old clipboard
		mem_clipboard = malloc( width * height * mem_img_bpp );
		mem_clip_w = width;
		mem_clip_h = height;
		mem_clip_bpp = mem_img_bpp;
		if ( !antialias[1] ) mem_clip_mask_init(0);
		else mem_clip_mask_clear();
	
		if ( mem_clipboard == NULL || ( !antialias[1] && mem_clip_mask == NULL) )
		{
			alert_box( _("Error"), _("Not enough memory to create clipboard"),
					_("OK"), NULL, NULL );
			if ( mem_clipboard != NULL ) free( mem_clipboard );
			mem_clip_mask_clear();
			text_paste = FALSE;
		}
		else
		{
			text_paste = TRUE;
			for ( j=0; j<height; j++ )
			{
				source = (unsigned char *) (t_image->mem) + j * t_image->bpl;
				dest = mem_clipboard + width * j * mem_img_bpp;
				dest2 = mem_clip_mask + width*j;
				if ( mem_img_bpp == 3 )
				{
					pat_off = mem_col_pat24 + (j%8)*8*3;
					if ( antialias[0] )
					{
					  if ( antialias[1] )	// Antialiased with background
					  {
						for ( i=0; i<width; i++ )
						{
							v = source[0] & pix_and;
							dest[0] = ( (pix_and-v)*pat_off[3*(i%8)] +
								v*r ) / pix_and;
							dest[1] = ( (pix_and-v)*pat_off[3*(i%8)+1] +
								v*g ) / pix_and;
							dest[2] = ( (pix_and-v)*pat_off[3*(i%8)+2] +
								v*b ) / pix_and;

							source += bpp;
							dest += 3;
						}
					  }
					  else			// Antialiased without background
					  {
						for ( i=0; i<width; i++ )
						{
							v = source[0] & pix_and;
							dest2[i] = v * 255 / pix_and;	// Alpha blend

							dest[0] = pat_off[3*(i%8)];
							dest[1] = pat_off[3*(i%8)+1];
							dest[2] = pat_off[3*(i%8)+2];

							source += bpp;
							dest += 3;
						}
					  }
					}
					else
					{
					 for ( i=0; i<width; i++ )
					 {
						if ( (source[0] & pix_and) > (pix_and/2) )
						{				// Background
							if ( !antialias[1] ) dest2[i] = 255;
							dest[0] = r;
							dest[1] = g;
							dest[2] = b;
						}
						else				// Text
						{
							dest[0] = pat_off[3*(i%8)];
							dest[1] = pat_off[3*(i%8)+1];
							dest[2] = pat_off[3*(i%8)+2];
						}
						source += bpp;
						dest += 3;
					 }
					}
				}
				if ( mem_img_bpp == 1 )
				{
					pat_off = mem_col_pat + (j%8)*8;
					for ( i=0; i<width; i++ )
					{
						if ( (source[0] & pix_and) > (pix_and/2) ) // Background
						{
							if ( !antialias[1] ) dest2[i] = 255;
							dest[i] = back;
						}
						else dest[i] = pat_off[i%8];	// Text
						source += bpp;
					}
				}
			}
			gdk_image_destroy(t_image);
		}
	}
	gdk_pixmap_unref(text_pixmap);		// REMOVE PIXMAP
#if GTK_MAJOR_VERSION == 2
	g_object_unref( layout );
	g_object_unref( context );
#endif

	return TRUE;		// Success
}

static gint delete_text( GtkWidget *widget, GdkEvent *event, gpointer data )
{	gtk_widget_destroy( text_window ); return FALSE;	}

static gint paste_text_ok( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	gboolean antialias[3] = { gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(text_toggle[0])),
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(text_toggle[1])),
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(text_toggle[2])) };

	char *t_string = (char *) gtk_font_selection_get_preview_text(
				GTK_FONT_SELECTION(text_font_window) ),
		*t_font_name = gtk_font_selection_get_font_name( GTK_FONT_SELECTION(text_font_window) );

#if GTK_MAJOR_VERSION == 1
	if ( gtk_font_selection_get_font( GTK_FONT_SELECTION(text_font_window) ) == NULL )
		return FALSE;
#endif

#if GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >= 6
	gtk_spin_button_update( GTK_SPIN_BUTTON(text_spin[1]) );
	inifile_set_gfloat( "fontAngle",
		gtk_spin_button_get_value_as_float( GTK_SPIN_BUTTON(text_spin[1]) ) );
#endif

	gtk_spin_button_update( GTK_SPIN_BUTTON(text_spin[0]) );
	inifile_set_gint32( "fontBackground",
		gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(text_spin[0]) ) );

	inifile_set( "lastTextFont", t_font_name );
	inifile_set( "textString", t_string );
	inifile_set_gboolean( "fontAntialias", antialias[0] );
	inifile_set_gboolean( "fontAntialias1", antialias[1] );
	inifile_set_gboolean( "fontAntialias2", antialias[2] );

	render_text( widget );
	if ( mem_clipboard != NULL ) pressed_paste_centre( NULL, NULL );

	delete_text( widget, event, data );

	return FALSE;
}

void pressed_text( GtkMenuItem *menu_item, gpointer user_data )
{
	GtkWidget *button, *vbox, *hbox;
	GtkAccelGroup* ag = gtk_accel_group_new();
	GtkObject *adj;

	text_window = add_a_window( GTK_WINDOW_TOPLEVEL, _("Paste Text"), GTK_WIN_POS_CENTER, TRUE );
	gtk_window_set_default_size( GTK_WINDOW(text_window), 400, 400 );

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (text_window), vbox);

	text_font_window = gtk_font_selection_new ();
	gtk_widget_show(text_font_window);
	gtk_container_set_border_width (GTK_CONTAINER (text_font_window), 4);

	gtk_box_pack_start( GTK_BOX(vbox), text_font_window, TRUE, TRUE, 0 );

	add_hseparator( vbox, 200, 10 );

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);

	text_toggle[0] = add_a_toggle( _("Antialias"), hbox,
			inifile_get_gboolean( "fontAntialias", FALSE ) );
	text_toggle[1] = add_a_toggle( _("Background colour ="), hbox,
			inifile_get_gboolean( "fontAntialias1", FALSE ) );
	adj = gtk_adjustment_new( inifile_get_gint32( "fontBackground", 0 ) % mem_cols,
		0, mem_cols-1, 1, 10, 10 );
	text_spin[0] = gtk_spin_button_new( GTK_ADJUSTMENT (adj), 1, 0 );
	gtk_widget_show(text_spin[0]);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (text_spin[0]), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), text_spin[0], FALSE, TRUE, 5);
	text_toggle[2] = add_a_toggle( _("Angle of rotation ="), hbox, FALSE );

	if ( GTK_MAJOR_VERSION == 2 )
	{
#if GTK_MINOR_VERSION >= 6
		adj = gtk_adjustment_new( inifile_get_gfloat( "fontAngle", 0 ),
			-360, 360, 1, 10, 10 );
		text_spin[1] = gtk_spin_button_new( GTK_ADJUSTMENT (adj), 1, 0 );
		gtk_widget_show(text_spin[1]);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (text_spin[1]), TRUE);
		gtk_box_pack_start (GTK_BOX (hbox), text_spin[1], FALSE, TRUE, 5);
		gtk_spin_button_set_digits(GTK_SPIN_BUTTON(text_spin[1]), 2);
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(text_toggle[2]), 
			inifile_get_gboolean( "fontAntialias2", FALSE ) );
#else
		gtk_widget_hide( text_toggle[2] );
#endif
	}
	else	gtk_widget_hide( text_toggle[2] );

	if ( mem_img_bpp == 1 || GTK_MAJOR_VERSION == 1 )
		gtk_widget_hide( text_toggle[0] );

	add_hseparator( vbox, 200, 10 );

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);

	button = add_a_button(_("Cancel"), 5, hbox, TRUE);
	gtk_widget_set_usize (button, 120, -2);
	gtk_signal_connect_object( GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(delete_text), GTK_OBJECT(text_window));
	gtk_widget_add_accelerator (button, "clicked", ag, GDK_Escape, 0, (GtkAccelFlags) 0);

	button = add_a_button(_("Paste Text"), 5, hbox, TRUE);
	gtk_signal_connect_object( GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(paste_text_ok), GTK_OBJECT(text_window));
	gtk_widget_set_usize (button, 120, -2);
	gtk_widget_add_accelerator (button, "clicked", ag, GDK_KP_Enter, 0, (GtkAccelFlags) 0);
	gtk_widget_add_accelerator (button, "clicked", ag, GDK_Return, 0, (GtkAccelFlags) 0);

	gtk_signal_connect_object (GTK_OBJECT (text_window), "delete_event",
		GTK_SIGNAL_FUNC (delete_text), NULL);

	gtk_widget_show(text_window);
	gtk_window_add_accel_group(GTK_WINDOW (text_window), ag);
	gtk_window_set_transient_for( GTK_WINDOW(text_window), GTK_WINDOW(main_window) );

	gtk_font_selection_set_font_name( GTK_FONT_SELECTION(text_font_window),
		inifile_get( "lastTextFont", "-misc-fixed-bold-r-normal-*-*-120-*-*-c-*-iso8859-1" ) );
	gtk_font_selection_set_preview_text( GTK_FONT_SELECTION(text_font_window),
		inifile_get( "textString", _("Enter Text Here") ) );

	gtk_widget_grab_focus( GTK_FONT_SELECTION(text_font_window)->preview_entry );
}
