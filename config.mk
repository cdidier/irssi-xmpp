# paths
PREFIX ?= /usr/local

# where are the sources of irssi?
IRSSI_INCLUDE ?= ${PREFIX}/include/irssi
# where should be installed the module?
IRSSI_LIB ?= ${PREFIX}/lib/irssi
# where should be installed the documentation?
IRSSI_DOC ?= ${PREFIX}/share/doc/irssi
# where should be installed the help for commands ?
IRSSI_HELP ?= ${PREFIX}/share/irssi/help

# includes and libs
INCS =	${LIB_INCS} \
	-I../../src/core \
	-I${IRSSI_INCLUDE} \
	-I${IRSSI_INCLUDE}/src \
	-I${IRSSI_INCLUDE}/src/core \
	-I$(IRSSI_INCLUDE)/src/fe-common/core \
	-I$(IRSSI_INCLUDE)/src/fe-text \
	`pkg-config --cflags glib-2.0` \
	`pkg-config --cflags loudmouth-1.0`
LIBS =	${LIB_LIBS}

# flags
CFLAGS += -fPIC -DUOFF_T_LONG
LDFLAGS += -shared

# debug
#CFLAGS += -std=c99 -W -g -Wall -Wno-unused-parameter
#CFLAGS += -Wno-deprecated-declarations
#CFLAGS += -Wno-cast-function-type

# compiler and linker
CC ?= cc
