/*
 * Author: Randy Li <randy.li@rock-chips.com>
 *
 */

#include <glib-unix.h>
#include <glib/gstdio.h>
#include <gst/gst.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>

#include "control.h"

#define MAX_INSTANCE_NUM (4)

GST_DEBUG_CATEGORY (rk_audioservice_debug);

static struct controller *ctrl = NULL;

inline static const char *
yesno (int yes)
{
  return yes ? "yes" : "no";
}

struct decoder
{
  GstElement *pipeline;
  GstElement *src;

  gint format;
  guint index;

  GMappedFile *file;
  gsize length;
};

static gboolean
bus_watch_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  struct decoder *dec = (struct decoder *) user_data;

  (void) bus;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STATE_CHANGED:{
      GstState old_gst_state, cur_gst_state, pending_gst_state;

      /* Only consider state change messages coming from
       * the toplevel element. */
      if (GST_MESSAGE_SRC (msg) != GST_OBJECT (dec->pipeline))
        break;

      gst_message_parse_state_changed (msg, &old_gst_state, &cur_gst_state,
          &pending_gst_state);

      /* TODO: inform the control thread */
      if (cur_gst_state == GST_STATE_PLAYING) {
        gchar *ctrl_msg = NULL;
	ctrl_msg = g_strdup_printf ("%d: PLaying", dec->index);
	rk_unix_ctrl_push_data (ctrl, ctrl_msg);
      }

      break;
    }
    case GST_MESSAGE_REQUEST_STATE:{
      GstState requested_state;
      gst_message_parse_request_state (msg, &requested_state);
      GST_LOG ("state change to %s was requested by %s\n",
          gst_element_state_get_name (requested_state),
          GST_MESSAGE_SRC_NAME (msg)
          );
      gst_element_set_state (GST_ELEMENT (dec->pipeline), requested_state);
      break;
    }
    case GST_MESSAGE_LATENCY:{
      GST_LOG ("redistributing latency\n");
      gst_bin_recalculate_latency (GST_BIN (dec->pipeline));
      break;
    }
    case GST_MESSAGE_EOS: {
      gchar *ctrl_msg = NULL;

      ctrl_msg = g_strdup_printf ("%d: EOS", dec->index);
      rk_unix_ctrl_push_data (ctrl, ctrl_msg);
      break;
    }

    case GST_MESSAGE_INFO:
    case GST_MESSAGE_WARNING:
    case GST_MESSAGE_ERROR:{
      GError *error = NULL;
      gchar *debug_info = NULL;
      gchar const *prefix;

      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_INFO:
          gst_message_parse_info (msg, &error, &debug_info);
          prefix = "INFO";
          break;
        case GST_MESSAGE_WARNING:
          gst_message_parse_warning (msg, &error, &debug_info);
          prefix = "WARNING";
          break;
        case GST_MESSAGE_ERROR:
          gst_message_parse_error (msg, &error, &debug_info);
          prefix = "ERROR";
          break;
        default:
          g_assert_not_reached ();
      }
      GST_LOG ("GStreamer %s: %s; debug info: %s", prefix, error->message,
          debug_info);

      g_clear_error (&error);
      g_free (debug_info);

      if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (dec->pipeline),
            GST_DEBUG_GRAPH_SHOW_ALL, "error");
      }
      // TODO: stop mainloop in case of an error
      gst_element_set_state(GST_ELEMENT(dec->pipeline), GST_STATE_NULL);

      break;
    }
    default:
      break;
  }

  return TRUE;
}

static struct decoder *create_audio_inst()
{
  struct decoder *dec;
  GstBus *bus;
  const char *pipeline = NULL;

  dec = g_new0 (struct decoder, 1);
  if (!dec)
	  return NULL;

  /* Setup pipeline: */
  pipeline = "playbin video-sink=fakesink";
  dec->pipeline = gst_parse_launch (pipeline, NULL);
  if (!dec->pipeline) {
	  GST_ERROR ("Gstreamer is not installed properly\n");
	  g_free(dec);
	  return NULL;
  }

  /* add bus to be able to receive error message, handle latency
   * requests, produce pipeline dumps, etc. */
  bus = gst_pipeline_get_bus (GST_PIPELINE (dec->pipeline));
  gst_bus_add_watch (bus, bus_watch_cb, dec);
  gst_object_unref (GST_OBJECT (bus));

  gst_element_set_state (dec->pipeline, GST_STATE_NULL);

  return dec;
}

void
rk_media_inst_play_url (gpointer data, char *uri)
{
  struct decoder *dec = data;

  if (!data || !uri)
    return;

  gst_element_set_state (dec->pipeline, GST_STATE_NULL);

  g_object_set (G_OBJECT (dec->pipeline), "uri", uri, NULL);
  gst_element_set_state (dec->pipeline, GST_STATE_PLAYING);
}

static void
audio_inst_clean (struct decoder *dec)
{
  if (!dec->pipeline)
	  return;

  gst_element_set_state (dec->pipeline, GST_STATE_NULL);

  gst_object_unref (dec->pipeline);
  g_free (dec);
}

gboolean termination_sig_handler(gpointer user_data)
{
  GMainLoop *loop = user_data;

  g_main_loop_quit (loop);

  return FALSE;
}

static void
daemonize ()
{
  pid_t pid, sid;

  pid = fork ();

  if (pid != 0) {
    if (-1 == pid)
      exit (EXIT_FAILURE);
    else
      exit (EXIT_SUCCESS);
  }

  g_message ("Rockchip Simple Audio Service starting\n");

  if ((sid = setsid ()) < 0)
    exit (EXIT_FAILURE);

  if ((chdir ("/")) < 0)
    exit (EXIT_FAILURE);

  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);
}

gint
main (gint argc, gchar * argv[])
{
  struct decoder *decs[MAX_INSTANCE_NUM] = { NULL, };
  GMainLoop *loop = NULL;
  GOptionContext *context;

  gboolean foreground = FALSE;
  gchar *address = NULL;
  gchar *unix_socket_path = NULL;
  guint port = 0;

  GOptionEntry entries[] = {
    {"foreground", 'f', 0, G_OPTION_ARG_NONE, &foreground,
        "Run this application at foreground", NULL}
    ,
    {"address", 's', 0, G_OPTION_ARG_STRING, &address,
        "Server listen address", NULL}
    ,
    {"port", 'p', 0, G_OPTION_ARG_INT, &port,
        "Server listen port", NULL}
    ,
    {"socket", 'c', 0, G_OPTION_ARG_FILENAME, &unix_socket_path,
        "the path for creating a Unix control socket", NULL}
    ,
    {NULL}
    ,
  };
  GError *error = NULL;

  GST_DEBUG_CATEGORY_INIT (rk_audioservice_debug, "rkaudio", 0,
		  	   "Rockchip Audio Simple service");
  gst_init (&argc, &argv);

  context = g_option_context_new ("Rockchip Audio Simple service");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("option parsing failed: %s\n", error->message);
    return 1;
  }

  if (!unix_socket_path)
    unix_socket_path = RK_DEAFAULT_UNIX_SOCKET;

  if (!foreground)
    daemonize ();

  for (guint i = 0; i < MAX_INSTANCE_NUM; i++) {
    decs[i] = create_audio_inst();
    if (!decs[i])
      goto dec_init_error;

    decs[i]->index = i;
  }

  ctrl = rk_new_unix_ctrl(unix_socket_path, decs, MAX_INSTANCE_NUM);
  if (!ctrl)
    goto dec_init_error;
  
  loop = g_main_loop_new (NULL, FALSE);
  g_unix_signal_add (SIGINT, termination_sig_handler, loop);
  g_unix_signal_add (SIGTERM, termination_sig_handler, loop);

  g_main_loop_run (loop);

  for (gint i = MAX_INSTANCE_NUM - 1; i >= 0; i--)
    audio_inst_clean (decs[i]);

  rk_destroy_unix_ctrl (ctrl);

  return 0;

dec_init_error:
  for (gint i = MAX_INSTANCE_NUM - 1; i >= 0; i--)
    audio_inst_clean (decs[i]);

  return 1;
}
