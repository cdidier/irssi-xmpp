SUBDIR= src

.PHONY: clean all user-install

all:
	@if [ -z "${IRSSI_INCLUDE}" ] ;                                            \
	then                                                                       \
		echo "Please specify a valid IRSSI_INCLUDE environment variable !" ;   \
		exit 1 ;                                                               \
	else                                                                       \
		${MAKE} -C ${SUBDIR} all ;                                             \
	fi

clean:
	${MAKE} -C ${SUBDIR} clean

user-install:
	${MAKE} -C ${SUBDIR} user-install
