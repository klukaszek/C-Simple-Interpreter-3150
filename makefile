
all: a4 a4ng

a4: a4.c
	gcc -g a4.c -o a4 -lncurses

a4ng: a4.c
	gcc -g a4.c -o a4ng -DNOGRAPHICS

make clean:
	rm -f a4 a4ng