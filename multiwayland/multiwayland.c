/*
 * Copyright (C) 2018 Rockchip
 *   @author Randy Li <randy.li@rock-chips.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#else
#error "Wayland is not supported in GTK+"
#endif

#include <gst/video/videooverlay.h>
#include "wayland.h"

typedef struct _PlayerData
{
  GtkWidget *video_widget;

  GstElement *pipeline;
  GstVideoOverlay *overlay;

} PlayerData;

typedef struct
{
  GResource *resource;
  GtkWidget *app_widget;
  GtkGrid *grid;
  GList *players;

  gint grid_left;
  gint grid_top;
  gint current_uri;             /* index for argv */
  GList *playlist;
} DemoApp;

static void
on_about_to_finish (GstElement * playbin, gchar * uri)
{

  g_print ("Now playing %s\n", uri);
  g_object_set (playbin, "uri", uri, NULL);
}

static void
error_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  PlayerData *d = user_data;
  gchar *debug = NULL;
  GError *err = NULL;

  gst_message_parse_error (msg, &err, &debug);

  g_print ("Error: %s\n", err->message);
  g_error_free (err);

  if (debug) {
    g_print ("Debug details: %s\n", debug);
    g_free (debug);
  }

  gst_element_set_state (d->pipeline, GST_STATE_NULL);
}

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{
  PlayerData *d = user_data;

  if (gst_is_wayland_display_handle_need_context_message (message)) {
    GstContext *context;
    GdkDisplay *display;
    struct wl_display *display_handle;

    display = gtk_widget_get_display (d->video_widget);
    display_handle = gdk_wayland_display_get_wl_display (display);
    context = gst_wayland_display_handle_context_new (display_handle);
    gst_element_set_context (GST_ELEMENT (GST_MESSAGE_SRC (message)), context);

    goto drop;
  } else if (gst_is_video_overlay_prepare_window_handle_message (message)) {
    GtkAllocation allocation;
    GdkWindow *window;
    struct wl_surface *window_handle;

    /* GST_MESSAGE_SRC (message) will be the overlay object that we have to
     * use. This may be waylandsink, but it may also be playbin. In the latter
     * case, we must make sure to use playbin instead of waylandsink, because
     * playbin resets the window handle and render_rectangle after restarting
     * playback and the actual window size is lost */
    d->overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));

    gtk_widget_get_allocation (d->video_widget, &allocation);
    window = gtk_widget_get_window (d->video_widget);
    window_handle = gdk_wayland_window_get_wl_surface (window);

    g_print ("setting window handle and size (%d x %d)\n",
        allocation.width, allocation.height);

    gst_video_overlay_set_window_handle (d->overlay, (guintptr) window_handle);
    gst_video_overlay_set_render_rectangle (d->overlay, allocation.x,
        allocation.y, allocation.width, allocation.height);

    goto drop;
  }

  return GST_BUS_PASS;

drop:
  gst_message_unref (message);
  return GST_BUS_DROP;
}

/* We use the "draw" callback to change the size of the sink
 * because the "configure-event" is only sent to top-level widgets. */
static gboolean
video_widget_draw_cb (GtkWidget * widget, cairo_t * cr, gpointer user_data)
{
  PlayerData *d = user_data;
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  g_print ("draw_cb x %d, y %d, w %d, h %d\n",
      allocation.x, allocation.y, allocation.width, allocation.height);

  if (d->overlay) {
    gst_video_overlay_set_render_rectangle (d->overlay, allocation.x,
        allocation.y, allocation.width, allocation.height);
  }

  /* There is no need to call gst_video_overlay_expose().
   * The wayland compositor can always re-draw the window
   * based on its last contents if necessary */

  return FALSE;
}

static void
playing_clicked_cb (GtkButton * button, PlayerData * d)
{
  gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
}

static void
paused_clicked_cb (GtkButton * button, PlayerData * d)
{
  gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
}

static void
ready_clicked_cb (GtkButton * button, PlayerData * d)
{
  gst_element_set_state (d->pipeline, GST_STATE_READY);
}

static void
null_clicked_cb (GtkButton * button, PlayerData * d)
{
  gst_element_set_state (d->pipeline, GST_STATE_NULL);
}

static PlayerData *
create_new_player (gchar * uri)
{
  PlayerData *d = NULL;
  GError *error = NULL;
  GstBus *bus;

  d = g_slice_new0 (PlayerData);

  d->pipeline = gst_parse_launch ("playbin video-sink=waylandsink", NULL);
  g_object_set (d->pipeline, "uri", uri, NULL);

  /* enable looping */
  g_signal_connect (d->pipeline, "about-to-finish",
      G_CALLBACK (on_about_to_finish), uri);

  d->video_widget = gtk_event_box_new ();
  gtk_widget_set_hexpand (d->video_widget, TRUE);
  gtk_widget_set_vexpand (d->video_widget, TRUE);

  g_signal_connect (d->video_widget, "draw",
      G_CALLBACK (video_widget_draw_cb), d);

#if 0
  button = GTK_WIDGET (gtk_builder_get_object (builder, "button_playing"));
  g_signal_connect (button, "clicked", G_CALLBACK (playing_clicked_cb), d);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "button_paused"));
  g_signal_connect (button, "clicked", G_CALLBACK (paused_clicked_cb), d);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "button_ready"));
  g_signal_connect (button, "clicked", G_CALLBACK (ready_clicked_cb), d);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "button_null"));
  g_signal_connect (button, "clicked", G_CALLBACK (null_clicked_cb), d);
#endif
  bus = gst_pipeline_get_bus (GST_PIPELINE (d->pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (error_cb), d);
  gst_bus_set_sync_handler (bus, bus_sync_handler, d, NULL);
  gst_object_unref (bus);

  return d;
}

void
add_a_player_to_app (gpointer uri, gpointer app)
{
  DemoApp *d = (DemoApp *) app;
  PlayerData *pd = NULL;

  pd = create_new_player (uri);
  if (pd) {
    gtk_grid_attach (d->grid, pd->video_widget, d->grid_left, d->grid_top,
        1, 1);
    gst_element_set_state (pd->pipeline, GST_STATE_PLAYING);
    d->players = g_list_prepend (d->players, pd);
    d->grid_top += 2;
  }
}

static void
build_window (DemoApp * d)
{
  GtkBuilder *builder;
  GtkWidget *button;
  GError *error = NULL;

  builder =
      gtk_builder_new_from_resource ("/com/rock-chips/multishow/window.ui");

  gtk_window_fullscreen (GTK_WINDOW (gtk_builder_get_object (builder,
              "window")));
  d->app_widget = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  g_object_ref (d->app_widget);
  g_signal_connect (d->app_widget, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  d->grid = GTK_GRID (gtk_builder_get_object (builder, "grid0"));
  d->grid_left = 0;
  d->grid_top = 0;

  g_list_foreach (d->playlist, add_a_player_to_app, d);

  gtk_widget_show_all (d->app_widget);

exit:
  g_object_unref (builder);
}

static void
remove_a_player (gpointer data, gpointer user_data)
{
  PlayerData *d = (PlayerData *) data;
  GstBus *bus;

  gst_element_set_state (d->pipeline, GST_STATE_NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (d->pipeline));
  gst_bus_remove_watch (bus);
  gst_object_unref (bus);

  gst_object_unref (d->pipeline);
  g_object_unref (d->video_widget);
}

static void
clean_up (DemoApp * app)
{
  g_list_foreach (app->players, remove_a_player, app);

  g_list_free (app->players);
  g_list_free (app->playlist);

  g_object_unref (app->app_widget);
}

static gboolean
parse_video_path (DemoApp * data, gchar * path)
{
  if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
    GError *err = NULL;
    const gchar *file = NULL;
    gchar *uri = NULL;
    GDir *dir = g_dir_open (path, 0, &err);
    GList *first = NULL, *last = NULL;
    if (err) {
      g_print ("Error open dir %s: %s\n", path, err->message);
      return FALSE;
    }
    do {
      file = g_dir_read_name (dir);
      uri = g_strdup_printf ("%s/%s", path, file);
      if (g_file_test (uri, G_FILE_TEST_EXISTS)) {
        g_free (uri);
        uri = g_strdup_printf ("file://%s/%s", path, file);
        data->playlist = g_list_prepend (data->playlist, uri);
      } else if (errno) {
        g_free (uri);
        g_print ("Error list file in directory %d\n", errno);
        break;
      }
    } while (file);
    last = data->playlist;
    data->playlist = g_list_reverse (data->playlist);
    /* NOTE: After it is connected, many functions won't work */
    first = data->playlist;
    first->prev = last;

    g_dir_close (dir);
    return TRUE;
  } else if (g_file_test (path, G_FILE_TEST_EXISTS)) {
    gchar *file = NULL;

    file = g_strdup_printf ("file://%s", path);
    data->playlist = g_list_prepend (data->playlist, file);
    data->playlist = g_list_prepend (data->playlist, NULL);
    data->playlist = g_list_reverse (data->playlist);
    data->playlist = g_list_last (data->playlist);

    return TRUE;
  }
  return FALSE;
}


int
main (int argc, char **argv)
{
  DemoApp *d;
  GOptionContext *context;
  GError *error = NULL;
  gchar *uri = NULL;
  gchar *path = NULL;
  gchar *bundle_file = NULL;
  GResource *res = NULL;

  GOptionEntry entries[] = {
    {"uri", '\0', 0, G_OPTION_ARG_STRING, &uri, "video source URI", NULL},
    {"path", '\0', 0, G_OPTION_ARG_STRING, &path,
        "the path of a video file or directory", NULL},
    {NULL}
  };


  gtk_init (&argc, &argv);
  gst_init (&argc, &argv);

  context = g_option_context_new ("- Multiple video");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("option parsing failed: %s\n", error->message);
    return 1;
  }
  g_option_context_free (context);

  if (!uri && !path) {
    g_printerr ("Usage: %s <pipeline spec>\n", argv[0]);
    return 1;
  }


  bundle_file = g_build_filename (DATADIR, "multishow.gresource", NULL);
  res = g_resource_load (bundle_file, &error);
  if (!res) {
    g_error ("Failed to load resource bundle: %s", error->message);
    g_error_free (error);
    goto exit;
  }
  g_clear_pointer (&bundle_file, g_free);
  g_resources_register (res);

  d = g_slice_new0 (DemoApp);

  if (uri == NULL) {
    if (!parse_video_path (d, path))
      return 1;
  }

  build_window (d);

  gtk_main ();

  clean_up (d);

  g_slice_free (DemoApp, d);
  g_resources_unregister (res);
  g_resource_unref (res);

exit:
  g_free (bundle_file);
  g_free (path);
  g_free (uri);
  return 0;
}
