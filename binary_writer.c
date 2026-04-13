#include <stdio.h>
#include "binary_writer.h"

int write_binary(const char *filename, const PassengerRecord *records, int count)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror(filename);
        return -1;
    }

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
