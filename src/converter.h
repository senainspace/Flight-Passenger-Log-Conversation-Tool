#ifndef CONVERTER_H
#define CONVERTER_H

#include <stdint.h>

/* Boarding status byte values */
#define STATUS_BOARDED   0
#define STATUS_CANCELLED 1
#define STATUS_DELAYED   2

/* Cabin class byte values */
#define CABIN_ECONOMY  0
#define CABIN_BUSINESS 1
#define CABIN_FIRST    2

/* CSV separator argument values */
#define SEP_COMMA     1
#define SEP_TAB       2
#define SEP_SEMICOLON 3

/* OS line-ending argument values */
#define OPSYS_WINDOWS 1
#define OPSYS_LINUX   2
#define OPSYS_MACOS   3

/*
 * Binary record layout (217 bytes, no padding)
 *  0  ticket_id[8]       3 uppercase letters + 4 digits + '\0'
 *  8  timestamp[20]      ISO 8601 + '\0'
 * 28  baggage_weight      float, 0.0-50.0 kg
 * 32  loyalty_points      int,   0-10000
 * 36  status              uint8, STATUS_* above
 * 37  destination[31]     max 30 chars + '\0'
 * 68  cabin_class         uint8, CABIN_* above
 * 69  seat_num            int,   1-300
 * 73  app_ver[16]         vX.Y.Z + '\0'
 * 89  passenger_name[128] UTF-8
 */
typedef struct __attribute__((packed)) {
    char    ticket_id[8];
    char    timestamp[20];
    float   baggage_weight;
    int     loyalty_points;
    uint8_t status;
    char    destination[31];
    uint8_t cabin_class;
    int     seat_num;
    char    app_ver[16];
    char    passenger_name[128];
} PassengerRecord;

/* Read CSV file, return heap-allocated array (caller must free).
   Invalid mandatory fields (ticket_id, seat_num) are skipped with a warning. */
PassengerRecord *read_csv(const char *filename, int sep_type,
                          int opsys_type, int *out_count);

/* Write count records to a binary .dat file. Returns 0 on success, -1 on error. */
int write_binary(const char *filename,
                 const PassengerRecord *records, int count);

/* Read records from a binary .dat file. Returns heap-allocated array or NULL. */
PassengerRecord *read_binary(const char *filename, int *out_count);

#endif /* CONVERTER_H */
