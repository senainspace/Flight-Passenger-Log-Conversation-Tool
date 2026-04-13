CC      = gcc
CFLAGS  = -Wall -Wextra -g $(shell xml2-config --cflags)
LIBS    = $(shell xml2-config --libs)
TARGET  = flightTool
SRCS    = main.c csv_reader.c binary_writer.c binary_reader.c \
          xml_writer.c xml_encoding.c xsd_validator.c validate.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Quick test pipeline ────────────────────────────────────────────────
test: $(TARGET)
	@echo "=== Step 1: CSV → Binary ==="
	./$(TARGET) flightlog.csv flightdata.dat 1 -separator 1 -opsys 2

	@echo "=== Step 2: Binary → XML (UTF-8) ==="
	./$(TARGET) flightdata.dat flightlogs_utf8.xml 2 -separator 1 -opsys 2

	@echo "=== Step 3a: XML → UTF-16 LE ==="
	./$(TARGET) flightlogs_utf8.xml flightlogs_utf16le.xml 4 -separator 1 -opsys 2 -encoding 1

	@echo "=== Step 3b: XML → UTF-16 BE ==="
	./$(TARGET) flightlogs_utf8.xml flightlogs_utf16be.xml 4 -separator 1 -opsys 2 -encoding 2

	@echo "=== Step 3c: UTF-16 LE → UTF-8 ==="
	./$(TARGET) flightlogs_utf16le.xml flightlogs_roundtrip.xml 4 -separator 1 -opsys 2 -encoding 3

	@echo "=== Step 4: XSD Validation ==="
	./$(TARGET) flightlogs_utf8.xml flightlogs.xsd 3 -separator 1 -opsys 2

	@echo "=== All steps complete ==="

clean:
	rm -f $(TARGET) $(OBJS) \
	      flightdata.dat \
	      flightlogs_utf8.xml flightlogs_utf16le.xml \
	      flightlogs_utf16be.xml flightlogs_roundtrip.xml
