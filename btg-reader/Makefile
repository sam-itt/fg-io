SRCDIR=.

CC=gcc
CFLAGS=-g3 -O0
LDFLAGS=-lz -lm
EXEC=btg-reader
SRC= $(wildcard $(SRCDIR)/*.c)
OBJ= $(SRC:.c=.o)

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrproper

clean:
	rm -rf *.o

mrproper: clean
	rm -rf $(EXEC)

