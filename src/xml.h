#ifndef XML_H
#define XML_H

#include "converter.h"

/* Encoding conversion type argument values */
#define ENCODING_UTF16LE  1   /* UTF-8 → UTF-16 Little Endian (BOM: FF FE) */
#define ENCODING_UTF16BE  2   /* UTF-8 → UTF-16 Big Endian    (BOM: FE FF) */
#define ENCODING_TO_UTF8  3   /* UTF-16 (auto-detect LE/BE)  → UTF-8       */

/* Build a UTF-8 XML file from records using libxml2.
   Root element name = output_file basename without extension.
   Returns 0 on success, -1 on error. */
int write_xml(const char *output_file,
              const PassengerRecord *records, int count);

/* Change the encoding of an existing XML file.
   encoding_type: ENCODING_UTF16LE | ENCODING_UTF16BE | ENCODING_TO_UTF8
   Returns 0 on success, -1 on error. */
int convert_xml_encoding(const char *input_file,
                         const char *output_file, int encoding_type);

#endif /* XML_H */
