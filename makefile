matrix: matrix.c
	gcc matrix.c -lncursesw -lpthread -o matrix

clean:
	rm matrix
