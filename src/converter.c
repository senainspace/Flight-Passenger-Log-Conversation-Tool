#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "converter.h"

/* =========================================================================
 * CSV READER
 * ========================================================================= */

static const unsigned char EMOJI_BOARDED[]   = { 0xF0, 0x9F, 0x9F, 0xA2 }; /* 🟢 */
static const unsigned char EMOJI_CANCELLED[] = { 0xF0, 0x9F, 0x94, 0xB4 }; /* 🔴 */
static const unsigned char EMOJI_DELAYED[]   = { 0xE2, 0x9A, 0xA0, 0xEF, 0xB8, 0x8F }; /* ⚠️ */

static int validate_ticket_id(const char *id)
{
    if (strlen(id) != 7) return 0;
    for (int i = 0; i < 3; i++)
        if (!isupper((unsigned char)id[i])) return 0;
    for (int i = 3; i < 7; i++)
        if (!isdigit((unsigned char)id[i])) return 0;
    return 1;
}

static int validate_app_ver(const char *ver)
{
    if (!ver || ver[0] != 'v') return 0;
    int x, y, z;
    return (sscanf(ver + 1, "%d.%d.%d", &x, &y, &z) == 3);
}

static void strip_cr(char *s)
{
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\r')
        s[len - 1] = '\0';
}

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

static int parse_status(const char *field, uint8_t *out)
{
    const unsigned char *f = (const unsigned char *)field;
    if (memcmp(f, EMOJI_BOARDED,   4) == 0) { *out = STATUS_BOARDED;   return 1; }
    if (memcmp(f, EMOJI_CANCELLED, 4) == 0) { *out = STATUS_CANCELLED; return 1; }
    if (memcmp(f, EMOJI_DELAYED,   6) == 0) { *out = STATUS_DELAYED;   return 1; }
    return 0;
}

static int parse_cabin(const char *field, uint8_t *out)
{
    if (strcmp(field, "ECONOMY")  == 0) { *out = CABIN_ECONOMY;  return 1; }
    if (strcmp(field, "BUSINESS") == 0) { *out = CABIN_BUSINESS; return 1; }
    if (strcmp(field, "FIRST")    == 0) { *out = CABIN_FIRST;    return 1; }
    return 0;
}

PassengerRecord *read_csv(const char *filename, int sep_type,
                          int opsys_type, int *out_count)
{
    (void)opsys_type;

    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror(filename); return NULL; }

    char sep;
    switch (sep_type) {
        case SEP_TAB:       sep = '\t'; break;
        case SEP_SEMICOLON: sep = ';';  break;
        default:            sep = ',';  break;
    }

    int capacity = 64, count = 0;
    PassengerRecord *records = malloc(capacity * sizeof(PassengerRecord));
    if (!records) { fclose(fp); return NULL; }

    char line[2048];
    int  line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        strip_cr(line);
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len == 0 || line_num == 1) continue; /* skip empty & header */

        char *fields[12];
        int nf = split_line(line, sep, fields, 12);
        if (nf < 10) {
            fprintf(stderr, "Warning: line %d has only %d field(s), skipping.\n",
                    line_num, nf);
            continue;
        }

        /* Mandatory: ticket_id */
        if (!validate_ticket_id(fields[0])) {
            fprintf(stderr, "Warning: line %d – invalid ticket_id '%s', skipping.\n",
                    line_num, fields[0]);
            continue;
        }

        /* Mandatory: seat_num */
        int seat_num = atoi(fields[7]);
        if (seat_num < 1 || seat_num > 300) {
            fprintf(stderr, "Warning: line %d – seat_num '%s' out of range, skipping.\n",
                    line_num, fields[7]);
            continue;
        }

        /* Optional validations with clamping */
        float baggage = (float)atof(fields[2]);
        if (baggage < 0.0f || baggage > 50.0f) {
            fprintf(stderr, "Warning: line %d – baggage_weight %.2f out of range, clamping.\n",
                    line_num, (double)baggage);
            if (baggage < 0.0f)  baggage = 0.0f;
            if (baggage > 50.0f) baggage = 50.0f;
        }

        int loyalty = atoi(fields[3]);
        if (loyalty < 0 || loyalty > 10000) {
            fprintf(stderr, "Warning: line %d – loyalty_points %d out of range, clamping.\n",
                    line_num, loyalty);
            if (loyalty < 0)     loyalty = 0;
            if (loyalty > 10000) loyalty = 10000;
        }

        uint8_t status;
        if (!parse_status(fields[4], &status)) {
            fprintf(stderr, "Warning: line %d – unknown status, defaulting to DELAYED.\n",
                    line_num);
            status = STATUS_DELAYED;
        }

        uint8_t cabin;
        if (!parse_cabin(fields[6], &cabin)) {
            fprintf(stderr, "Warning: line %d – unknown cabin_class '%s', defaulting to ECONOMY.\n",
                    line_num, fields[6]);
            cabin = CABIN_ECONOMY;
        }

        if (!validate_app_ver(fields[8]))
            fprintf(stderr, "Warning: line %d – app_ver '%s' does not match vX.Y.Z.\n",
                    line_num, fields[8]);

        if (count >= capacity) {
            capacity *= 2;
            PassengerRecord *tmp = realloc(records, capacity * sizeof(PassengerRecord));
            if (!tmp) { free(records); fclose(fp); return NULL; }
            records = tmp;
        }

        PassengerRecord *r = &records[count++];
        memset(r, 0, sizeof(*r));
        strncpy(r->ticket_id,      fields[0], sizeof(r->ticket_id)      - 1);
        strncpy(r->timestamp,      fields[1], sizeof(r->timestamp)      - 1);
        r->baggage_weight = baggage;
        r->loyalty_points = loyalty;
        r->status         = status;
        strncpy(r->destination,    fields[5], sizeof(r->destination)    - 1);
        r->cabin_class    = cabin;
        r->seat_num       = seat_num;
        strncpy(r->app_ver,        fields[8], sizeof(r->app_ver)        - 1);
        strncpy(r->passenger_name, fields[9], sizeof(r->passenger_name) - 1);
    }

    fclose(fp);
    *out_count = count;
    return records;
}

/* =========================================================================
 * BINARY WRITER
 * ========================================================================= */

int write_binary(const char *filename,
                 const PassengerRecord *records, int count)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) { perror(filename); return -1; }

    for (int i = 0; i < count; i++) {
        if (fwrite(&records[i], sizeof(PassengerRecord), 1, fp) != 1) {
            perror("fwrite");
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

/* =========================================================================
 * BINARY READER
 * ========================================================================= */

PassengerRecord *read_binary(const char *filename, int *out_count)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror(filename); return NULL; }

    if (fseek(fp, 0, SEEK_END) != 0) { perror("fseek"); fclose(fp); return NULL; }
    long file_size = ftell(fp);
    if (file_size < 0) { perror("ftell"); fclose(fp); return NULL; }
    rewind(fp);

    if (file_size % (long)sizeof(PassengerRecord) != 0) {
        fprintf(stderr,
            "Error: '%s' size (%ld bytes) is not a multiple of record size (%zu bytes).\n",
            filename, file_size, sizeof(PassengerRecord));
        fclose(fp);
        return NULL;
    }

    int count = (int)(file_size / (long)sizeof(PassengerRecord));
    if (count == 0) { fclose(fp); *out_count = 0; return NULL; }

    PassengerRecord *records = malloc((size_t)count * sizeof(PassengerRecord));
    if (!records) { perror("malloc"); fclose(fp); return NULL; }

    if ((int)fread(records, sizeof(PassengerRecord), (size_t)count, fp) != count) {
        perror("fread");
        free(records);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    *out_count = count;
    return records;
}
