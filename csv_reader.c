#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "csv_reader.h"

/* ── Emoji UTF-8 byte sequences ──────────────────────────────────────── */
/* 🟢 U+1F7E2  → F0 9F 9F A2  (4 bytes) */
static const unsigned char EMOJI_BOARDED[]   = { 0xF0, 0x9F, 0x9F, 0xA2 };
/* 🔴 U+1F534  → F0 9F 94 B4  (4 bytes) */
static const unsigned char EMOJI_CANCELLED[] = { 0xF0, 0x9F, 0x94, 0xB4 };
/* ⚠️ U+26A0 U+FE0F → E2 9A A0 EF B8 8F  (6 bytes) */
static const unsigned char EMOJI_DELAYED[]   = { 0xE2, 0x9A, 0xA0, 0xEF, 0xB8, 0x8F };

/* ── ticket_id validation ─────────────────────────────────────────────── */
static int validate_ticket_id(const char *id)
{
    if (strlen(id) != 7) return 0;
    for (int i = 0; i < 3; i++)
        if (!isupper((unsigned char)id[i])) return 0;
    for (int i = 3; i < 7; i++)
        if (!isdigit((unsigned char)id[i])) return 0;
    return 1;
}

/* ── app_ver validation  (vX.Y.Z) ─────────────────────────────────────── */
static int validate_app_ver(const char *ver)
{
    if (ver[0] != 'v') return 0;
    int x, y, z;
    return (sscanf(ver + 1, "%d.%d.%d", &x, &y, &z) == 3);
}

/* ── Strip trailing \r (in-place) ─────────────────────────────────────── */
static void strip_cr(char *s)
{
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\r')
        s[len - 1] = '\0';
}

/* ── Split a line into fields by separator ───────────────────────────── */
/*
 * Fills fields[0..9].  Each pointer points into `line` (modified in-place).
 * Returns the number of fields found.
 *
 * NOTE: The status field may contain multi-byte UTF-8 emoji, so we cannot
 * use strchr on the separator alone when the separator byte could appear
 * inside a multi-byte sequence.  Since our separators are all ASCII (< 0x80)
 * and UTF-8 continuation bytes are always >= 0x80, a simple byte scan is safe.
 */
static int split_line(char *line, char sep, char *fields[], int max_fields)
{
    int count = 0;
    fields[count++] = line;

    for (char *p = line; *p && count < max_fields; p++) {
        if ((unsigned char)*p == (unsigned char)sep) {
            *p = '\0';
            fields[count++] = p + 1;
        }
    }
    return count;
}

/* ── Parse emoji status field → STATUS_* constant ───────────────────── */
static int parse_status(const char *field, uint8_t *out_status)
{
    const unsigned char *f = (const unsigned char *)field;

    if (memcmp(f, EMOJI_BOARDED, 4) == 0) {
        *out_status = STATUS_BOARDED;
        return 1;
    }
    if (memcmp(f, EMOJI_CANCELLED, 4) == 0) {
        *out_status = STATUS_CANCELLED;
        return 1;
    }
    if (memcmp(f, EMOJI_DELAYED, 6) == 0) {
        *out_status = STATUS_DELAYED;
        return 1;
    }
    return 0; /* unknown status */
}

/* ── Parse cabin_class string → CABIN_* constant ─────────────────────── */
static int parse_cabin_class(const char *field, uint8_t *out_class)
{
    if (strcmp(field, "ECONOMY")  == 0) { *out_class = CABIN_ECONOMY;  return 1; }
    if (strcmp(field, "BUSINESS") == 0) { *out_class = CABIN_BUSINESS; return 1; }
    if (strcmp(field, "FIRST")    == 0) { *out_class = CABIN_FIRST;    return 1; }
    return 0;
}

/* ── Public API ───────────────────────────────────────────────────────── */

PassengerRecord *read_csv(const char *filename,
                          int         sep_type,
                          int         opsys_type,
                          int        *out_count)
{
    (void)opsys_type; /* line endings handled below */

    FILE *fp = fopen(filename, "rb"); /* binary mode to preserve raw bytes */
    if (!fp) {
        perror(filename);
        return NULL;
    }

    /* Determine separator character */
    char sep;
    switch (sep_type) {
        case SEP_TAB:       sep = '\t'; break;
        case SEP_SEMICOLON: sep = ';';  break;
        default:            sep = ',';  break; /* SEP_COMMA */
    }

    /* Capacity-doubling record array */
    int capacity = 64;
    int count    = 0;
    PassengerRecord *records = malloc(capacity * sizeof(PassengerRecord));
    if (!records) { fclose(fp); return NULL; }

    char line[2048];
    int  line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        strip_cr(line); /* remove Windows \r if present */

        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';

        /* Skip empty lines */
        if (len == 0) continue;

        /* Skip header line */
        if (line_num == 1) continue;

        /* Split into fields */
        char *fields[12];
        int nf = split_line(line, sep, fields, 12);

        if (nf < 10) {
            fprintf(stderr, "Warning: line %d has only %d field(s), skipping.\n",
                    line_num, nf);
            continue;
        }

        /* Field order:
         * 0  ticket_id
         * 1  timestamp
         * 2  baggage_weight
         * 3  loyalty_points
         * 4  status  (emoji)
         * 5  destination
         * 6  cabin_class
         * 7  seat_num
         * 8  app_ver
         * 9  passenger_name
         */

        /* ── Mandatory: ticket_id ─────────────────────────────── */
        if (!validate_ticket_id(fields[0])) {
            fprintf(stderr,
                "Warning: line %d – invalid ticket_id '%s', skipping.\n",
                line_num, fields[0]);
            continue;
        }

        /* ── Mandatory: seat_num ──────────────────────────────── */
        int seat_num = atoi(fields[7]);
        if (seat_num < 1 || seat_num > 300) {
            fprintf(stderr,
                "Warning: line %d – seat_num '%s' out of range (1-300), skipping.\n",
                line_num, fields[7]);
            continue;
        }

        /* ── Optional field validations ──────────────────────── */
        float baggage_weight = (float)atof(fields[2]);
        if (baggage_weight < 0.0f || baggage_weight > 50.0f) {
            fprintf(stderr,
                "Warning: line %d – baggage_weight %.2f out of range, clamping.\n",
                line_num, (double)baggage_weight);
            if (baggage_weight < 0.0f) baggage_weight = 0.0f;
            if (baggage_weight > 50.0f) baggage_weight = 50.0f;
        }

        int loyalty_points = atoi(fields[3]);
        if (loyalty_points < 0 || loyalty_points > 10000) {
            fprintf(stderr,
                "Warning: line %d – loyalty_points %d out of range, clamping.\n",
                line_num, loyalty_points);
            if (loyalty_points < 0)     loyalty_points = 0;
            if (loyalty_points > 10000) loyalty_points = 10000;
        }

        uint8_t status;
        if (!parse_status(fields[4], &status)) {
            fprintf(stderr,
                "Warning: line %d – unknown status, defaulting to DELAYED.\n",
                line_num);
            status = STATUS_DELAYED;
        }

        uint8_t cabin_class;
        if (!parse_cabin_class(fields[6], &cabin_class)) {
            fprintf(stderr,
                "Warning: line %d – unknown cabin_class '%s', defaulting to ECONOMY.\n",
                line_num, fields[6]);
            cabin_class = CABIN_ECONOMY;
        }

        if (!validate_app_ver(fields[8])) {
            fprintf(stderr,
                "Warning: line %d – app_ver '%s' does not match vX.Y.Z.\n",
                line_num, fields[8]);
        }

        /* ── Grow array if needed ────────────────────────────── */
        if (count >= capacity) {
            capacity *= 2;
            PassengerRecord *tmp = realloc(records, capacity * sizeof(PassengerRecord));
            if (!tmp) { free(records); fclose(fp); return NULL; }
            records = tmp;
        }

        /* ── Populate record ─────────────────────────────────── */
        PassengerRecord *r = &records[count++];
        memset(r, 0, sizeof(*r));

        strncpy(r->ticket_id,       fields[0], sizeof(r->ticket_id)       - 1);
        strncpy(r->timestamp,       fields[1], sizeof(r->timestamp)       - 1);
        r->baggage_weight  = baggage_weight;
        r->loyalty_points  = loyalty_points;
        r->status          = status;
        strncpy(r->destination,     fields[5], sizeof(r->destination)     - 1);
        r->cabin_class     = cabin_class;
        r->seat_num        = seat_num;
        strncpy(r->app_ver,         fields[8], sizeof(r->app_ver)         - 1);
        strncpy(r->passenger_name,  fields[9], sizeof(r->passenger_name)  - 1);
    }

    fclose(fp);
    *out_count = count;
    return records;
}
