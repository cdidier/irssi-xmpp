include ../../config.mk

OBJS = ${SRCS:.c=.o}

LIBSO=lib${LIB}.so

all: ${LIBSO}

.c.o:
	${CC} ${CPPFLAGS} ${CFLAGS} ${INCS} -o $@ -c $<

${LIBSO}: ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LIBS}

clean:
	rm -f ${LIBSO} ${OBJS}

install: all
	@echo installing the module ${LIBSO} to ${DESTDIR}${IRSSI_LIB}/modules
	install -d ${DESTDIR}${IRSSI_LIB}/modules
	install ${LIBSO} ${DESTDIR}${IRSSI_LIB}/modules

uninstall:
	@echo deinstalling the module ${LIBSO} from ${DESTDIR}${IRSSI_LIB}/modules
	rm -f ${DESTDIR}${IRSSI_LIB}/modules/${LIBSO}

user-install:
	env DESTDIR= IRSSI_LIB=~/.irssi ${MAKE} install

user-uninstall:
	env DESTDIR= IRSSI_LIB=~/.irssi ${MAKE} uninstall

.PHONY: clean install uninstall user-install user-uninstall
