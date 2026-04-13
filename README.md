# Flight Passenger Log Conversion Tool

**C** ile yazılmış bir komut satırı aracı. Havayolu yolcu kayıtlarını **CSV**, **Binary** ve **XML** formatları arasında dönüştürür. **XSD validation** ve **UTF-8 / UTF-16** encoding desteği (LE & BE) içerir.

---

## Proje Yapısı

```
.
├── src/
│   ├── main.c          # Argüman parsing ve conversion routing
│   ├── converter.c/h   # CSV okuma + binary dosya yazma/okuma
│   ├── xml.c/h         # XML üretimi ve UTF-8/UTF-16 encoding dönüşümü
│   ├── validate.c/h    # libxml2 ile XSD validation (hoca tarafından sağlandı)
├── build/              # Derlenmiş object file'lar (make tarafından otomatik oluşturulur)
├── flightlog.csv       # Input CSV verisi
├── flightlogs.xsd      # XML validation için XSD schema
├── Makefile
└── README.md
```

---

## Bağımlılıklar

```bash
# Ubuntu / Debian
sudo apt-get install libxml2-dev

# macOS
brew install libxml2
```

---

## Derleme

```bash
make
```

Proje kök dizininde `./flightTool` üretir.

---

## Kullanım

```
./flightTool <input_file> <output_file> <conv_type> -separator <1|2|3> -opsys <1|2|3> [-encoding <1|2|3>] [-h]
```

| Argüman | Açıklama |
|---|---|
| `input_file` | Okunacak kaynak dosya |
| `output_file` | Yazılacak hedef dosya (ya da validation için XSD dosyası) |
| `conv_type` | 1=CSV→Binary, 2=Binary→XML, 3=XSD Validation, 4=Encoding Conversion |
| `-separator` | CSV ayracı: 1=virgül, 2=tab, 3=noktalı virgül |
| `-opsys` | Satır sonu: 1=Windows(\r\n), 2=Linux(\n), 3=macOS(\n) |
| `-encoding` | conv_type 4 için zorunlu: 1=UTF-16LE, 2=UTF-16BE, 3=UTF-16→UTF-8 |
| `-h` | Yardım mesajını göster ve çık |

---

## Örnekler

```bash
# CSV → Binary
./flightTool flightlog.csv flightdata.dat 1 -separator 1 -opsys 2

# Binary → XML (UTF-8)
./flightTool flightdata.dat flightlogs_utf8.xml 2 -separator 1 -opsys 2

# XML → UTF-16 Little Endian
./flightTool flightlogs_utf8.xml flightlogs_utf16le.xml 4 -separator 1 -opsys 2 -encoding 1

# XML → UTF-16 Big Endian
./flightTool flightlogs_utf8.xml flightlogs_utf16be.xml 4 -separator 1 -opsys 2 -encoding 2

# UTF-16 → UTF-8
./flightTool flightlogs_utf16le.xml flightlogs_roundtrip.xml 4 -separator 1 -opsys 2 -encoding 3

# XSD Validation
./flightTool flightlogs_utf8.xml flightlogs.xsd 3 -separator 1 -opsys 2
```

Tüm adımları tek seferde çalıştır:

```bash
make test
```

---

## Veri Sözlüğü

| Alan | Tip | Kısıtlamalar |
|---|---|---|
| `ticket_id` | string | **Zorunlu.** 3 büyük harf + 4 rakam (örn. `THY1234`) |
| `timestamp` | string | ISO 8601 (örn. `2025-03-01T14:25:00`) |
| `baggage_weight` | float | 0.0 – 50.0 kg |
| `loyalty_points` | int | 0 – 10000 |
| `status` | emoji | 🟢 BOARDED · 🔴 CANCELLED · ⚠️ DELAYED |
| `destination` | string | Maksimum 30 karakter |
| `cabin_class` | enum | `ECONOMY` · `BUSINESS` · `FIRST` |
| `seat_num` | int | **Zorunlu.** 1 – 300 |
| `app_ver` | string | Format `vX.Y.Z` (örn. `v1.0.2`) |
| `passenger_name` | string | UTF-8, ASCII dışı karakterler desteklenir |

---

## Binary Format

Her kayıt, packed `PassengerRecord` struct'ıdır (217 byte, padding yok):

| Offset | Alan | Boyut |
|---|---|---|
| 0 | `ticket_id` | 8 |
| 8 | `timestamp` | 20 |
| 28 | `baggage_weight` | 4 |
| 32 | `loyalty_points` | 4 |
| 36 | `status` | 1 |
| 37 | `destination` | 31 |
| 68 | `cabin_class` | 1 |
| 69 | `seat_num` | 4 |
| 73 | `app_ver` | 16 |
| 89 | `passenger_name` | 128 |

---

## XML Output Formatı

Root element = output dosya adının uzantısız hali.

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

`first_char_hex` — aktif encoding'deki ilk karakterin byte'larının hex gösterimi:

| Encoding | Karakter | Byte'lar | Değer |
|---|---|---|---|
| UTF-8 | Ö (U+00D6) | `C3 96` | `C396` |
| UTF-16LE | Ö (U+00D6) | `D6 00` | `D600` |
| UTF-16BE | Ö (U+00D6) | `00 D6` | `00D6` |

---

## Temizleme

```bash
make clean
```
