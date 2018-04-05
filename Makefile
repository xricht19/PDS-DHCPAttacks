DHCPSTARVENAME=pds-dhcpstarve
SDIR=src
IDIR =src/include
CC=g++
CFLAGS=-std=c++11 -I$(IDIR)

ODIR=build
LDIR =../lib

LIBS=-lm

_DEPS = pds-dhcpstarve.h pds-dhcpCore.h pds-RawSocketHelper.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = pds-dhcpstarve.o pds-dhcpCore.o pds-RawSocketHelper.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(DHCPSTARVENAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
	rm -f $(DHCPSTARVENAME)