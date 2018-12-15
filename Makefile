
options=-std=gnu99 -Wall -g -L/path/to/libs -lm -lpthread -pg
source=solution1.c
all:solution solution_optimized

solution: $(source)
	gcc $(source) -o solution1 $(options)

solution_optimized: $(sources)
	gcc $(source) -o solution1_optimized $(options) -O3


clean:
	rm solution1 solution1_optimized



