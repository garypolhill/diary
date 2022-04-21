.PHONY: all

all: ls find

ls: ls.c ls.h
	gcc -DCOMPILE_LS_MAIN -o ls ls.c

ls.o: ls.c ls.h
	gcc -c -o ls.o ls.c

find: ls.h ls.o find.c find.h
	gcc -DCOMPILE_FIND_MAIN -o find ls.o find.c
