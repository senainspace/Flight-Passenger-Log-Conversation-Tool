#include <stdio.h>
#include <stdlib.h>
#include "binary_reader.h"

PassengerRecord *read_binary(const char *filename, int *out_count)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror(filename);
        return NULL;
    }

    /* Determine file size to calculate record count */
    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return NULL;
    }
    long file_size = ftell(fp);
    if (file_size < 0) {
        perror("ftell");
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    if (file_size % (long)sizeof(PassengerRecord) != 0) {
        fprintf(stderr,
            "Error: '%s' size (%ld bytes) is not a multiple of PassengerRecord"
            " size (%zu bytes). File may be corrupt.\n",
            filename, file_size, sizeof(PassengerRecord));
        fclose(fp);
        return NULL;
    }

    int count = (int)(file_size / (long)sizeof(PassengerRecord));
    if (count == 0) {
        fclose(fp);
        *out_count = 0;
        return NULL;
    }

    PassengerRecord *records = malloc((size_t)count * sizeof(PassengerRecord));
    if (!records) {
        perror("malloc");
        fclose(fp);
        return NULL;
    }

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
