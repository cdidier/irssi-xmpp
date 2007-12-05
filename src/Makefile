# $Id$

all clean user-install install:
	@echo "core module:"
	@cd core/ && ${MAKE} $@
	@echo "fe-common submodule:"
	@cd fe-common/ && ${MAKE} $@
	@echo "fe-text submodule:"
	@cd fe-text/ && ${MAKE} $@	

.PHONY: all clean user-install install
