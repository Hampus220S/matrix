COMPILE_FLAGS := -Wall -Werror

LINKER_FLAGS := -lpthread -lncurses

matrix: matrix.c matrix.h
	gcc matrix.c $(COMPILE_FLAGS) $(LINKER_FLAGS) -o matrix

clean:
	rm matrix
