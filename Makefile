# An admittedly primitive Makefile
# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC = gcc
# Added -pthread for thread library, -g for debugging
# Ensure Wall is present for warnings.
CFLAGS = -Wall -g -pthread
# LDFLAGS = -pthread # Alternative if -pthread in CFLAGS doesn't link threads for some compilers

OBJS = wserver.o request.o io_helper.o 
# Removed wclient.o from OBJS as it's a separate target, not linked into wserver

.SUFFIXES: .c .o 

all: wserver wclient spin.cgi

# Link wserver with its objects and pthread library
wserver: wserver.o request.o io_helper.o
	$(CC) $(CFLAGS) -o wserver wserver.o request.o io_helper.o # $(LDFLAGS) if used

wclient: wclient.o io_helper.o
	$(CC) $(CFLAGS) -o wclient wclient.o io_helper.o # No pthread needed for basic client

spin.cgi: spin.c
	$(CC) $(CFLAGS) -o spin.cgi spin.c # No pthread needed for spin

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f $(OBJS) wserver.o wclient.o spin.o # Clean specific .o files
	-rm -f wserver wclient spin.cgi