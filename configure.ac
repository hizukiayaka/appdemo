#
#  Copyright (c) 2012 Rob Clark <rob@ti.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.
#

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([appdemo], [0.0.1], [https://github.com/hizukiayaka], [appdemo])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AC_PROG_CC

# Enable quiet compiles on automake 1.11.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Obtain compiler/linker options for depedencies
# Check for gst and enable cube-video conditionally:
PKG_CHECK_MODULES(GIO, gio-2.0 gio-unix-2.0)
PKG_CHECK_MODULES(GST, gstreamer-1.0 >= 1.6.0
		  gstreamer-plugins-base-1.0 >= 1.6.0
		  gstreamer-app-1.0 >= 1.6.0
		  gstreamer-allocators-1.0 >= 1.6.0
		  gstreamer-video-1.0 >= 1.6.0
		  glib-2.0,
		 [HAVE_GST=yes], [HAVE_GST=no])

PKG_CHECK_MODULES(GST_BAD_VIDOE, gstreamer-bad-video-1.0 >= 1.10.0,
		  [HAVE_GST_BAD_VIDEO=yes], [HAVE_GST_BAD_VIDEO=no])
PKG_CHECK_MODULES(GTK3, gtk+-3.0 >= 3.4, HAVE_GTK3=yes, HAVE_GTK3=no)

AC_CONFIG_FILES([Makefile
audioservice/Makefile
multiwayland/Makefile
])
AC_OUTPUT
