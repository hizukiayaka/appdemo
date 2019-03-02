/*
 * Author: Randy Li <randy.li@rock-chips.com>
 */
#ifndef _CONTROL_H_
#define _CONTROL_H_

GST_DEBUG_CATEGORY_EXTERN (rk_audioservice_debug);

#define RK_DEAFAULT_UNIX_SOCKET  "/tmp/audioservice.sock"
//#define RK_DEAFAULT_UNIX_SOCKET  "/var/run/audioservice.sock"

#include <gst/gst.h>

struct controller;

struct controller *
rk_new_unix_ctrl (const char *path, gpointer decs, guint num_dec);
void rk_destroy_unix_ctrl (struct controller *ctrl);

void rk_unix_ctrl_push_data(struct controller *ctrl, gpointer data);
#endif
