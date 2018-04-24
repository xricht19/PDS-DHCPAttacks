DHCPSTARVENAME=pds-dhcpstarve
DHCPROGUENAME=pds-dhcprogue
SDIR=src
IDIR =src/include
CC=g++
CFLAGS=-std=c++11 -I$(IDIR) -pthread

ODIR=build
LDIR =../lib

LIBS=-lm

_DEPS = pds-dhcpstarve.h pds-dhcpCore.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = pds-dhcpstarve.o pds-dhcpCore.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_DEPS_ROGUE = pds-dhcprogue.h pds-dhcpCore.h
DEPS_ROGUE = $(patsubst %,$(IDIR)/%,$(_DEPS_ROGUE))

_OBJ_ROGUE = pds-dhcprogue.o pds-dhcpCore.o
OBJ_ROGUE = $(patsubst %,$(ODIR)/%,$(_OBJ_ROGUE))

all: $(DHCPROGUENAME) $(DHCPSTARVENAME)

$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(DHCPSTARVENAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS_ROGUE)
	$(CC) -c -o $@ $< $(CFLAGS)

$(DHCPROGUENAME): $(OBJ_ROGUE)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)


.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
	rm -f $(DHCPSTARVENAME)
	rm -f $(DHCPROGUENAME)