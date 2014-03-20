#
# Makefile for firstModule.c
#
#
#
obj-m += kyouko2Module.o

default: tester.c
	$(MAKE) -C /usr/src/linux M=$(PWD) modules
	gcc -Wall -g -o run tester.c
clean:
	rm kyouko2Module.ko
	rm *.o
	rm *.mod.c
	rm run
