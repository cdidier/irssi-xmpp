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
	`pkg-config --cflags loudmouth-1.0`
LIBS =	${LIB_LIBS}

# flags
CFLAGS = -fPIC -std=c99 -DUOFF_T_LONG
LDFLAGS = -shared

# debug
CFLAGS += -W -g -Wall -Wno-unused-parameter

# compiler and linker
CC = cc
