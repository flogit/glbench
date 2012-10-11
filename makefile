CC=g++
CFLAGS=-Wall -Wextra -W -g -I/usr/include/SDL
LDFLAGS=-lSDL -lGL -lGLU
EXEC=glbench

all: $(EXEC)

glbench: main.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.cpp %.h
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrproper

clean:
	rm -rf *.o

mrproper: clean
	rm -rf $(EXEC)
