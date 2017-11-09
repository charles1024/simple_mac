#ast_mac-objs+=ast_fileio.o
#ast_mac-objs+=ast_dev.o
simpleMac-objs+=simple_mac.o
obj-m+=simpleMac-objs

obj-m+=simpleMac.o
MY_CFLAGS+= -g -DDEBUG
ccflags-y+= ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD)  modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean