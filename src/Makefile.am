EXTRA_DIST = config.h.in

pkglibdir = $(AUDACIOUS_IPLUGIN_DIR)

pkglib_LTLIBRARIES = xmp.la

xmp_la_LDFLAGS = -module -avoid-version -no-undefined
xmp_la_CXXFLAGS = $(AUDACIOUS_CFLAGS) $(LIBXMP_CFLAGS)
xmp_la_LIBADD = $(AUDACIOUS_LIBS) $(LIBXMP_LIBS)

xmp_la_SOURCES = audxmp.cpp
