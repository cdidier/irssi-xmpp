LIB= xmpp_core
SRCS= xmpp-core.c xmpp-commands.c xmpp-protocol.c xmpp-queries.c xmpp-rosters.c xmpp-servers.c xmpp-settings.c
OBJS= $(SRCS:.c=.o)

CFLAGS+=	-fPIC -std=c99 \
			-I${IRSSI_INCLUDE} \
			-I${IRSSI_INCLUDE}/src \
			-I${IRSSI_INCLUDE}/src/core \
			`pkg-config --cflags loudmouth-1.0` \
			-DUOFF_T_LONG
LDFLAGS+=	-shared \
			`pkg-config --libs loudmouth-1.0`

ifeq ($(DEBUG), yes)
CFLAGS+=	-Wall -W -ggdb -DDEBUG
endif

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) -shared -o lib$@.so $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean user-install

user-install:
	install -d ~/.irssi/modules
	install lib$(LIB).so ~/.irssi/modules

clean:
	rm -rf *.o *.so *.core
