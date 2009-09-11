/*	layer.c
	Copyright (C) 2005-2008 Mark Tyler and Dmitry Groshev

	This file is part of mtPaint.

	mtPaint is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	mtPaint is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with mtPaint in the file COPYING.
*/

#include "mygtk.h"
#include "memory.h"
#include "png.h"
#include "layer.h"
#include "mainwindow.h"
#include "otherwindow.h"
#include "canvas.h"
#include "inifile.h"
#include "global.h"
#include "viewer.h"
#include "ani.h"
#include "channels.h"
#include "toolbar.h"
#include "icons.h"

//	Layer toolbar buttons
#define LTB_NEW    0
#define LTB_RAISE  1
#define LTB_LOWER  2
#define LTB_DUP    3
#define LTB_CENTER 4
#define LTB_DEL    5
#define LTB_CLOSE  6

#define TOTAL_ICONS_LAYER 7


int	layers_total,		// Layers currently being used
	layer_selected,		// Layer currently selected in the layers window
	layers_changed;		// 0=Unchanged

char layers_filename[PATHBUF];		// Current filename for layers file
int	show_layers_main,		// Show all layers in main window
	layers_pastry_cut;		// Pastry cut layers in view area (for animation previews)


// !!! Always follow adding/changing layer's image_info by update_undo()
layer_node layer_table[MAX_LAYERS + 1];	// Table of layer info


static void layer_clear_slot(int l, int visible)
{
	memset(layer_table + l, 0, sizeof(layer_node));
	layer_table[l].opacity = 100;
	layer_table[l].visible = visible;
}

void layers_init()
{
	layer_clear_slot(0, TRUE);
	strncpy0(layer_table[0].name, _("Background"), LAYER_NAMELEN);
	layer_table[0].image = calloc(1, sizeof(layer_image));
}

/* Allocate layer image, its channels and undo stack
 * !!! Must be followed by update_undo() after setting up image is done */
layer_image *alloc_layer(int w, int h, int bpp, int cmask, chanlist src)
{
	layer_image *lim;

	if (src) cmask = cmask_from(src);
	lim = calloc(1, sizeof(layer_image));
	if (!lim) return (NULL);
	if (init_undo(&lim->image_.undo_, mem_undo_depth) &&
		mem_alloc_image(AI_COPY, &lim->image_, w, h, bpp, cmask, src))
		return (lim);
	free(lim->image_.undo_.items);
	free(lim);
	return (NULL);
}

static void repaint_layer(int l)	// Repaint layer in view/main window
{
	layer_node *t = layer_table + l;
	image_info *image;
	int lx, ly, lw, lh;

	image = l == layer_selected ? &mem_image : &t->image->image_;
	lw = image->width;
	lh = image->height;
	lx = t->x;
	ly = t->y;
	if (layer_selected)
	{
		lx -= layer_table[layer_selected].x;
		ly -= layer_table[layer_selected].y;
	}

	vw_update_area(lx, ly, lw, lh);
	if (show_layers_main) main_update_area(lx, ly, lw, lh);
}

static void repaint_layers()
{
	if (show_layers_main) gtk_widget_queue_draw(drawing_canvas);
	if (view_showing) gtk_widget_queue_draw(vw_drawing);
}


///	LAYERS WINDOW

GtkWidget *layers_window = NULL;

typedef struct {
	GtkWidget *item, *name, *toggle;
} layer_item;

static GtkWidget *layer_list, *entry_layer_name, *layer_x, *layer_y,
	*layer_tools[TOTAL_ICONS_LAYER], *layer_spin, *layer_slider;
static layer_item layer_list_data[MAX_LAYERS + 1];

static int layers_initialized;		// Indicates if initializing is complete


static int layers_sensitive(int state)
{
	int res;

	if (!layers_window) return (TRUE);
	res = GTK_WIDGET_SENSITIVE(layers_window);
	gtk_widget_set_sensitive(layers_window, state);
	return (res);
}

static void layers_update_titlebar()		// Update filename in titlebar
{
	char txt[300], txt2[PATHTXT], *extra = "-";

	if ( layers_window == NULL ) return;		// Don't bother if window is not showing

	gtkuncpy(txt2, layers_filename, PATHTXT);

	if ( layers_changed == 1 ) extra = _("(Modified)");

	snprintf( txt, 290, "%s %s %s", _("Layers"), extra, txt2[0] ? txt2 :
		_("Untitled"));

	gtk_window_set_title (GTK_WINDOW (layers_window), txt );
}

void layers_notify_changed()			// Layers have just changed - update vars as needed
{
	if ( layers_changed != 1 )
	{
		layers_changed = 1;
		layers_update_titlebar();
	}
}

static void layers_notify_unchanged()		// Layers have just been unchanged (saved) - update vars as needed
{
	if ( layers_changed != 0 )
	{
		layers_changed = 0;
		layers_update_titlebar();
	}
}


void layer_copy_from_main( int l )	// Copy info from main image to layer
{
	layer_image *lp = layer_table[l].image;

	lp->image_ = mem_image;
	lp->state_ = mem_state;
	lp->image_.undo_.size = 0; // Invalidate
	update_undo(&lp->image_); // Safety net
}

void layer_copy_to_main( int l )		// Copy info from layer to main image
{
	layer_image *lp = layer_table[l].image;

	mem_image = lp->image_;
	mem_state = lp->state_;
}

/* !!! An evil hack for reliably moving focus around in GtkList */
static void layer_select_slot(int slot)
{
	GtkWidget *item, *fw;

	if (!layers_window) return;

	item = layer_list_data[slot].item;
	fw = GTK_WINDOW(layers_window)->focus_widget;

	/* Focus is somewhere in list - move it, selection will follow */
	if (fw && gtk_widget_is_ancestor(fw, layer_list))
		gtk_widget_grab_focus(item);
	else /* Focus is elsewhere - move whatever remains, then */
	{
	/* !!! For simplicity, an undocumented field is used; a bit less hacky
	 * but longer is to set focus child to item, NULL, and item again - WJ */
		gtk_container_set_focus_child(GTK_CONTAINER(layer_list), item);
		GTK_LIST(layer_list)->last_focus_child = item;
	}
}

void shift_layer(int val)
{
	layer_node temp;
	int i, j, x, y, newbkg, lv = layer_selected + val;

	if ((lv < 0) || (lv > layers_total)) return; // Cannot move
	layer_copy_from_main(layer_selected);
	temp = layer_table[layer_selected];
	layer_table[layer_selected] = layer_table[lv];
	layer_table[lv] = temp;
	newbkg = (layer_selected == 0) || (lv == 0);
	layer_selected = lv;

	/* Background layer changed - shift the entire stack */
	if (newbkg)
	{
		x = layer_table[0].x;
		y = layer_table[0].y;
		for (i = 0; i <= layers_total; i++)
		{
			layer_table[i].x -= x;
			layer_table[i].y -= y;
		}
	}

	// Updated 2 list items - Text name + visible toggle
	for (i = layer_selected , j = 0; j < 2; i -= val , j++)
	{
		gtk_label_set_text(GTK_LABEL(layer_list_data[i].name), layer_table[i].name);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(layer_list_data[i].toggle),
			layer_table[i].visible);
	}

	layer_select_slot(layer_selected);
	layers_notify_changed();

	if (newbkg)	// Background layer changed
	{
		vw_realign();
		repaint_layers();
	}
	else repaint_layer(layer_selected);	// Regular layer shifted
}

void layer_show_new()
{
	layer_copy_from_main(layer_selected);
	layer_copy_to_main(layer_selected = layers_total);
	update_main_with_new_layer();
	layers_notify_changed();

	if (layers_window)
	{
		layer_item *l = layer_list_data + layers_total;
		layer_node *t = layer_table + layers_total;

		// Disable new/duplicate if we have max layers
		if (layers_total >= MAX_LAYERS)
		{
			gtk_widget_set_sensitive(layer_tools[LTB_NEW], FALSE);
			gtk_widget_set_sensitive(layer_tools[LTB_DUP], FALSE);
		}

		gtk_widget_show(l->item);
		gtk_widget_set_sensitive(l->item, TRUE); // Enable list item

		gtk_label_set_text(GTK_LABEL(l->name), t->name );
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l->toggle), t->visible);

		layer_select_slot(layers_total);
	}
}

int layer_add(int w, int h, int bpp, int cols, png_color *pal, int cmask)
{
	layer_image *lim;

	if (layers_total >= MAX_LAYERS) return (FALSE);

	lim = alloc_layer(w, h, bpp, cmask, NULL);
	if (!lim)
	{
		memory_errors(1);
		return (FALSE);
	}
	lim->state_.xpm_trans = lim->state_.xbm_hot_x = lim->state_.xbm_hot_y = -1;
	lim->state_.channel = lim->image_.img[mem_channel] ? mem_channel : CHN_IMAGE;

	lim->image_.cols = cols;
	if (pal) mem_pal_copy(lim->image_.pal, pal);
	else mem_scale_pal(lim->image_.pal, 0, 0,0,0, cols - 1, 255,255,255);

	init_istate(&lim->state_, &lim->image_);
	update_undo(&lim->image_);

	layers_total++;
	layer_clear_slot(layers_total, TRUE);
	layer_table[layers_total].image = lim;

	/* Start with fresh animation data if new */
	if (layers_total == 1) ani_init();

	return (TRUE);
}

/* !!! No calling GTK+ until after updating the structures - we don't need a
 * recursive call in the middle of it (possible on a slow machine) - WJ */
void layer_new(int w, int h, int bpp, int cols, png_color *pal, int cmask)
{
	if (layer_add(w, h, bpp, cols, pal, cmask)) layer_show_new();
}

/* !!! Same as above: modify structures, *then* show results - WJ */
void layer_press_duplicate()
{
	layer_image *lim, *ls;

	if (layers_total >= MAX_LAYERS) return;

	lim = alloc_layer(mem_width, mem_height, mem_img_bpp, 0, mem_img);
	if (!lim)
	{
		memory_errors(1);
		return;
	}

	// Copy layer info
	layer_copy_from_main(layer_selected);
	layers_total++;
	layer_table[layers_total] = layer_table[layer_selected];
	layer_table[layers_total].image = lim;
	ls = layer_table[layer_selected].image;

	lim->state_ = ls->state_;
	mem_pal_copy(lim->image_.pal, ls->image_.pal);
	lim->image_.cols = ls->image_.cols;
	update_undo(&lim->image_);

	// Copy across position data
	memcpy(lim->ani_pos, ls->ani_pos, sizeof(lim->ani_pos));

	layer_show_new();
}

void layer_delete(int item)
{
	layer_image *lp = layer_table[item].image;
	int i;

	mem_free_image(&lp->image_, FREE_ALL);
	free(lp);

	// If deleted item is not at the end shuffle rest down
	for (i = item; i < layers_total; i++)
		layer_table[i] = layer_table[i + 1];
	memset(layer_table + layers_total, 0, sizeof(layer_node));
	layers_total--;
}

void layer_refresh_list()
{
	static int in_refresh;
	int i;

	if (!layers_window) return;

	// As befits a refresh, this should show the final state
	if (in_refresh)
	{
		in_refresh |= 2;
		return;
	}

	while (TRUE)
	{
		in_refresh = 1;
		for (i = 0; i <= layers_total; i++)
		{
			layer_item *l = layer_list_data + i;
			layer_node *t = layer_table + i;

			gtk_widget_show(l->item);
			gtk_widget_set_sensitive(l->item, TRUE);
			gtk_label_set_text(GTK_LABEL(l->name), t->name);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l->toggle),
				t->visible);
		}
		for (; i <= MAX_LAYERS; i++)	// Disable items
		{
			layer_item *l = layer_list_data + i;

			gtk_widget_hide(l->item);
			gtk_widget_set_sensitive(l->item, FALSE);
		}

		gtk_widget_set_sensitive(layer_tools[LTB_RAISE],
			layer_selected < layers_total);
		gtk_widget_set_sensitive(layer_tools[LTB_NEW],
			layers_total < MAX_LAYERS);
		gtk_widget_set_sensitive(layer_tools[LTB_DUP],
			layers_total < MAX_LAYERS);
		if (in_refresh < 2) break;
	}
	in_refresh = 0;
}

void layer_press_delete()
{
	char txt[256];
	int i;

	if (!layer_selected) return; // Deleting background is forbidden
	snprintf(txt, 256, _("Do you really want to delete layer %i (%s) ?"),
		layer_selected, layer_table[layer_selected].name );

	i = alert_box( _("Warning"), txt, _("No"), _("Yes"), NULL );
	if ((i != 2) || (check_for_changes() == 1)) return;

	layer_copy_from_main(layer_selected);
	layer_copy_to_main(--layer_selected);
	layer_delete(layer_selected + 1);

	layer_select_slot(layer_selected);
	layer_refresh_list();
	layers_notify_changed();
	update_main_with_new_layer();
}

static void layer_show_position()
{
	layer_node *t = layer_table + layer_selected;
	int oldinit = layers_initialized;

	if (!layers_window) return;

	layers_initialized = FALSE;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(layer_x), t->x);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(layer_y), t->y);
	layers_initialized = oldinit;
}

void layer_press_centre()
{
	if (!layer_selected) return; // Nothing to do
	layer_table[layer_selected].x = layer_table[0].image->image_.width / 2 -
		mem_width / 2;
	layer_table[layer_selected].y = layer_table[0].image->image_.height / 2 -
		mem_height / 2;
	layer_show_position();
	layers_notify_changed();
	repaint_layers();
}

static int layers_unsaved_tot()	// Return number of layers with no filenames
{
	int j = 0, k;

	for ( k=0; k<=layers_total; k++ )	// Check each layer for proper filename
	{
		j += !(k == layer_selected ? mem_filename[0] :
			layer_table[k].image->state_.filename[0]);
	}

	return j;
}

static int layers_changed_tot()	// Return number of layers with changes
{
	image_state *state;
	int j, k;

	for (j = k =0; k <= layers_total; k++)	// Check each layer for mem_changed
	{
		state = k == layer_selected ? &mem_state :
			&layer_table[k].image->state_;
		j += state->changed;
		j += !state->filename[0];
	}

	return j;
}

int check_layers_for_changes()			// 1=STOP, 2=IGNORE, 10=ESCAPE, -10=NOT CHECKED
{
	int i = -10, j = 0;
	char *warning = _("One or more of the layers contains changes that have not been saved.  Do you really want to lose these changes?");


	j = j + layers_changed_tot() + layers_changed;
	if ( j>0 )
		i = alert_box( _("Warning"), warning, _("Cancel Operation"), _("Lose Changes"), NULL );

	return i;
}

static void layer_update_filename( char *name )
{
	strncpy(layers_filename, name, PATHBUF);
	layers_changed = 1;		// Forces update of titlebar
	layers_notify_unchanged();
}

static void layers_free_all()
{
	layer_node *t;

	if (layers_total && layer_selected)	// Copy over layer 0
	{
		layer_copy_from_main(layer_selected);
		layer_copy_to_main(0);
		layer_selected = 0;
	}

	for (t = layer_table + layers_total; t != layer_table; t--)
	{
		mem_free_image(&t->image->image_, FREE_ALL);
		free(t->image);
	}
	memset(layer_table + 1, 0, sizeof(layer_node) * MAX_LAYERS);
	layers_total = 0;
	layers_filename[0] = 0;
	layers_changed = 0;
}

void string_chop( char *txt )
{
	char *cp = txt + strlen(txt) - 1;

	// Chop off unwanted non ASCII characters at end
	while ((cp - txt >= 0) && ((unsigned char)*cp < 32)) *cp-- = 0;
}

int read_file_num(FILE *fp, char *txt)
{
	int i;

	if (!fgets(txt, 32, fp)) return -987654321;
	sscanf(txt, "%i", &i);

	return i;
}

int load_layers( char *file_name )
{
	layer_node *t;
	layer_image *lim2;
	char tin[300], load_name[PATHBUF], *c;
	int i, j, k, kk, sens;
	int layers_to_read = -1, layer_file_version = -1, lfail = 0, lplen = 0;
	FILE *fp;

	c = strrchr(file_name, DIR_SEP);
	if (c) lplen = c - file_name + 1;

		// Try to save text file, return -1 if failure
	if ((fp = fopen(file_name, "r")) == NULL) goto fail;

	if (!fgets(tin, 32, fp)) goto fail2;

	string_chop( tin );
	if ( strcmp( tin, LAYERS_HEADER ) != 0 ) goto fail2;		// Bad header

	i = read_file_num(fp, tin);
	if ( i==-987654321 ) goto fail2;
	layer_file_version = i;
	if ( i>LAYERS_VERSION ) goto fail2;		// Version number must be compatible

	i = read_file_num(fp, tin);
	if ( i==-987654321 ) goto fail2;
	layers_to_read = i < MAX_LAYERS ? i : MAX_LAYERS;

	sens = layers_sensitive(FALSE);
	if (layers_total) layers_free_all();	// Remove all current layers if any
	for ( i=0; i<=layers_to_read; i++ )
	{
		// Read filename, strip end chars & try to load (if name length > 0)
		fgets(tin, 256, fp);
		string_chop(tin);
		snprintf(load_name, PATHBUF, "%.*s%s", lplen, file_name, tin);
		k = 1;
		j = detect_image_format(load_name);
		if ((j > 0) && (j != FT_NONE) && (j != FT_LAYERS1))
			k = load_image(load_name, FS_LAYER_LOAD, j) != 1;

		if (k) /* Failure - skip this layer */
		{
			for ( j=0; j<7; j++ ) read_file_num(fp, tin);
			lfail++;
			continue;
		}

		/* Update image variables after load */
		t = layer_table + layers_total;
		lim2 = t->image;
		strncpy(lim2->state_.filename, load_name, PATHBUF);
		init_istate(&lim2->state_, &lim2->image_);

		fgets(tin, 256, fp);
		string_chop(tin);
		strncpy0(t->name, tin, LAYER_NAMELEN);

		k = read_file_num(fp, tin);
		t->visible = k > 0;

		t->x = read_file_num(fp, tin);
		t->y = read_file_num(fp, tin);

		kk = read_file_num(fp, tin);
		k = read_file_num(fp, tin);
		lim2->state_.xpm_trans = kk <= 0 ? -1 : k < 0 ? 0 : k > 255 ? 255 : k;

		k = read_file_num(fp, tin);
		t->opacity = k < 1 ? 1 : k > 100 ? 100 : k;

		layers_total++;
	}
	if (layers_total)
	{
		layer_copy_to_main(--layers_total);
		layer_selected = layers_total;
	}
	layer_refresh_list();
	layer_select_slot(layer_selected);

	/* Read in animation data - only if all layers loaded OK
	 * (to do otherwise is likely to result in SIGSEGV) */
	if (!lfail) ani_read_file(fp);

	fclose(fp);
	layers_sensitive(sens);
	layer_update_filename( file_name );

	if (lfail) /* There were failures */
	{
		snprintf(tin, 300, _("%d layers failed to load"), lfail);
		alert_box( _("Error"), tin, _("OK"), NULL, NULL );
	}

	return 1;		// Success
fail2:
	fclose(fp);
fail:
	return -1;
}

/* Convert absolute filename 'file' into one relative to prefix */
static void parse_filename(char *dest, char *prefix, char *file, int len)
{
	int i, k;

	/* # of chars that match at start */
	for (i = 0; (i < len) && (prefix[i] == file[i]); i++);

	if (!i || (i == len)) /* Complete match, or no match at all */
		strncpy(dest, file + i, PATHBUF);
	else	/* Partial match */
	{
		dest[0] = 0;
		/* Count number of DIR_SEP encountered on and after point i in
		 * 'prefix', add a '../' for each found */
		for (k = i; k < len; k++)
			if (prefix[k] == DIR_SEP) strnncat( dest, "../", PATHBUF);
		/* nip backwards on 'file' from i to previous DIR_SEP or
		 * beginning and ... */
		for (k = i; (k >= 0) && (file[k] != DIR_SEP); k--);
		/* ... add rest of 'file' */
		strnncat(dest, file + k + 1, PATHBUF);
	}
}

int layer_save_composite(char *fname, ls_settings *settings)
{
	image_info *image;
	unsigned char *layer_rgb;
	int w, h, res=0;

	image = layer_selected ? &layer_table[0].image->image_ : &mem_image;
	w = image->width;
	h = image->height;
	layer_rgb = malloc(w * h * 3);
	if (layer_rgb)
	{
		view_render_rgb(layer_rgb, 0, 0, w, h, 1);	// Render layer
		settings->img[CHN_IMAGE] = layer_rgb;
		settings->width = w;
		settings->height = h;
		settings->bpp = 3;
		if (layer_selected) /* Set up background transparency */
		{
			res = layer_table[0].image->state_.xpm_trans;
			settings->xpm_trans = res;
			settings->rgb_trans = res < 0 ? -1 :
				PNG_2_INT(layer_table[0].image->image_.pal[res]);

		}
		res = save_image(fname, settings);
		free( layer_rgb );
	}
	else memory_errors(1);

	return res;
}

void layer_add_composite()
{
	layer_image *lim;
	image_info *image = layer_selected ? &layer_table[0].image->image_ :
		&mem_image;

	if (layers_total >= MAX_LAYERS) return;
	if (layer_add(image->width, image->height, 3, image->cols, image->pal,
		CMASK_IMAGE))
	{
		/* Render to an invisible layer */
		layer_table[layers_total].visible = FALSE;
		lim = layer_table[layers_total].image;
		view_render_rgb(lim->image_.img[CHN_IMAGE], 0, 0,
			image->width, image->height, 1);
		/* Copy background's transparency */
		lim->state_.xpm_trans = layer_selected ?
			layer_table[0].image->state_.xpm_trans : mem_xpm_trans;
		/* Activate the result */
		layer_show_new();
	}
	else memory_errors(1);
}

int save_layers( char *file_name )
{
	layer_node *t;
	char comp_name[PATHBUF], *c, *msg;
	int i, l = 0, xpm;
	FILE *fp;


	layer_copy_from_main(layer_selected);

	c = strrchr(file_name, DIR_SEP);
	if (c) l = c - file_name + 1;

		// Try to save text file, return -1 if failure
	if ((fp = fopen(file_name, "w")) == NULL) goto fail;

	fprintf( fp, "%s\n%i\n%i\n", LAYERS_HEADER, LAYERS_VERSION, layers_total );
	for ( i=0; i<=layers_total; i++ )
	{
		t = layer_table + i;
		parse_filename(comp_name, file_name, t->image->state_.filename, l);
		fprintf( fp, "%s\n", comp_name );

		xpm = t->image->state_.xpm_trans;
		fprintf(fp, "%s\n%i\n%i\n%i\n%i\n%i\n%i\n", t->name,
			t->visible, t->x, t->y, xpm >= 0, xpm, t->opacity);
	}

	ani_write_file(fp);			// Write animation data

	fclose(fp);
	layer_update_filename( file_name );
	register_file( file_name );		// Recently used file list / last directory

	return 1;		// Success
fail:
	c = gtkuncpy(NULL, layers_filename, 0);
	msg = g_strdup_printf(_("Unable to save file: %s"), c);
	alert_box(_("Error"), msg, _("OK"), NULL, NULL);
	g_free(msg);
	g_free(c);

	return -1;
}


int check_layers_all_saved()
{
	if ( layers_unsaved_tot() > 0 )
	{
		alert_box( _("Warning"), _("One or more of the image layers has not been saved.  You must save each image individually before saving the layers text file in order to load this composite image in the future."), _("OK"), NULL, NULL );
		return 1;
	}

	return 0;
}

void layer_press_save()
{
	if (!layers_filename[0]) file_selector( FS_LAYER_SAVE );
	else
	{
		check_layers_all_saved();
		save_layers( layers_filename );
	}
}

void layer_press_remove_all()
{
	int i = check_layers_for_changes();
	if (i < 0) i = alert_box( _("Warning"), _("Do you really want to delete all of the layers?"), _("No"), _("Yes"), NULL );
	if (i != 2) return;

	layers_free_all();

	layer_select_slot(0);
	layer_refresh_list();
	update_main_with_new_layer();
}

static void layer_tog_visible(GtkToggleButton *togglebutton, gpointer user_data)
{
	int j = (int)user_data;

	if (!layers_initialized) return;

	layer_table[j].visible = gtk_toggle_button_get_active(togglebutton);
	layers_notify_changed();
	repaint_layer(j);
}

static void layer_main_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	show_layers_main = gtk_toggle_button_get_active(togglebutton);
	update_stuff(UPD_RENDER);
}

static void layer_inputs_changed(GtkObject *thing, gpointer user_data)
{
	layer_node *t = layer_table + layer_selected;
	const char *nname;
	int dx, dy;

	if (!layers_initialized) return;

	layers_notify_changed();

	switch ((int)user_data)
	{
	case 0: // Name entry
		nname = gtk_entry_get_text(GTK_ENTRY(entry_layer_name));
		strncpy0(t->name, nname, LAYER_NAMELEN);
		gtk_label_set_text(GTK_LABEL(layer_list_data[layer_selected].name), t->name);
		break; // No need to redraw
	case 1: // Position spin
		dx = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(layer_x)) - t->x;
		dy = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(layer_y)) - t->y;
		if (dx | dy) move_layer_relative(layer_selected, dx, dy);
		break;
	case 2: // Opacity slider
		t->opacity = mt_spinslide_get_value(layer_slider);
		repaint_layer(layer_selected);
		break;
	case 3: // Transparency spin
		mem_xpm_trans = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(layer_spin));
		update_stuff(UPD_TRANS);
		break;
	}
}

void layer_choose(int l)	// Select a new layer from the list
{
	if ((l > layers_total) || (l < 0) || (l == layer_selected)) return;
	// Copy image info to layer table before we change
	layer_copy_from_main(layer_selected);
	layer_copy_to_main(layer_selected = l);
	update_main_with_new_layer();
	layer_select_slot(l);
}

static void layer_select(GtkList *list, GtkWidget *widget, gpointer user_data)
{
	layer_node *t;
	int j;

	if (!layers_initialized) return;

	j = (int)gtk_object_get_user_data(GTK_OBJECT(widget));
	if (j > layers_total) return;

	layers_initialized = FALSE;
	if (j != layer_selected) /* Move data before doing anything else */
	{
		layer_copy_from_main(layer_selected);
		layer_copy_to_main(layer_selected = j);
		update_main_with_new_layer();
	}

	t = layer_table + j;
	gtk_entry_set_text(GTK_ENTRY(entry_layer_name), t->name );
	gtk_widget_set_sensitive(layer_tools[LTB_RAISE], j < layers_total);
	gtk_widget_set_sensitive(layer_tools[LTB_LOWER], j);
	gtk_widget_set_sensitive(layer_tools[LTB_DEL], j);
	gtk_widget_set_sensitive(layer_tools[LTB_CENTER], j);
	gtk_widget_set_sensitive(layer_spin, j);
	gtk_widget_set_sensitive(layer_slider, j);
	gtk_widget_set_sensitive(layer_x, j);
	gtk_widget_set_sensitive(layer_y, j);

	mt_spinslide_set_value(layer_slider, j ? t->opacity : 100);
	layer_show_position();
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(layer_spin), j ? mem_xpm_trans : -1);

// !!! May cause list to be stuck in drag mode (release handled before press?)
//	while (gtk_events_pending()) gtk_main_iteration();
	layers_initialized = TRUE;
}

gboolean delete_layers_window()
{
	// No deletion if no window, or inside a sensitive action
	if (!layers_window || !layers_initialized) return (TRUE);
	// Stop user prematurely exiting while drag 'n' drop loading
	if (!GTK_WIDGET_SENSITIVE(layers_window)) return (TRUE);

	layers_initialized = FALSE;
	win_store_pos(layers_window, "layers");

	gtk_widget_destroy(layers_window);
	layers_window = NULL;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		menu_widgets[MENU_LAYER]), FALSE); // Ensure it's unchecked

	return (FALSE);
}

void pressed_paste_layer()
{
	layer_image *lim;
	unsigned char *dest;
	int i, j, k, cmask = CMASK_IMAGE;

	/* No way to put RGB clipboard into utility channel */
	if ((mem_clip_bpp == 3) && (mem_channel != CHN_IMAGE)) return;

	if (layers_total >= MAX_LAYERS)
	{
		alert_box(_("Error"), _("You cannot add any more layers."),
			_("OK"), NULL, NULL);
		return;
	}

	if ((mem_clip_alpha || mem_clip_mask) && !channel_dis[CHN_ALPHA])
		cmask = CMASK_RGBA;
	cmask |= CMASK_FOR(mem_channel);

	if (!layer_add(mem_clip_w, mem_clip_h, mem_clip_bpp, mem_cols, mem_pal,
		cmask)) return; // Failed

	layer_table[layers_total].x = mem_clip_x;
	layer_table[layers_total].y = mem_clip_y;
	lim = layer_table[layers_total].image;

	lim->state_ = mem_state;

	j = mem_clip_w * mem_clip_h;
	memcpy(lim->image_.img[mem_channel], mem_clipboard, j * mem_clip_bpp);

	/* Image channel with alpha */
	dest = lim->image_.img[CHN_ALPHA];
	if (dest && (mem_channel == CHN_IMAGE))
	{
		/* Fill alpha channel */
		if (mem_clip_alpha) memcpy(dest, mem_clip_alpha, j);
		else memset(dest, 255, j);
	}

	/* Image channel with mask */
	if (mem_clip_mask && (mem_channel == CHN_IMAGE))
	{
		/* Mask image - fill unselected part with color A */
		dest = lim->image_.img[CHN_IMAGE];
		k = mem_clip_bpp == 1 ? mem_col_A : mem_col_A24.red;
		for (i = 0; i < j; i++ , dest += mem_clip_bpp)
		{
			if (mem_clip_mask[i]) continue;
			dest[0] = k;
			if (mem_clip_bpp == 1) continue;
			dest[1] = mem_col_A24.green;
			dest[2] = mem_col_A24.blue;
		}
	}

	/* Utility channel with mask */
	dest = lim->image_.img[CHN_ALPHA];
	if (mem_channel != CHN_IMAGE) dest = lim->image_.img[mem_channel];
	if (dest && mem_clip_mask)
	{
		/* Mask the channel */
		for (i = 0; i < j; i++)
		{
			k = dest[i] * mem_clip_mask[i];
			dest[i] = (k + (k >> 8) + 1) >> 8;
		}
	}

	set_new_filename(layers_total, "");

	layer_show_new();
//	pressed_layers();
	view_show();
}

void move_layer_relative(int l, int change_x, int change_y)	// Move a layer & update window labels
{
	image_info *image = l == layer_selected ? &mem_image :
		&layer_table[l].image->image_;
	int lx = layer_table[l].x, ly = layer_table[l].y, lw, lh;

	layer_table[l].x += change_x;
	layer_table[l].y += change_y;
	if (change_x < 0) lx += change_x;
	if (change_y < 0) ly += change_y;
	lw = image->width + abs(change_x);
	lh = image->height + abs(change_y);
	if (layer_selected)
	{
		lx -= layer_table[layer_selected].x;
		ly -= layer_table[layer_selected].y;
	}

	layers_notify_changed();
	if (l == layer_selected) layer_show_position();
	vw_update_area(lx, ly, lw, lh);
	if (show_layers_main) main_update_area(lx, ly, lw, lh);
}

#undef _
#define _(X) X

static toolbar_item layer_bar[] = {
	{ _("New Layer"), -1, LTB_NEW, 0, xpm_new_xpm, ACT_LR_ADD, LR_NEW },
	{ _("Raise"), -1, LTB_RAISE, 0, xpm_up_xpm, ACT_LR_SHIFT, 1 },
	{ _("Lower"), -1, LTB_LOWER, 0, xpm_down_xpm, ACT_LR_SHIFT, -1 },
	{ _("Duplicate Layer"), -1, LTB_DUP, 0, xpm_copy_xpm, ACT_LR_ADD, LR_DUP },
	{ _("Centralise Layer"), -1, LTB_CENTER, 0, xpm_centre_xpm, ACT_LR_CENTER, 0 },
	{ _("Delete Layer"), -1, LTB_DEL, 0, xpm_cut_xpm, ACT_LR_DEL, 0 },
	{ _("Close Layers Window"), -1, LTB_CLOSE, 0, xpm_close_xpm, DLG_LAYERS, 1 },
	{ NULL }};

#undef _
#define _(X) __(X)

/* Create toolbar for layers window */
static GtkWidget *layer_toolbar(GtkWidget **wlist)
{		
	GtkWidget *toolbar;

#if GTK_MAJOR_VERSION == 1
	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
#endif
#if GTK_MAJOR_VERSION == 2
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
#endif
	fill_toolbar(GTK_TOOLBAR(toolbar), layer_bar, wlist, NULL, NULL);
	gtk_widget_show(toolbar);

	return toolbar;
}

void pressed_layers()
{
	GtkWidget *vbox, *hbox, *table, *label, *tog, *scrolledwindow, *item;
	GtkAccelGroup* ag = gtk_accel_group_new();
	char txt[32];
	int i;


	if (layers_window) return; // Already have one
	layers_initialized = FALSE;

	layers_window = add_a_window( GTK_WINDOW_TOPLEVEL, "", GTK_WIN_POS_NONE, FALSE );
	win_restore_pos(layers_window, "layers", 0, 0, 400, 400);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (layers_window), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	scrolledwindow = xpack(vbox, gtk_scrolled_window_new(NULL, NULL));
	gtk_widget_show (scrolledwindow);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	layer_list = gtk_list_new ();
	gtk_signal_connect( GTK_OBJECT(layer_list), "select_child",
			GTK_SIGNAL_FUNC(layer_select), NULL );
	gtk_widget_show (layer_list);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwindow),
		layer_list);
	gtk_container_set_focus_vadjustment(GTK_CONTAINER(layer_list),
		gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolledwindow)));

	for ( i=MAX_LAYERS; i>=0; i-- )
	{
		hbox = gtk_hbox_new(FALSE, 3);

		layer_list_data[i].item = item = gtk_list_item_new();
		gtk_object_set_user_data(GTK_OBJECT(item), (gpointer)i);
		gtk_container_add(GTK_CONTAINER(layer_list), item);
		gtk_container_add(GTK_CONTAINER(item), hbox);

		sprintf(txt, "%i", i);
		label = pack(hbox, gtk_label_new(txt));
		gtk_widget_set_usize (label, 40, -2);
		gtk_misc_set_alignment( GTK_MISC(label), 0.5, 0.5 );

		label = xpack(hbox, gtk_label_new(""));
		gtk_misc_set_alignment( GTK_MISC(label), 0, 0.5 );
		layer_list_data[i].name = label;

		tog = pack(hbox, gtk_check_button_new_with_label(""));
		layer_list_data[i].toggle = tog;
		gtk_widget_show_all(item);
		if (i == 0) gtk_widget_set_sensitive(tog, FALSE);
		else gtk_signal_connect(GTK_OBJECT(tog), "toggled",
			GTK_SIGNAL_FUNC(layer_tog_visible), (gpointer)i);
	}

	for (i = 0; i <= layers_total; i++)
	{
		gtk_label_set_text(GTK_LABEL(layer_list_data[i].name), layer_table[i].name);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(layer_list_data[i].toggle),
			layer_table[i].visible);
	}
	for (; i <= MAX_LAYERS; i++)
	{
		gtk_widget_hide(layer_list_data[i].item);
		gtk_widget_set_sensitive(layer_list_data[i].item, FALSE);
		layer_table[i].image = NULL;	// Needed for checks later
	}
	gtk_list_set_selection_mode(GTK_LIST(layer_list), GTK_SELECTION_BROWSE);

	pack(vbox, layer_toolbar(layer_tools));

	if (layers_total >= MAX_LAYERS) // Hide new/duplicate if we have max layers
	{
		gtk_widget_set_sensitive(layer_tools[LTB_NEW], FALSE);
		gtk_widget_set_sensitive(layer_tools[LTB_DUP], FALSE);
	}

	table = add_a_table( 4, 3, 5, vbox );
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);

	add_to_table( _("Layer Name"), table, 0, 0, 0 );
	add_to_table( _("Position"), table, 1, 0, 0 );
	add_to_table( _("Opacity"), table, 2, 0, 0 );
	add_to_table( _("Transparent Colour"), table, 3, 0, 0 );

	entry_layer_name = gtk_entry_new_with_max_length(LAYER_NAMELEN - 1);
	gtk_widget_set_usize(entry_layer_name, 100, -2);
	gtk_widget_show (entry_layer_name);
	gtk_table_attach (GTK_TABLE (table), entry_layer_name, 1, 3, 0, 1,
		(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry_layer_name), "changed",
		GTK_SIGNAL_FUNC(layer_inputs_changed), (gpointer)0);

	layer_x = spin_to_table(table, 1, 1, 0, 0, -MAX_WIDTH, MAX_WIDTH);
	layer_y = spin_to_table(table, 1, 2, 0, 0, -MAX_HEIGHT, MAX_HEIGHT);
	spin_connect(layer_x, GTK_SIGNAL_FUNC(layer_inputs_changed), (gpointer)1);
	spin_connect(layer_y, GTK_SIGNAL_FUNC(layer_inputs_changed), (gpointer)1);

	layer_slider = mt_spinslide_new(-2, -2);
	mt_spinslide_set_range(layer_slider, 0, 100);
	gtk_table_attach(GTK_TABLE(table), layer_slider, 1, 3, 2, 3,
		GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	mt_spinslide_connect(layer_slider, GTK_SIGNAL_FUNC(layer_inputs_changed),
		(gpointer)2);
	mt_spinslide_set_value(layer_slider, layer_table[layer_selected].opacity);

	layer_spin = spin_to_table(table, 3, 1, 0, 0, -1, 255);
	spin_connect(layer_spin, GTK_SIGNAL_FUNC(layer_inputs_changed), (gpointer)3);

	pack(vbox, sig_toggle(_("Show all layers in main window"),
		show_layers_main, NULL, GTK_SIGNAL_FUNC(layer_main_toggled)));

	gtk_widget_add_accelerator(layer_tools[LTB_CLOSE], "clicked", ag,
		GDK_Escape, 0, (GtkAccelFlags) 0);

	gtk_signal_connect_object (GTK_OBJECT (layers_window), "delete_event",
		GTK_SIGNAL_FUNC (delete_layers_window), NULL);

	layers_update_titlebar();

	/* !!! Select *before* show - otherwise it's nontrivial (try & see) */
	layers_initialized = TRUE;
	layer_select_slot(layer_selected);
	gtk_window_set_transient_for( GTK_WINDOW(layers_window), GTK_WINDOW(main_window) );
	gtk_widget_show(layers_window);
	gtk_window_add_accel_group(GTK_WINDOW (layers_window), ag);
}
