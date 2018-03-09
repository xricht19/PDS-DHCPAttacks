DHCPSTARVENAME=pds-dhcpstarve
SDIR=src
IDIR =src/include
CC=g++
CFLAGS=-I$(IDIR)

ODIR=build
LDIR =../lib

LIBS=-lm

_DEPS = pds-dhcpstarve.h pds-dhcpCore.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = pds-dhcpstarve.o pds-dhcpCore.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(DHCPSTARVENAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
	rm -f $(DHCPSTARVENAME)