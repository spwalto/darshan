AC_DEFUN([CHECK_LDMS],
[

AC_ARG_WITH(ldms,
[  --dxt-enable-ldms=DIR root directory path of ldms installation [defaults to
                    /usr/local or /usr if not found in /usr/local]
  --dxt-disable-ldms to disable ldms usage completely],
[if test "$withval" != no ; then
  if test -d "$withval"
  then
    LDMS_HOME="$withval"
    LDFLAGS="$LDFLAGS -L${LDMS_HOME}/lib -Wl,-rpath=${LDMS_HOME}/lib"
    CPPFLAGS="$CPPFLAGS -I${LDMS_HOME}/include"
    __DARSHAN_LDMS_LINK_FLAGS="-L${LDMS_HOME}/lib -Wl,-rpath=${LDMS_HOME}/lib"
    __DARSHAN_LDMS_INCLUDE_FLAGS="-I${LDMS_HOME}/include"
  else
    AC_MSG_WARN([Sorry, $withval does not exist, checking usual places])
  fi
else
  AC_MSG_ERROR(ldms is required)
fi])

AC_CHECK_HEADER(ldms/ldms.h, [],[AC_MSG_ERROR(ldms.h not found)])
AC_CHECK_HEADER(ldms/ldmsd_stream.h, [],[AC_MSG_ERROR(ldmsd_stream.h not found)])
AC_CHECK_HEADER(ovis_json/ovis_json.h, [],[AC_MSG_ERROR(ovis_json.h not found)])
AC_CHECK_HEADER(ldms/ldms_sps.h, [],[AC_MSG_ERROR(ldms_sps.h not found)])

AC_CHECK_LIB(ldms, inflateEnd, [],[AC_MSG_ERROR(libldms not found)])
AC_CHECK_LIB(ldmsd_stream, inflateEnd, [],[AC_MSG_ERROR(libldmsd_stream not found)])
AC_CHECK_LIB(ovis_json, inflateEnd, [],[AC_MSG_ERROR(libovis_json not found)]
AC_CHECK_LIB(ldmsd_stream, inflateEnd, [],[AC_MSG_ERROR(libldmsd_stream not found)]
AC_CHECK_LIB(ldms_sps, inflateEnd, [],[AC_MSG_ERROR(libldms_sps not found)]
AC_CHECK_LIB(coll, inflateEnd, [],[AC_MSG_ERROR(libcoll not found)]
])
