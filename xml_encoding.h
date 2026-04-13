#ifndef XML_ENCODING_H
#define XML_ENCODING_H

/* Encoding type codes (matches -encoding CLI argument) */
#define ENCODING_UTF16LE  1   /* UTF-8 → UTF-16 Little Endian (BOM: FF FE) */
#define ENCODING_UTF16BE  2   /* UTF-8 → UTF-16 Big Endian    (BOM: FE FF) */
#define ENCODING_TO_UTF8  3   /* UTF-16 (auto-detect LE/BE) → UTF-8        */

/**
 * convert_xml_encoding – convert the encoding of an XML file.
 *
 * For ENCODING_UTF16LE / ENCODING_UTF16BE:
 *   - Reads a UTF-8 XML file.
 *   - Writes the result as UTF-16 LE or BE with the appropriate BOM.
 *   - Updates passenger_name current_encoding and first_char_hex attributes.
 *
 * For ENCODING_TO_UTF8:
 *   - Reads a UTF-16 XML file (BOM auto-detected).
 *   - Writes the result as UTF-8.
 *   - Updates passenger_name current_encoding and first_char_hex attributes.
 *
 * @param input_file    source XML file path
 * @param output_file   destination XML file path
 * @param encoding_type ENCODING_UTF16LE | ENCODING_UTF16BE | ENCODING_TO_UTF8
 *
 * Returns 0 on success, -1 on error.
 */
int convert_xml_encoding(const char *input_file,
                         const char *output_file,
                         int         encoding_type);

/* ── Internal helpers (also used by xml_writer.c) ───────────────────── */

/**
 * utf8_char_len – return the byte length of the UTF-8 character whose
 * leading byte is `c`, or -1 if `c` is an invalid leading byte.
 */
int utf8_char_len(unsigned char c);

/**
 * utf8_decode – decode one UTF-8 character starting at `s` (must be
 * `len` bytes long as returned by utf8_char_len).
 * Returns the Unicode code point.
 */
unsigned int utf8_decode(const unsigned char *s, int len);

/**
 * compute_first_char_hex_utf16le – hex string of the UTF-16 LE byte
 * sequence for the first character of `utf8_str`.
 * `hex_out` must be at least 9 bytes.
 */
void compute_first_char_hex_utf16le(const char *utf8_str, char *hex_out);

/**
 * compute_first_char_hex_utf16be – same for UTF-16 BE.
 */
void compute_first_char_hex_utf16be(const char *utf8_str, char *hex_out);

#endif /* XML_ENCODING_H */
