/*
 * Copyright (C) 2022 Nicole Faerber <nicole.faerber@puri.sm>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <adwaita.h>
#include <glib.h>

#define LED_RED_PATH			"/sys/class/leds/red:status"
#define LED_GREEN_PATH			"/sys/class/leds/green:status"
#define LED_BLUE_PATH			"/sys/class/leds/blue:status"
#define LED_AIRPLANE_PATH		"/sys/class/leds/librem_ec:airplane"
#define LED_KBD_BACKLIGHT		"/sys/class/leds/librem_ec:kbd_backlight"

#define BAT_SOC					"/sys/class/power_supply/BAT0/capacity"
#define BAT_START_THRESHOLD_PATH	"/sys/class/power_supply/BAT0/charge_control_start_threshold"
#define BAT_END_THRESHOLD_PATH		"/sys/class/power_supply/BAT0/charge_control_end_threshold"

#define CPU_PL1_PATH			"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/constraint_0_power_limit_uw"
#define CPU_PL2_PATH			"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/constraint_1_power_limit_uw"
// CometLake U, TDP 15W, cTDP-Up 25W
// Intel recommends: PL2 = PL1 * 1.25, would be 18.75W
// some Intel NUC BIOS set this to 30/40 !?

typedef struct {
	GtkWidget *window;
	GtkApplication *gapp;
	gboolean is_root;
	double bat_soc;
	GtkWidget *bat_start_slider;
	double bat_start_thres;
	GtkWidget *bat_end_slider;
	double bat_end_thres;
	GtkWidget *bat_apply_btn;
	GtkWidget *bat_undo_btn;
	double cpu_pl1;
	GtkWidget *cpu_pl1_slider;
	double cpu_pl2;
	GtkWidget *cpu_pl2_slider;
	GtkWidget *cpu_apply_btn;
	GtkWidget *cpu_undo_btn;
	int red_val;
	int green_val;
	int blue_val;
	int kbd_backl;
	bool airplane;
} lcontrol_app_t ;


static int get_value_from_text_file(char *fname)
{
	FILE *fp;
	char buf[64];

	fp=fopen(fname, "r");

	if (fp==NULL) {
		perror(fname);
		return -1;
	}

	memset(buf, 0, 64);

	if (fread(buf, 1, 63, fp) <= 0) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return atoi(buf);
}

static int set_value_to_text_file(char *fname, char *value)
{
	int fd;

	fprintf(stderr, "set_value_to_text_file('%s', '%s')\n", fname, value);
	if (value == NULL)
		return -EINVAL;

	fd = open(fname, O_WRONLY);
	if (fd < 0)
		return fd;
	write(fd, value, strlen(value));
	close(fd);

	return 1;
}

static void update_values_get(lcontrol_app_t *lc_app)
{
	int val;

	val = get_value_from_text_file(BAT_SOC);
	if (val >= 0)
		lc_app->bat_soc = (double)val;
	val = get_value_from_text_file(BAT_START_THRESHOLD_PATH);
	if (val >= 0)
		lc_app->bat_start_thres = (double)val;
	val = get_value_from_text_file(BAT_END_THRESHOLD_PATH);
	if (val >= 0)
		lc_app->bat_end_thres = (double)val;

	val = get_value_from_text_file(CPU_PL1_PATH);
	if (val >= 0)
		lc_app->cpu_pl1 = (double)val / 1000000;
	val = get_value_from_text_file(CPU_PL2_PATH);
	if (val >= 0)
		lc_app->cpu_pl2 = (double)val / 1000000;

	val = get_value_from_text_file(LED_RED_PATH "/brightness");
	if (val >= 0)
		lc_app->red_val = val;
	val = get_value_from_text_file(LED_GREEN_PATH "/brightness");
	if (val >= 0)
		lc_app->green_val = val;
	val = get_value_from_text_file(LED_BLUE_PATH "/brightness");
	if (val >= 0)
		lc_app->blue_val = val;

	val = get_value_from_text_file(LED_KBD_BACKLIGHT "/brightness");
	if (val >= 0)
		lc_app->kbd_backl = val;

	val = get_value_from_text_file(LED_AIRPLANE_PATH "/brightness");
	if (val >= 0)
		lc_app->airplane = (val > 0) ? true : false;
}

static void print_me (GtkWidget *widget, gpointer data)
{
    g_print ("UID=%d\n", getuid());
    g_print ("EUID=%d\n", geteuid());
}

static void bat_start_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
    double bat_start_val;
    double bat_end_val;

    bat_end_val = gtk_range_get_value(GTK_RANGE(lc_app->bat_end_slider));
    bat_start_val = gtk_range_get_value(self);

    if (bat_start_val < 10) {
        bat_start_val = 10;
        gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), bat_start_val);
    }
    if (bat_start_val > 99) {
        bat_start_val = 99;
        gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), bat_start_val);
    }
    bat_start_val+=1;
    if (bat_start_val > bat_end_val) {
        gtk_range_set_value(GTK_RANGE(lc_app->bat_end_slider), bat_start_val);
    }

    if (lc_app->is_root) {
		gtk_widget_set_sensitive(lc_app->bat_apply_btn, true);
		gtk_widget_set_sensitive(lc_app->bat_undo_btn, true);
	}
}

static void bat_end_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
    double bat_start_val;
    double bat_end_val;

	bat_start_val = gtk_range_get_value(GTK_RANGE(lc_app->bat_start_slider));
	bat_end_val = gtk_range_get_value(self);

	bat_end_val-=1;
	if (bat_end_val < bat_start_val) {
		gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), bat_end_val);
    }

    if (lc_app->is_root) {
		gtk_widget_set_sensitive(lc_app->bat_apply_btn, true);
		gtk_widget_set_sensitive(lc_app->bat_undo_btn, true);
	}
}

static void bat_thres_apply_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	char buf[32];

    if (lc_app->is_root) {
		lc_app->bat_end_thres = gtk_range_get_value(GTK_RANGE(lc_app->bat_end_slider));
		lc_app->bat_start_thres = gtk_range_get_value(GTK_RANGE(lc_app->bat_start_slider));

		snprintf(buf, 31, "%d", (int)lc_app->bat_start_thres);
		set_value_to_text_file(BAT_START_THRESHOLD_PATH, buf);

		snprintf(buf, 31, "%d", (int)lc_app->bat_end_thres);
		set_value_to_text_file(BAT_END_THRESHOLD_PATH, buf);
	}

	gtk_widget_set_sensitive(lc_app->bat_apply_btn, false);
	gtk_widget_set_sensitive(lc_app->bat_undo_btn, false);
}

static void bat_thres_undo_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

    if (lc_app->is_root) {
    	gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), lc_app->bat_start_thres);
    	gtk_range_set_value(GTK_RANGE(lc_app->bat_end_slider), lc_app->bat_end_thres);
	}

	gtk_widget_set_sensitive(lc_app->bat_apply_btn, false);
	gtk_widget_set_sensitive(lc_app->bat_undo_btn, false);
}

static void start_charge_now_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	int tval;
	char buf[32];

	// only works if SOC < end threshold
	if (lc_app->bat_end_thres < lc_app->bat_soc) {
		fprintf(stderr, "not starting, end %d, soc %d\n", (int)lc_app->bat_end_thres, (int)lc_app->bat_soc);
		return;
	}
	tval = 	(int)lc_app->bat_end_thres - 1;
	snprintf(buf, 31, "%d", tval);
	set_value_to_text_file(BAT_START_THRESHOLD_PATH, buf);
	g_usleep(G_USEC_PER_SEC);
	snprintf(buf, 31, "%d", (int)lc_app->bat_start_thres);
	set_value_to_text_file(BAT_START_THRESHOLD_PATH, buf);
}

static void stop_charge_now_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	int tval;
	char buf[32];

	// only works if start threshold < SOC
	if (lc_app->bat_start_thres >= lc_app->bat_soc) {
		return;
	}
	tval = 	(int)lc_app->bat_soc + 1;
	snprintf(buf, 31, "%d", tval);
	set_value_to_text_file(BAT_END_THRESHOLD_PATH, buf);
	g_usleep(G_USEC_PER_SEC);
	snprintf(buf, 31, "%d", (int)lc_app->bat_end_thres);
	set_value_to_text_file(BAT_END_THRESHOLD_PATH, buf);

}

static void cpu_pl1_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, true);
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, true);
}

static void cpu_pl2_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, true);
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, true);
}

static void cpu_undo_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

    if (lc_app->is_root) {
    	gtk_range_set_value(GTK_RANGE(lc_app->cpu_pl1_slider), lc_app->cpu_pl1);
    	gtk_range_set_value(GTK_RANGE(lc_app->cpu_pl2_slider), lc_app->cpu_pl2);
	}

	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, false);
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, false);
}

static void cpu_apply_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
}

void kbd_backl_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	char buf[32];

	lc_app->kbd_backl = gtk_range_get_value(self);
	snprintf(buf, 31, "%d", lc_app->kbd_backl);
	set_value_to_text_file(LED_KBD_BACKLIGHT "/brightness", buf);
}

static void close_window (gpointer user_data)
{
	//lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

    // clean up and quit
	//gtk_application_remove_window(lc_app->gapp, GTK_WINDOW(lc_app->window));
}


#define USE_GSTACK 1

#if 0
void create_main_window (lcontrol_app_t *lc_app)
{
    GtkWidget *box;
    GtkWidget *w, *w2;
#ifdef USE_GSTACK
	GtkWidget *stack;
	GtkWidget *stack_sb;
#else
	GtkWidget *leaflet;
	AdwLeafletPage *llpage;
#endif
    gtk_window_set_title (GTK_WINDOW (lc_app->window), "Librem Control");
	gtk_window_set_default_size(GTK_WINDOW(lc_app->window), 400, 300);

    g_signal_connect (lc_app->window, "destroy",
        G_CALLBACK (close_window), lc_app);

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_window_set_child (GTK_WINDOW (lc_app->window), box);

#ifdef USE_GSTACK
	stack_sb=gtk_stack_sidebar_new();
	gtk_box_append(GTK_BOX(box), stack_sb);

	stack=gtk_stack_new();
	gtk_box_append(GTK_BOX(box), stack);
	gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);

	gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(stack_sb), GTK_STACK(stack));
#else
	leaflet = adw_leaflet_new();
	adw_leaflet_set_can_navigate_back(ADW_LEAFLET(leaflet), true);
	adw_leaflet_set_can_navigate_forward(ADW_LEAFLET(leaflet), true);
	adw_leaflet_set_can_unfold(ADW_LEAFLET(leaflet), true);
	gtk_box_append(GTK_BOX(box), leaflet);
#endif
	//
	// Battery page
	//
    box = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(box), 1);

#ifdef USE_GSTACK
	gtk_stack_add_titled(GTK_STACK(stack), box, NULL, "Battery");
#else
	llpage = adw_leaflet_append(ADW_LEAFLET(leaflet), box);
	adw_leaflet_page_set_name(llpage, "Battery");
#endif
    w=gtk_image_new_from_icon_name("battery-low-charging");
    gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
    gtk_grid_attach (GTK_GRID(box), w, 1, 1, 1, 2);

    w=gtk_image_new_from_icon_name("battery-full-charged");
    gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
    gtk_grid_attach (GTK_GRID(box), w, 1, 3, 1, 2);

    w = gtk_label_new("Start charge threshold");
    gtk_grid_attach (GTK_GRID(box), w, 1, 1, 3, 1);

    lc_app->bat_start_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 100., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->bat_start_slider), true);
    gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), lc_app->bat_start_thres);
    g_signal_connect (lc_app->bat_start_slider, "value-changed", G_CALLBACK (bat_start_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->bat_start_slider, true);
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(lc_app->bat_start_slider, false);
    gtk_grid_attach (GTK_GRID(box), lc_app->bat_start_slider, 2, 2, 2, 1);

    w = gtk_label_new("End charge threshold");
    gtk_grid_attach (GTK_GRID(box), w, 1, 3, 3, 1);

    lc_app->bat_end_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 100., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->bat_end_slider), true);
    gtk_range_set_value(GTK_RANGE(lc_app->bat_end_slider), lc_app->bat_end_thres);
    g_signal_connect (lc_app->bat_end_slider, "value-changed", G_CALLBACK (bat_end_val_chg), lc_app);
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(lc_app->bat_end_slider, false);
    gtk_grid_attach (GTK_GRID(box), lc_app->bat_end_slider, 2, 4, 2, 1);

    w = gtk_button_new_with_label("Apply");
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(w, false);
    g_signal_connect (w, "clicked", G_CALLBACK (print_me), NULL);
    gtk_grid_attach (GTK_GRID(box), w, 2, 5, 1, 1);

    w = gtk_button_new_with_label("Charge now!");
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(w, false);
    g_signal_connect (w, "clicked", G_CALLBACK (print_me), NULL);
    gtk_grid_attach (GTK_GRID(box), w, 1, 6, 2, 1);

    w = gtk_button_new_with_label("Stop charge now!");
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(w, false);
    g_signal_connect (w, "clicked", G_CALLBACK (print_me), NULL);
    gtk_grid_attach (GTK_GRID(box), w, 1, 7, 2, 1);

	//
	// CPU page
	//
    box = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(box), 1);
#ifdef USE_GSTACK
	gtk_stack_add_titled(GTK_STACK(stack), box, NULL, "CPU");
#else
	llpage = adw_leaflet_append(ADW_LEAFLET(leaflet), box);
	adw_leaflet_page_set_name(llpage, "CPU");
#endif
    w = gtk_label_new("Long term energy limit (PL1)");
    gtk_grid_attach (GTK_GRID(box), w, 1, 1, 2, 1);
    w = gtk_spin_button_new_with_range(5, 15, .1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), lc_app->cpu_pl1);
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(w, false);
    gtk_grid_attach (GTK_GRID(box), w, 1, 2, 1, 1);
    w = gtk_label_new("W");
    gtk_grid_attach (GTK_GRID(box), w, 2, 2, 1, 1);

    w = gtk_label_new("Short term energy limit (PL2)");
    gtk_grid_attach (GTK_GRID(box), w, 1, 3, 2, 1);
    w = gtk_spin_button_new_with_range(5, 25, .1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), lc_app->cpu_pl2);
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(w, false);
    gtk_grid_attach (GTK_GRID(box), w, 1, 4, 1, 1);
    w = gtk_label_new("W");
    gtk_grid_attach (GTK_GRID(box), w, 2, 4, 1, 1);

    w = gtk_button_new_with_label("Apply");
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(w, false);
    g_signal_connect (w, "clicked", G_CALLBACK (print_me), NULL);
    gtk_grid_attach (GTK_GRID(box), w, 2, 5, 1, 1);


	//
	// LEDs page
	//
    box = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(box), 1);

#ifdef USE_GSTACK
	gtk_stack_add_titled(GTK_STACK(stack), box, NULL, "LEDs");
#else
	llpage = adw_leaflet_append(ADW_LEAFLET(leaflet), box);
	adw_leaflet_page_set_name(llpage, "LEDs");
#endif
    w = gtk_label_new("Notification");
    gtk_grid_attach (GTK_GRID(box), w, 1, 1, 2, 1);

	w = gtk_color_button_new();
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(w), false);
    gtk_grid_attach (GTK_GRID(box), w, 2, 2, 1, 1);

    w = gtk_label_new("Keyboard backlight");
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(w, false);
    gtk_grid_attach (GTK_GRID(box), w, 1, 3, 2, 1);

	w = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 255., 1);
    gtk_scale_set_draw_value (GTK_SCALE(w), true);
    gtk_widget_set_hexpand(w, true);
    gtk_range_set_value(GTK_RANGE(w), lc_app->kbd_backl);
    g_signal_connect (w, "value-changed", G_CALLBACK (kbd_backl_val_chg), lc_app);
    if (!lc_app->is_root)
        gtk_widget_set_sensitive(w, false);
    gtk_grid_attach (GTK_GRID(box), w, 2, 4, 2, 1);

    w = gtk_label_new("WiFi/BT");
    gtk_grid_attach (GTK_GRID(box), w, 1, 5, 2, 1);

    w = gtk_label_new("Trigger:");
    gtk_grid_attach (GTK_GRID(box), w, 2, 6, 1, 1);
	const char *rfk_labels[] = {"Rfkill","WiFi TX","WiFi RX", NULL};
	w=gtk_drop_down_new_from_strings(rfk_labels);
    gtk_grid_attach (GTK_GRID(box), w, 3, 6, 1, 1);
#if 0
	w=gtk_check_button_new_with_label("Rfkill");
    gtk_grid_attach (GTK_GRID(box), w, 2, 6, 1, 1);
	w2=gtk_check_button_new_with_label("WiFi tx activity");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(w),GTK_CHECK_BUTTON(w2));
    gtk_grid_attach (GTK_GRID(box), w2, 2, 7, 1, 1);
	w2=gtk_check_button_new_with_label("WiFi rx activity");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(w),GTK_CHECK_BUTTON(w2));
    gtk_grid_attach (GTK_GRID(box), w2, 2, 8, 1, 1);
#endif
#if 0
    w = gtk_radio_button_new_with_label("rfkill");
    w2 = gtk_radio_button_new_with_label("WiFi activity");
    gtk_radio_button_join_group(GTK_RADIO_BUTTON(w));

    gtk_grid_attach (GTK_GRID(box), w, 2, 8, 1, 1);
    gtk_grid_attach (GTK_GRID(box), w2, 2, 9, 1, 1);
#endif
}
#else
void create_main_window (lcontrol_app_t *lc_app)
{
    GtkWidget *box;
    GtkWidget *w, *w2, *c;
	GtkWidget *stack;
	GtkWidget *stack_sb;

    gtk_window_set_title (GTK_WINDOW (lc_app->window), "Librem Control");
	gtk_window_set_default_size(GTK_WINDOW(lc_app->window), 400, 300);

    g_signal_connect (lc_app->window, "destroy",
        G_CALLBACK (close_window), lc_app);

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_window_set_child (GTK_WINDOW (lc_app->window), box);

	stack_sb=gtk_stack_sidebar_new();
	gtk_box_append(GTK_BOX(box), stack_sb);

	stack=gtk_stack_new();
	gtk_box_append(GTK_BOX(box), stack);
	gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
	gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(stack_sb), GTK_STACK(stack));

	//
	// Battery page
	//
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_stack_add_titled(GTK_STACK(stack), box, "Battery", "Battery");

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_label_new("SOC");
	gtk_widget_set_vexpand(w, false);
	gtk_widget_set_hexpand(w, false);
	gtk_box_append(GTK_BOX(c), w);
	w = gtk_progress_bar_new();
	gtk_widget_set_hexpand(w, true);
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(w), true);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w), lc_app->bat_soc / 100.);
	gtk_box_append(GTK_BOX(c), w);

	w = gtk_frame_new("Start Charge Threshold");
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_image_new_from_icon_name("battery-low-charging");
    gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
	gtk_box_append(GTK_BOX(c), w);
    lc_app->bat_start_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 100., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->bat_start_slider), true);
    gtk_scale_set_value_pos(GTK_SCALE(lc_app->bat_start_slider), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), lc_app->bat_start_thres);
    g_signal_connect (lc_app->bat_start_slider, "value-changed", G_CALLBACK (bat_start_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->bat_start_slider, true);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(lc_app->bat_start_slider, false);
	}
	gtk_box_append(GTK_BOX(c), lc_app->bat_start_slider);
	w = gtk_label_new("%");
	gtk_box_append(GTK_BOX(c), w);

	w = gtk_frame_new("End Charge Threshold");
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_image_new_from_icon_name("battery-full-charged");
    gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
	gtk_box_append(GTK_BOX(c), w);
    lc_app->bat_end_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 100., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->bat_end_slider), true);
    gtk_scale_set_value_pos(GTK_SCALE(lc_app->bat_end_slider), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(lc_app->bat_end_slider), lc_app->bat_end_thres);
    g_signal_connect (lc_app->bat_end_slider, "value-changed", G_CALLBACK (bat_end_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->bat_end_slider, true);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(lc_app->bat_end_slider, false);
	}
	gtk_box_append(GTK_BOX(c), lc_app->bat_end_slider);
	w = gtk_label_new("%");
	gtk_box_append(GTK_BOX(c), w);

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_label_new("");
    gtk_widget_set_hexpand(w, true);
	gtk_box_append(GTK_BOX(c), w);
	lc_app->bat_undo_btn = gtk_button_new_from_icon_name("edit-undo-symbolic");
	gtk_widget_set_sensitive(lc_app->bat_undo_btn, false);
    g_signal_connect (lc_app->bat_undo_btn, "clicked", G_CALLBACK (bat_thres_undo_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), lc_app->bat_undo_btn);
	lc_app->bat_apply_btn = gtk_button_new_from_icon_name("emblem-ok-symbolic");
	gtk_widget_set_sensitive(lc_app->bat_apply_btn, false);
    g_signal_connect (lc_app->bat_apply_btn, "clicked", G_CALLBACK (bat_thres_apply_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), lc_app->bat_apply_btn);

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_button_new_with_label("Start charge now!");
	gtk_widget_set_tooltip_text(w, "Will start charging immediately,\nup to End Charge Threshold");
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(w, false);
	}
    g_signal_connect (w, "clicked", G_CALLBACK (start_charge_now_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), w);
	w = gtk_label_new("");
    gtk_widget_set_hexpand(w, true);
	gtk_box_append(GTK_BOX(c), w);

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_button_new_with_label("Stop charge now!");
	gtk_widget_set_tooltip_text(w, "Will stop charging immediately.");
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(w, false);
	}
    g_signal_connect (w, "clicked", G_CALLBACK (stop_charge_now_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), w);
	w = gtk_label_new("");
    gtk_widget_set_hexpand(w, true);
	gtk_box_append(GTK_BOX(c), w);

	//
	// CPU page
	//
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_stack_add_titled(GTK_STACK(stack), box, "CPU", "CPU");

	w = gtk_frame_new("Long Term");
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_label_new("PL1");
    // gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
	gtk_box_append(GTK_BOX(c), w);
    lc_app->cpu_pl1_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 20., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->cpu_pl1_slider), true);
    gtk_scale_set_value_pos(GTK_SCALE(lc_app->cpu_pl1_slider), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(lc_app->cpu_pl1_slider), lc_app->cpu_pl1);
    g_signal_connect (lc_app->cpu_pl1_slider, "value-changed", G_CALLBACK (cpu_pl1_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->cpu_pl1_slider, true);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(lc_app->cpu_pl1_slider, false);
	}
	gtk_box_append(GTK_BOX(c), lc_app->cpu_pl1_slider);
	w = gtk_label_new("W");
	gtk_box_append(GTK_BOX(c), w);

	w = gtk_frame_new("Short Term");
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_label_new("PL2");
    // gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
	gtk_box_append(GTK_BOX(c), w);
    lc_app->cpu_pl2_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 35., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->cpu_pl2_slider), true);
    gtk_scale_set_value_pos(GTK_SCALE(lc_app->cpu_pl2_slider), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(lc_app->cpu_pl2_slider), lc_app->cpu_pl2);
    g_signal_connect (lc_app->cpu_pl2_slider, "value-changed", G_CALLBACK (cpu_pl2_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->cpu_pl2_slider, true);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(lc_app->cpu_pl2_slider, false);
	}
	gtk_box_append(GTK_BOX(c), lc_app->cpu_pl2_slider);
	w = gtk_label_new("W");
	gtk_box_append(GTK_BOX(c), w);

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_label_new("");
    gtk_widget_set_hexpand(w, true);
	gtk_box_append(GTK_BOX(c), w);
	lc_app->cpu_undo_btn = gtk_button_new_from_icon_name("edit-undo-symbolic");
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, false);
    g_signal_connect (lc_app->cpu_undo_btn, "clicked", G_CALLBACK (cpu_undo_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), lc_app->cpu_undo_btn);
	lc_app->cpu_apply_btn = gtk_button_new_from_icon_name("emblem-ok-symbolic");
	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, false);
    g_signal_connect (lc_app->cpu_apply_btn, "clicked", G_CALLBACK (cpu_apply_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), lc_app->cpu_apply_btn);

}
#endif

void gtest_app_activate (GApplication *application, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

    if (getuid() == 0 || geteuid() == 0) {
        lc_app->is_root=true;
	}

	lc_app->window = gtk_application_window_new (GTK_APPLICATION (application));
    create_main_window(lc_app);
    gtk_window_present (GTK_WINDOW(lc_app->window));
}

int main (int argc, char **argv)
{
static lcontrol_app_t lcontrol_app;

	lcontrol_app.is_root = false;
	lcontrol_app.cpu_pl1 = 15.0;
	lcontrol_app.cpu_pl1 = 20.0;
	lcontrol_app.bat_soc = 0.;
	lcontrol_app.bat_start_thres = 90;
	lcontrol_app.bat_end_thres = 100;

	update_values_get(&lcontrol_app);

    lcontrol_app.gapp=gtk_application_new("com.purism.librem-control", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(lcontrol_app.gapp, "activate", G_CALLBACK (gtest_app_activate), &lcontrol_app);
    g_application_run (G_APPLICATION (lcontrol_app.gapp), argc, argv);
    g_object_unref (lcontrol_app.gapp);

return 0;
}
