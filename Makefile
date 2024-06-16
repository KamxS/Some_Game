main: main.c
	gcc main.c -lraylib -o main.exe

test: network_test.c
	gcc network_test.c -lwinmm -lws2_32 -o network_test.exe
