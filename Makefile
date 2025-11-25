
SRC=fso-sh.c fs.c disk.c bitmap.c
OBJ=$(SRC:%.c=%.o)
CFLAGS=-Wall -g

all: fso-sh

-include deps

fso-sh: $(OBJ)
	cc $(CFLAGS) $(OBJ) -o fso-sh

clean:
	rm -f fso-sh $(OBJ) *~ deps
	cc -MM $(SRC) > deps

