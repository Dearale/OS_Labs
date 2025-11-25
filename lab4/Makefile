CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -fPIC -Iinclude
LDFLAGS = -lm

ALL = library_1.so library_2.so prog1 prog2

all: $(ALL)

library_1.so: library_1.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

library_2.so: library_2.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)


library_1.o: library_1.c library.h
	$(CC) $(CFLAGS) -c $< -o $@

library_2.o: library_2.c library.h
	$(CC) $(CFLAGS) -c $< -o $@


prog1: prog1.o library_1.so
	$(CC) -o $@ prog1.o ./library_1.so -Wl,-rpath,'$$ORIGIN' $(LDFLAGS)

prog1.o: prog1.c library.h
	$(CC) $(CFLAGS) -c $< -o $@


prog2: prog2.o
	$(CC) -o $@ prog2.o -ldl $(LDFLAGS)

prog2.o: prog2.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f *.o $(ALL)

.PHONY: all clean
