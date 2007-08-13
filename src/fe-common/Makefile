LIB= fe_xmpp
SRCS= fe-xmpp.c module-formats.c xmpp-completion.c fe-xmpp-roster.c
OBJS= $(SRCS:.c=.o)

CFLAGS+=	-std=c99 -Wall -W \
			-I${IRSSI_INCLUDE} \
			-I${IRSSI_INCLUDE}/src \
			-I${IRSSI_INCLUDE}/src/core \
			-I$(IRSSI_INCLUDE)/src/fe-common/core \
			-I../../src/core \
			`pkg-config --cflags loudmouth-1.0` \
			-DUOFF_T_LONG
LDFLAGS+= `pkg-config --libs loudmouth-1.0`

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) -shared -o lib$@.so $^ $(LDFLAGS)

%.o: %.c
	$(CC) -fPIC -o $@ -c $< $(CFLAGS)

.PHONY: clean user-install

user-install:
	install -d ~/.irssi/modules
	install lib$(LIB).so ~/.irssi/modules

clean:
	rm -rf *.o *.so *.core 
