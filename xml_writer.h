#ifndef XML_WRITER_H
#define XML_WRITER_H

#include "binary_writer.h"

/**
 * write_xml – create a UTF-8 XML file from `records` using libxml2.
 *
 * The root element name is derived from `output_file` by stripping the
 * file extension (e.g. "flightlogs_utf8.xml" → root element "flightlogs_utf8").
 *
 * Each <entry> contains:
 *   <ticket>    ticket_id, destination, app_ver
 *   <metrics>   status (emoji attr), cabin_class (attr),
 *               baggage_weight, loyalty_points, seat_num
 *   <timestamp>
 *   <passenger_name current_encoding="UTF-8" first_char_hex="...">
 *
 * @param output_file  path of the XML file to create
 * @param records      array of PassengerRecord
 * @param count        number of records
 *
 * Returns 0 on success, -1 on error.
 */
int write_xml(const char *output_file,
              const PassengerRecord *records,
              int count);

/**
 * compute_first_char_hex_utf8 – compute the hex string of the UTF-8 byte
 * sequence for the first character of `utf8_str`.
 *
 * @param utf8_str  null-terminated UTF-8 string
 * @param hex_out   output buffer (at least 9 bytes for 4-byte chars + '\0')
 */
void compute_first_char_hex_utf8(const char *utf8_str, char *hex_out);

#endif /* XML_WRITER_H */
