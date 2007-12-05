# $Id$

include ../../config.mk

OBJS = $(SRCS:.c=.o)

all: ${LIB}

.c.o:
	@echo ${CC} -c $<
	@$(CC) -c $< $(CFLAGS)

${LIB}: $(OBJS)
	@echo ${CC} -o lib$@.so
	@${CC} -o lib$@.so $(OBJS) ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f lib$(LIB).so ${OBJS}

install: all
	@echo installing the module lib$(LIB).so to ${DESTDIR}${IRSSI_LIB}/modules
#	@install -d ${DESTDIR}${IRSSI_LIB}/modules
#	@install lib$(LIB).so ${DESTDIR}${IRSSI_LIB}/modules

user-install:
	@env DESTDIR= IRSSI_LIB=~/.irssi ${MAKE} install

.PHONY: clean user-install install
