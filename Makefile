CFLAGS = -Wall -Wextra -pedantic -ggdb

main: main.o huffman.o
	gcc $(CFLAGS) -o main main.o huffman.o

main.o: main.c
	gcc $(CFLAGS) main.c -c

huffman.o: huffman.c
	gcc $(CFLAGS) huffman.c -c

test: main
	./test.sh test4.txt
	./test.sh test3.txt
	./test.sh test2.txt
	./test.sh testcopy.txt
	./test.sh test.txt
