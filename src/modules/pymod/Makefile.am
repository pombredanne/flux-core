AM_CFLAGS = \
	@GCCWARN@ \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) -I$(top_srcdir)/src/include \
	$(JSON_CFLAGS) $(ZMQ_CFLAGS) \
	$(PYTHON_CPPFLAGS) \
	"-DFLUX_PYTHON_PATH=\"${pyexecdir}\""

#
# Comms module
#
fluxmod_LTLIBRARIES = pymod.la

pymod_la_SOURCES = py_mod.c
pymod_la_LDFLAGS = $(fluxmod_ldflags) -module $(PYTHON_LDFLAGS)
pymod_la_LIBADD = $(top_builddir)/src/common/libflux-core.la \
		  $(top_builddir)/src/common/libflux-internal.la \
		  $(JSON_LIBS) $(ZMQ_LIBS)

fluxpymod_PYTHON = echo.py __init__.py

