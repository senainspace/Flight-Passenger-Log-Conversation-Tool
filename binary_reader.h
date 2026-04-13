#ifndef BINARY_READER_H
#define BINARY_READER_H

#include "binary_writer.h"

/**
 * read_binary – read PassengerRecord structs from a binary .dat file.
 *
 * @param filename   path to the binary file produced by write_binary()
 * @param out_count  [out] number of records read
 *
 * Returns a malloc'd array on success (caller must free()), or NULL on error.
 */
PassengerRecord *read_binary(const char *filename, int *out_count);

#endif /* BINARY_READER_H */
