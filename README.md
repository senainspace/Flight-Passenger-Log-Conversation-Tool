# Flight Passenger Log Conversion Tool

A command-line utility written in **C** that converts airline passenger records between **CSV**, **Binary (.dat)**, and **XML** formats, with **XSD validation** and full **UTF-8 / UTF-16 encoding** support (LE & BE).

---

## Project Structure

```
.
├── main.c              # Argument parsing and conversion routing
├── csv_reader.c/h      # CSV parsing (separator, line-ending, emoji, UTF-8)
├── binary_writer.c/h   # Binary (.dat) file writing – PassengerRecord struct
├── binary_reader.c/h   # Binary (.dat) file reading
├── xml_writer.c/h      # UTF-8 XML generation via libxml2
├── xml_encoding.c/h    # UTF-8 ↔ UTF-16 LE/BE conversion with BOM handling
├── xsd_validator.c/h   # XSD validation wrapper (calls validate.c)
├── validate.c          # libxml2-based XSD validator (instructor-supplied stub)
├── flightlogs.xsd      # XML Schema for passenger log documents
├── flightlog.csv       # Sample CSV data (5 records, Turkish names, emoji)
├── Makefile
└── README.md
```

---

## Data Dictionary

| Field            | Type    | Constraints                                |
|------------------|---------|--------------------------------------------|
| `ticket_id`      | string  | **Required.** 3 uppercase letters + 4 digits (e.g. `THY1234`) |
| `timestamp`      | string  | ISO 8601 (e.g. `2025-03-01T14:25:00`)      |
| `baggage_weight` | float   | 0.0 – 50.0 kg                              |
| `loyalty_points` | int     | 0 – 10000                                  |
| `status`         | emoji   | 🟢 BOARDED · 🔴 CANCELLED · ⚠️ DELAYED    |
| `destination`    | string  | Max 30 characters                          |
| `cabin_class`    | enum    | `ECONOMY` · `BUSINESS` · `FIRST`           |
| `seat_num`       | int     | **Required.** 1 – 300                      |
| `app_ver`        | string  | Format `vX.Y.Z` (e.g. `v1.0.2`)           |
| `passenger_name` | string  | UTF-8, non-ASCII supported (Turkish, etc.) |

---

## Dependencies

| Library  | Purpose             | Install                         |
|----------|---------------------|---------------------------------|
| libxml2  | XML / XSD handling  | `brew install libxml2` (macOS)  |
|          |                     | `sudo apt-get install libxml2-dev` (Debian/Ubuntu) |

---

## Build

```bash
make
```

The binary is placed at `./flightTool`.

---

## Usage

```
./flightTool <input_file> <output_file> <conv_type> \
             -separator <1|2|3> -opsys <1|2|3> \
             [-encoding <1|2|3>] [-h]
```

### Conversion Types

| `conv_type` | Description                                   |
|-------------|-----------------------------------------------|
| `1`         | CSV → Binary (.dat)                           |
| `2`         | Binary → XML (always UTF-8)                   |
| `3`         | Validate XML against XSD                      |
| `4`         | XML encoding conversion (UTF-8 ↔ UTF-16)      |

### `-separator` Options

| Value | Character  |
|-------|------------|
| `1`   | Comma `,`  |
| `2`   | Tab `\t`   |
| `3`   | Semicolon `;` |

### `-opsys` Options

| Value | Line Ending      |
|-------|------------------|
| `1`   | Windows `\r\n`   |
| `2`   | Linux `\n`       |
| `3`   | macOS `\n`       |

### `-encoding` Options (required for `conv_type 4`)

| Value | Conversion                                    |
|-------|-----------------------------------------------|
| `1`   | UTF-8 → UTF-16 Little Endian (BOM: `FF FE`)   |
| `2`   | UTF-8 → UTF-16 Big Endian (BOM: `FE FF`)      |
| `3`   | UTF-16 (auto-detect LE/BE) → UTF-8            |

---

## Example Pipeline

```bash
# 1. Parse CSV and write binary
./flightTool flightlog.csv flightdata.dat 1 -separator 1 -opsys 2

# 2. Convert binary to UTF-8 XML
./flightTool flightdata.dat flightlogs_utf8.xml 2 -separator 1 -opsys 2

# 3a. Convert UTF-8 XML → UTF-16 Little Endian
./flightTool flightlogs_utf8.xml flightlogs_utf16le.xml 4 -separator 1 -opsys 2 -encoding 1

# 3b. Convert UTF-8 XML → UTF-16 Big Endian
./flightTool flightlogs_utf8.xml flightlogs_utf16be.xml 4 -separator 1 -opsys 2 -encoding 2

# 3c. Convert UTF-16 back to UTF-8
./flightTool flightlogs_utf16le.xml flightlogs_roundtrip.xml 4 -separator 1 -opsys 2 -encoding 3

# 4. Validate XML against XSD
./flightTool flightlogs_utf8.xml flightlogs.xsd 3 -separator 1 -opsys 2
```

Or run all steps at once:

```bash
make test
```

---

## Binary Format

Each record is stored as a packed `PassengerRecord` struct (217 bytes, no padding):

| Offset | Field            | Size | Notes                             |
|--------|------------------|------|-----------------------------------|
| 0      | `ticket_id`      | 8    | 3 letters + 4 digits + `\0`       |
| 8      | `timestamp`      | 20   | ISO 8601 + `\0`                   |
| 28     | `baggage_weight` | 4    | 32-bit float                      |
| 32     | `loyalty_points` | 4    | 32-bit int                        |
| 36     | `status`         | 1    | 0=BOARDED 1=CANCELLED 2=DELAYED   |
| 37     | `destination`    | 31   | max 30 chars + `\0`               |
| 68     | `cabin_class`    | 1    | 0=ECONOMY 1=BUSINESS 2=FIRST      |
| 69     | `seat_num`       | 4    | 32-bit int                        |
| 73     | `app_ver`        | 16   | `vX.Y.Z\0`                        |
| 89     | `passenger_name` | 128  | UTF-8, Turkish characters OK      |

---

## XML Output Format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<flightlogs_utf8>
  <entry id="1">
    <ticket>
      <ticket_id>THY1234</ticket_id>
      <destination>Istanbul</destination>
      <app_ver>v1.0.2</app_ver>
    </ticket>
    <metrics status="🟢" cabin_class="ECONOMY">
      <baggage_weight>22.50</baggage_weight>
      <loyalty_points>1500</loyalty_points>
      <seat_num>85</seat_num>
    </metrics>
    <timestamp>2025-03-01T14:25:00</timestamp>
    <passenger_name current_encoding="UTF-8" first_char_hex="C396">Ömer Yılmaz</passenger_name>
  </entry>
</flightlogs_utf8>
```

The root element name equals the output filename without extension.

The `first_char_hex` attribute holds the hex encoding of the first character's
byte sequence in the current encoding:

| Encoding   | Example char | Bytes      | `first_char_hex` |
|------------|-------------|------------|------------------|
| UTF-8      | Ö (U+00D6)  | `C3 96`    | `C396`           |
| UTF-16 LE  | Ö (U+00D6)  | `D6 00`    | `D600`           |
| UTF-16 BE  | Ö (U+00D6)  | `00 D6`    | `00D6`           |
| UTF-8      | A (U+0041)  | `41`       | `41`             |

---

## Clean

```bash
make clean
```

Removes the compiled binary, all object files, and generated `.dat` / `.xml` files.
