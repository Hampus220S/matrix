matrix: matrix.c
	gcc matrix.c -lncurses -lpthread -o matrix

clean:
	rm matrix
