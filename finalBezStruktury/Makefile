DHCPSTARVENAME=pds-dhcpstarve
DHCPROGUENAME=pds-dhcprogue
CC=g++
CFLAGS=-std=c++11 -pthread
CFLAGS_ROGUE=-std=c++11
LIBS=-lm

DEPS = pds-dhcpstarve.h pds-dhcpCore.h
DEPS_ROGUE = pds-dhcprogue.h pds-dhcpCore.h

OBJ = pds-dhcpstarve.o pds-dhcpCore.o

OBJROGUE = pds-dhcprogue_rogue.o pds-dhcpCore_rogue.o


all: $(DHCPROGUENAME) $(DHCPSTARVENAME)

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%_rogue.o: %.cpp $(DEPS_ROGUE)
	$(CC) -c -o $@ $< $(CFLAGS_ROGUE)

$(DHCPSTARVENAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(DHCPROGUENAME): $(OBJROGUE)
	$(CC) -o $@ $^ $(CFLAGS_ROGUE) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o
	rm -f $(DHCPSTARVENAME)
	rm -f $(DHCPROGUENAME)