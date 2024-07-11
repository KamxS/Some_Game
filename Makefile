main: main.c
	gcc src/hashmap.c src/sds.c main.c -lraylib -o main.exe
