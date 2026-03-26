CFLAGS = -Wall -Wextra -pedantic -ggdb

main: main.c
	gcc $(CFLAGS) -o main main.c
