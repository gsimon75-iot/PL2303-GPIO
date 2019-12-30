TARGETS		= test spi_eeprom

CXX		= g++

CXXFLAGS	= -ggdb3 -std=c++11
LDFLAGS		= -ggdb3
LDLIBS		= -lusb

.PHONY:		all clean

all:		$(TARGETS)

clean:
		rm -rf *.o $(TARGETS)

%.o:		%.cc
		$(CXX) -c $(CXXFLAGS) $^

test:		test.o
		$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)


spi_eeprom:	spi_eeprom.o
		$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)

