#ifndef CSV_READER_H
#define CSV_READER_H

#include "binary_writer.h"

/* Separator type codes (matches -separator CLI argument) */
#define SEP_COMMA     1   /* ',' */
#define SEP_TAB       2   /* '\t' */
#define SEP_SEMICOLON 3   /* ';' */

/* OS line-ending type codes (matches -opsys CLI argument) */
#define OPSYS_WINDOWS 1   /* \r\n */
#define OPSYS_LINUX   2   /* \n   */
#define OPSYS_MACOS   3   /* \n   */

/**
 * read_csv – parse `filename` and return a heap-allocated array of
 *            PassengerRecord structs.
 *
 * @param filename   path to the CSV file
 * @param sep_type   SEP_COMMA | SEP_TAB | SEP_SEMICOLON
 * @param opsys_type OPSYS_WINDOWS | OPSYS_LINUX | OPSYS_MACOS
 * @param out_count  [out] number of valid records parsed
 *
 * Returns a malloc'd array on success (caller must free()), or NULL on error.
 * Records with missing / invalid mandatory fields (ticket_id, seat_num) are
 * skipped with a warning printed to stderr.
 */
PassengerRecord *read_csv(const char *filename,
                          int         sep_type,
                          int         opsys_type,
                          int        *out_count);

#endif /* CSV_READER_H */
