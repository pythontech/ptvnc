CC	= cc
LD	= cc
CFLAGS	= -g -Wall
LDFLAGS	= -g

#GLIB_FLAGS = $(shell /usr/bin/glib-config --cflags)
GLIB_FLAGS = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
#GLIB_LIBS = $(shell /usr/bin/glib-config --libs)
GLIB_LIBS = -L/usr/lib -lglib-2.0

ptvncc_FLAGS = $(GLIB_FLAGS)
ptvncc_LIBS = $(GLIB_LIBS)

%:		%.o
		$(LD) $(LDFLAGS) -o $@ $< $($*_LIBS)

%.o:		%.c
		$(CC) -c $(CFLAGS) $(CPPFLAGS) $($*_FLAGS) -o $@ $<
