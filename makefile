CC=g++
CFLAGS=-Wall -Wextra -W -O3 -I/usr/include/SDL
LDFLAGS=-lSDL -lGL -lGLU
EXEC=glbench

all: $(EXEC)

glbench: main.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.cpp %.h
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean

clean:
	rm -rf *.o $(EXEC)

