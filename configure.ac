AC_INIT([xmp-plugin-audacious], [4.0.0.3.8])
AC_CONFIG_SRCDIR([src/audxmp.cpp])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIR([m4])

dnl AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_LANG([C++])
dnl AC_C_BIGENDIAN
AC_PROG_AR
AC_PATH_PROG(RM,rm,rm)

CXXFLAGS="$CXXFLAGS -std=gnu++11"
dnl CXXFLAGS="$CXXFLAGS -std=gnu++11 -fvisibility=hidden"
dnl EXPORT="__attribute__((visibility(\"default\")))"
dnl AC_DEFINE_UNQUOTED([EXPORT], [$EXPORT], [Define to compiler syntax for public symbols])

dnl AC_HEADER_STDC
AC_CHECK_HEADER_STDBOOL

AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strtoul])
dnl AC_CHECK_HEADER_STDBOOL

PKG_CHECK_MODULES([LIBXMP], [libxmp >= 4], [],
    AC_MSG_ERROR([Could not find libxmp >= 4 API pkgconfig file.]))

AC_MSG_CHECKING([for XMP version])
xmp_version=`$PKG_CONFIG --modversion libxmp`
test -n "$xmp_version" && AC_MSG_RESULT([$xmp_version])
test "x$xmp_version" == "x" && AC_MSG_ERROR([check failed])
XMP_VERSION="$xmp_version"
AC_SUBST([XMP_VERSION])


PKG_CHECK_MODULES([AUDACIOUS], [audacious >= 3.8], [],
    AC_MSG_ERROR([Could not find Audacious >= 3.8 API pkgconfig file.]))

AC_MSG_CHECKING([for Audacious plug-in directory])
audfc_plugin_dir=`$PKG_CONFIG --variable=plugin_dir audacious`
test -n "$audfc_plugin_dir" && AC_MSG_RESULT([$audfc_plugin_dir])
test "x$audfc_plugin_dir" == "x" && AC_MSG_ERROR([check failed])
AUDACIOUS_IPLUGIN_DIR="$audfc_plugin_dir/Input"
AC_SUBST([AUDACIOUS_IPLUGIN_DIR])

LT_INIT([disable-static])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
