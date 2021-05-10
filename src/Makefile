######################
# Makefile for CliFM #
######################

OS != uname -s

PROG = ../clifm
OBJS != ls *.c | sed "s/.c\$$/.o/g"

CFLAGS = -I/usr/local/include -O3 -fstack-protector-strong -march=native -Wall
LIBS_Linux = -lreadline -lacl -lcap
LIBS_FreeBSD = -L/usr/local/lib -lreadline -lintl

%.o: %.c
	cc -c $(CFLAGS) $<

build: ${OBJS}
	@printf "Detected operating system: ";
	@echo "$(OS)"
	cc -o $(PROG) ${OBJS} ${LIBS_${OS}}

clean:
	rm -f *.o
