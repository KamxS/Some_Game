main: main.c
	gcc src/hashmap.c src/sds.c src/kxecs.c main.c -lraylib -o main.exe
