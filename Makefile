IDIR =./include
CXX=g++
CFLAGS=-I$(IDIR)

ODIR=./
LDIR=./lib

LIBS=-lm

_DEPS = clipper.h benchmark.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = benchmark.o main.o clipper.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)

clipper: $(OBJ)
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean clipper

clean:
	rm -f $(ODIR)/*.o *~ $(INCDIR)/*~ 
