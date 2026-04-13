CC      = gcc
CFLAGS  = -Wall -Wextra -g $(shell xml2-config --cflags)
LIBS    = $(shell xml2-config --libs)
TARGET  = flightTool
SRCS    = src/main.c src/converter.c src/xml.c src/validate.c
OBJS    = $(patsubst src/%.c, build/%.o, $(SRCS))

.PHONY: all clean test

all: $(TARGET)

build:
	mkdir -p build

$(TARGET): build $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	@echo "=== Step 1: CSV -> Binary ==="
	./$(TARGET) flightlog.csv flightdata.dat 1 -separator 1 -opsys 2

	@echo "=== Step 2: Binary -> XML (UTF-8) ==="
	./$(TARGET) flightdata.dat flightlogs_utf8.xml 2 -separator 1 -opsys 2

	@echo "=== Step 3a: XML -> UTF-16 LE ==="
	./$(TARGET) flightlogs_utf8.xml flightlogs_utf16le.xml 4 -separator 1 -opsys 2 -encoding 1

	@echo "=== Step 3b: XML -> UTF-16 BE ==="
	./$(TARGET) flightlogs_utf8.xml flightlogs_utf16be.xml 4 -separator 1 -opsys 2 -encoding 2

	@echo "=== Step 3c: UTF-16 LE -> UTF-8 ==="
	./$(TARGET) flightlogs_utf16le.xml flightlogs_roundtrip.xml 4 -separator 1 -opsys 2 -encoding 3

	@echo "=== Step 4: XSD Validation ==="
	./$(TARGET) flightlogs_utf8.xml flightlogs.xsd 3 -separator 1 -opsys 2

	@echo "=== All steps complete ==="

clean:
	rm -rf build $(TARGET) \
	       flightdata.dat \
	       flightlogs_utf8.xml flightlogs_utf16le.xml \
	       flightlogs_utf16be.xml flightlogs_roundtrip.xml
