# You need to insert the module first before running the game.

OBJECTS = game.o Tool.o

.PHONY: build
.PHONY: install
.PHONY: uninstall
.PHONY: clean
.PHONY: distrib


ifneq (${KERNELRELEASE},)

# KERNELRELEASE defined: we are being compiled as part of the Kernel
        obj-m := Tetris.o

else

# We are being compiled as a module: use the Kernel build system

	KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
		PWD := $(shell pwd)

default: module game

module:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules

clean:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
	${RM} game
	rm -f *.o ${program}

game : $(OBJECTS)
	cc -Wall -o game $(OBJECTS)

game.o : game.c Tool.h
Tool.o : Tool.c Tool.h

endif
