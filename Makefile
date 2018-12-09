CC = gcc

all: dfc.o dfs.o
	$(CC) -o dfc dfc.o -lcrypto
	$(CC) -o dfs dfs.o

dfc.o: dfc.c
	$(CC) -g -c dfc.c

dfs.o: dfs.c
	$(CC) -c dfs.c
