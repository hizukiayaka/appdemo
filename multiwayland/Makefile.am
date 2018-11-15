#
#  Copyright (c) 2018 Randy Li <ayaka@soulik.info>
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

bin_PROGRAMS = multishow

multishow_SOURCES = \
	multiwayland.c \
	$(NULL)

multishow_CFLAGS = \
	-O0 -g \
	-Wall -Wextra \
	-std=c99 \
	-DDATADIR=\""$(multishow_datadir)"\" \
	$(GST_CFLAGS) \
	$(GST_BAD_VIDOE_CFLAGS) \
	$(GTK3_CFLAGS) \
	$(NULL)

multishow_LDADD = \
	$(GST_LIBS) \
	$(GST_BAD_VIDOE_LIBS) \
	$(GTK3_LIBS) \
	-lgstwayland-1.0 \
	$(NULL)

BUILT_SOURCES =			\
	multishow-resources.h	\
	multishow-resources.c	\
	multishow.gresource     \
	$(NULL)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) \
--generate-dependencies $(srcdir)/multishow.gresource.xml)

multishow.gresource: multishow.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate $<
multishow-resources.c: multishow.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name multishow $<
multishow-resources.h: multishow.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name multishow $<

multishow_datadir = ${datarootdir}/${PACKAGE}
dist_multishow_data_DATA = multishow.gresource

EXTRA_DIST = \
	multishow.gresource.xml \
	$(NULL)
