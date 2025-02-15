COMPILE_FLAGS := -Wall -Werror `ncursesw6-config --libs`

LINKER_FLAGS := -lpthread -lncursesw `ncursesw6-config --cflags`

matrix: matrix.c
	gcc matrix.c $(COMPILE_FLAGS) $(LINKER_FLAGS) -o matrix

clean:
	rm matrix
