LIB= fe_xmpp
SRCS= fe-xmpp.c module-formats.c xmpp-completion.c fe-xmpp-rosters.c fe-xmpp-queries.c
OBJS= $(SRCS:.c=.o)

CFLAGS+=	-std=c99 \
			-I${IRSSI_INCLUDE} \
			-I${IRSSI_INCLUDE}/src \
			-I${IRSSI_INCLUDE}/src/core \
			-I$(IRSSI_INCLUDE)/src/fe-common/core \
			-I../../src/core \
			`pkg-config --cflags loudmouth-1.0` \
			-DUOFF_T_LONG
LDFLAGS+= `pkg-config --libs loudmouth-1.0`

ifeq ($(DEBUG), yes)
CFLAGS+=	-Wall -W -ggdb -DDEBUG
endif

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
