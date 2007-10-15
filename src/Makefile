SUBDIR= core fe-common fe-text

.PHONY: clean all user-install install

all:
	for dir in ${SUBDIR} ; do (${MAKE} -C $$dir all) ; done

clean:
	for dir in ${SUBDIR} ; do (${MAKE} -C $$dir clean) ; done

user-install:
	for dir in ${SUBDIR} ; do (${MAKE} -C $$dir user-install) ; done

install:
	for dir in ${SUBDIR} ; do (${MAKE} -C $$dir install) ; done
