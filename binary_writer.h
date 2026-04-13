#ifndef BINARY_WRITER_H
#define BINARY_WRITER_H

#include <stdint.h>

/* ── Status codes (uint8_t stored in binary) ─────────────────────────── */
#define STATUS_BOARDED   0
#define STATUS_CANCELLED 1
#define STATUS_DELAYED   2

/* ── Cabin class codes (uint8_t stored in binary) ────────────────────── */
#define CABIN_ECONOMY  0
#define CABIN_BUSINESS 1
#define CABIN_FIRST    2

/*
 * PassengerRecord – packed binary layout (217 bytes total, no padding)
 *
 * Offset | Field           | Size
 * -------|-----------------|------
 *      0 | ticket_id       |   8
 *      8 | timestamp       |  20
 *     28 | baggage_weight  |   4
 *     32 | loyalty_points  |   4
 *     36 | status          |   1
 *     37 | destination     |  31
 *     68 | cabin_class     |   1
 *     69 | seat_num        |   4
 *     73 | app_ver         |  16
 *     89 | passenger_name  | 128
 *    217 | (end)           |
 */
typedef struct __attribute__((packed)) {
    char    ticket_id[8];        /* 3 uppercase letters + 4 digits + '\0' */
    char    timestamp[20];       /* ISO 8601 e.g. "2025-03-01T14:25:00\0" */
    float   baggage_weight;      /* kg  0.0 – 50.0                         */
    int     loyalty_points;      /* 0 – 10000                               */
    uint8_t status;              /* STATUS_* constant above                 */
    char    destination[31];     /* max 30 chars + '\0'                     */
    uint8_t cabin_class;         /* CABIN_* constant above                  */
    int     seat_num;            /* 1 – 300                                 */
    char    app_ver[16];         /* "vX.Y.Z\0"                              */
    char    passenger_name[128]; /* UTF-8, Turkish / non-ASCII supported    */
} PassengerRecord;

/**
 * write_binary – write `count` PassengerRecord structs to `filename`.
 *
 * Returns  0 on success, -1 on error (with perror message).
 */
int write_binary(const char *filename, const PassengerRecord *records, int count);

#endif /* BINARY_WRITER_H */
