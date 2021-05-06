######################
# Makefile for CliFM #
######################

OS := $(shell uname -s)

PROG = ../clifm
OBJS := $(shell ls *.c | sed "s/.c\$$/.o/g")

CFLAGS = -O3 -s -fstack-protector-strong -march=native -Wall
LIBS_Linux = -lreadline -lacl -lcap
LIBS_FreeBSD = -lreadline -lintl

%.o: %.c
	cc -c $(CFLAGS) $<

build: ${OBJS}
	@printf "Detected operating system: ";
	@echo $(OS)
	cc -o $(PROG) $^ ${LIBS_${OS}}
