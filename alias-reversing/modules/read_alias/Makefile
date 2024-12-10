all: setup-dirs kmod_readalias.ko libkmodreadlias.a 
.PHONY: clean setup-dirs

CFLAGS= -O3 -std=gnu11 -Wall -Wextra -Wpedantic -Werror

ifndef KERNEL_PATH
$(info Using currently running kernel. Overwrite KERNEL_PATH env var to change this)
KERNEL_RELEASE = $(shell uname -r)
KERNEL_PATH = /lib/modules/$(KERNEL_RELEASE)/build
endif

$(info Using KERNEL_PATH=$(KERNEL_PATH))

obj-m += kmod_readalias.o
kmod_readalias.ko: kmod_readalias.c
	make -C $(KERNEL_PATH) M=$(PWD) modules

libkmodreadlias.a: readalias.c
	gcc $(CFLAGS) -o readalias.o -c readalias.c
	ar rcs libkmodreadalias.a readalias.o
clean:
	rm -f kmod_readalias.ko
	rm -f libkmodreadalias.a
	rm -f modules.order
	rm -f *.o
	rm -f *.mod
