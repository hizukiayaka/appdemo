/*
 * Author: Randy Li <randy@soulik.info>
 */

#include <gst/gst.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gio/gunixsocketaddress.h>

struct controller
{
  GSocket *socket;
  GThread *ctrl_thread;
  GMutex mutex;

  gpointer *decs;
  guint num_dec;

  GAsyncQueue *ctrl_queue;
};

gboolean
tesagure_set_uri(gpointer data)
{
  rk_media_inst_play_url(data,
		         "file:///home/randy/Music/てさぐり部部歌.mp3");

  return FALSE;
}

gboolean
takarasagashi_set_uri(gpointer data)
{
  rk_media_inst_play_url(data,
		         "file:///home/randy/Videos/takarasagashi_Vol3_SP.mp4");

  return FALSE;
}

gpointer rk_unix_ctrl_handler(gpointer data)
{
  struct controller *ctrl = data;
  gpointer msg = NULL;

#if 1
  g_idle_add(tesagure_set_uri, ctrl->decs[1]);
  g_timeout_add(1000 * 3, takarasagashi_set_uri, ctrl->decs[1]);
  g_timeout_add(1000 * 7, tesagure_set_uri, ctrl->decs[3]);

  do {
    msg = g_async_queue_pop (ctrl->ctrl_queue);
    if (!g_strcmp0(msg, "END"))
      g_thread_exit (0);

    g_print("%s\n", msg);
    g_free (msg);
  } while (1);

  return NULL;
#else
#endif
}

void
rk_unix_ctrl_push_data(struct controller *ctrl, gpointer data)
{
  g_async_queue_push (ctrl->ctrl_queue, data);
}

struct controller *
rk_new_unix_ctrl (const char *path, gpointer decs, guint num_dec)
{
  struct controller *ctrl = NULL;
  GSocket *socket = NULL;
  GSocketAddress *addr = NULL;
  GError *error;

  addr = g_unix_socket_address_new (path);
  if (!addr) {
    GST_ERROR ("invalid path for socket\n");
    return NULL;
  }

  ctrl = g_new0 (struct controller, 1);
  if (!ctrl)
    goto failed;

  socket = g_socket_new (G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_DATAGRAM, 0,
                         &error);
  if (!socket) {
    GST_DEBUG ("can't create socket %s", error->message);
    goto failed;
  }

  if (g_unlink(g_unix_socket_address_get_path(G_UNIX_SOCKET_ADDRESS (addr)))) {
    GST_DEBUG ("can't create socket %s", error->message);
  }

  if (!g_socket_bind (socket, addr, FALSE, &error)) {
    GST_ERROR ("can't bind address %s", error->message);
    goto net_failed;
  }
  g_object_unref (addr);

  ctrl->ctrl_queue = g_async_queue_new ();

  ctrl->socket = socket;
  ctrl->decs = decs;
  ctrl->num_dec = num_dec;
  g_mutex_init (&ctrl->mutex);
  ctrl->ctrl_thread = g_thread_new ("audio control",
		                    rk_unix_ctrl_handler, ctrl);

  return ctrl;

net_failed:
  g_object_unref (socket);
failed:
  g_object_unref (addr);
  g_error_free (error);
  g_free (ctrl);
  return NULL;
}

void
rk_destroy_unix_ctrl (struct controller *ctrl)
{
  rk_unix_ctrl_push_data (ctrl, "END");

  g_object_unref (ctrl->socket);
  g_thread_unref (ctrl->ctrl_thread);
  g_async_queue_unref(ctrl->ctrl_queue);
  g_free (ctrl);
}
