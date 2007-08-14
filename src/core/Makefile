LIB= xmpp_core
SRCS= xmpp-core.c xmpp-servers.c xmpp-commands.c xmpp-queries.c xmpp-protocol.c xmpp-rosters.c
OBJS= $(SRCS:.c=.o)

CFLAGS+=	-fPIC \
			-std=c99 -Wall -W \
			-I${IRSSI_INCLUDE} \
			-I${IRSSI_INCLUDE}/src \
			-I${IRSSI_INCLUDE}/src/core \
			`pkg-config --cflags loudmouth-1.0` \
			-DUOFF_T_LONG -ggdb -DDEBUG
LDFLAGS+=	-shared \
			`pkg-config --libs loudmouth-1.0`

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
