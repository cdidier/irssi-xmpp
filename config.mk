# paths
PREFIX ?= /usr/local

# where are the sources of irssi? lets ask pkg-config first
irssi_incdirs_pc != pkg-config --cflags-only-I irssi-1 2>/dev/null
ifneq (,$(irssi_incdirs_pc))
IRSSI_INCLUDE ?= `pkg-config --cflags-only-I irssi-1`
irssi_incdirs = ${IRSSI_INCLUDE}
else
IRSSI_INCLUDE ?= ${PREFIX}/include/irssi
irssi_incdirs = \
	-I${IRSSI_INCLUDE} \
	-I${IRSSI_INCLUDE}/src \
	-I${IRSSI_INCLUDE}/src/core \
	-I$(IRSSI_INCLUDE)/src/fe-common/core \
	-I$(IRSSI_INCLUDE)/src/fe-text
endif
# where should be installed the module?
irssi_libdir_pc != pkg-config --variable=libdir irssi-1 2>/dev/null
ifneq (,$(irssi_libdir_pc))
IRSSI_LIB ?= ${irssi_libdir_pc}/irssi
else
IRSSI_LIB ?= ${PREFIX}/lib/irssi
endif
# where should be installed the documentation?
IRSSI_DOC ?= ${PREFIX}/share/doc/irssi
# where should be installed the help for commands ?
IRSSI_HELP ?= ${PREFIX}/share/irssi/help

# includes and libs
INCS =	${LIB_INCS} \
	-I../../src/core \
	${irssi_incdirs} \
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
