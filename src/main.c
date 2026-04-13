#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "converter.h"
#include "xml.h"
#include "validate.h"

static void print_help(const char *prog);
static void die(const char *msg);

int main(int argc, char *argv[])
{
    if (argc < 4) { print_help(argv[0]); return EXIT_FAILURE; }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) { print_help(argv[0]); return EXIT_SUCCESS; }
    }

    const char *input_file  = argv[1];
    const char *output_file = argv[2];
    int         conv_type   = atoi(argv[3]);

    if (conv_type < 1 || conv_type > 4) {
        fprintf(stderr, "Error: conversion_type must be 1, 2, 3, or 4.\n");
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    int sep_type = 0, opsys_type = 0, encoding_type = 0;

    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-separator") == 0) {
            if (i + 1 >= argc) die("Missing value for -separator");
            sep_type = atoi(argv[++i]);
            if (sep_type < 1 || sep_type > 3) die("Invalid -separator (1-3)");
        } else if (strcmp(argv[i], "-opsys") == 0) {
            if (i + 1 >= argc) die("Missing value for -opsys");
            opsys_type = atoi(argv[++i]);
            if (opsys_type < 1 || opsys_type > 3) die("Invalid -opsys (1-3)");
        } else if (strcmp(argv[i], "-encoding") == 0) {
            if (i + 1 >= argc) die("Missing value for -encoding");
            encoding_type = atoi(argv[++i]);
            if (encoding_type < 1 || encoding_type > 3) die("Invalid -encoding (1-3)");
        } else {
            fprintf(stderr, "Warning: unknown argument '%s'\n", argv[i]);
        }
    }

    if (sep_type == 0)   die("-separator is required");
    if (opsys_type == 0) die("-opsys is required");
    if (conv_type == 4 && encoding_type == 0)
        die("-encoding is required for conversion type 4");

    int ret = 0;

    switch (conv_type) {

        case 1: {
            printf("Converting CSV '%s' to binary '%s' ...\n",
                   input_file, output_file);
            int count = 0;
            PassengerRecord *records = read_csv(input_file, sep_type,
                                                opsys_type, &count);
            if (!records) {
                fprintf(stderr, "Error: failed to read '%s'\n", input_file);
                return EXIT_FAILURE;
            }
            printf("Parsed %d valid record(s).\n", count);
            ret = write_binary(output_file, records, count);
            free(records);
            if (ret == 0) printf("Binary file written to '%s'.\n", output_file);
            break;
        }

        case 2: {
            printf("Converting binary '%s' to XML '%s' ...\n",
                   input_file, output_file);
            int count = 0;
            PassengerRecord *records = read_binary(input_file, &count);
            if (!records) {
                fprintf(stderr, "Error: failed to read '%s'\n", input_file);
                return EXIT_FAILURE;
            }
            printf("Read %d record(s).\n", count);
            ret = write_xml(output_file, records, count);
            free(records);
            if (ret == 0) printf("XML file written to '%s'.\n", output_file);
            break;
        }

        case 3: {
            printf("Validating '%s' against '%s' ...\n",
                   input_file, output_file);
            ret = validate_xml_with_xsd(input_file, output_file);
            if (ret == 0) printf("Validation PASSED.\n");
            else          printf("Validation FAILED (%d error(s)).\n", ret);
            break;
        }

        case 4: {
            const char *names[] = { "", "UTF-16 LE", "UTF-16 BE", "UTF-8" };
            printf("Converting '%s' to %s, writing '%s' ...\n",
                   input_file, names[encoding_type], output_file);
            ret = convert_xml_encoding(input_file, output_file, encoding_type);
            if (ret == 0) printf("Done: '%s'.\n", output_file);
            break;
        }
    }

    return (ret == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void print_help(const char *prog)
{
    printf(
        "Usage:\n"
        "  %s <input> <output> <conv_type> -separator <1|2|3>"
        " -opsys <1|2|3> [-encoding <1|2|3>] [-h]\n\n"
        "conv_type:\n"
        "  1  CSV  -> Binary\n"
        "  2  Binary -> XML (UTF-8)\n"
        "  3  Validate XML against XSD\n"
        "  4  XML encoding conversion\n\n"
        "-separator:  1=comma  2=tab  3=semicolon\n"
        "-opsys:      1=Windows(\\r\\n)  2=Linux(\\n)  3=macOS(\\n)\n"
        "-encoding:   1=UTF-16LE  2=UTF-16BE  3=UTF-16->UTF-8\n\n"
        "Examples:\n"
        "  %s flightlog.csv      flightdata.dat        1 -separator 1 -opsys 2\n"
        "  %s flightdata.dat     flightlogs_utf8.xml   2 -separator 1 -opsys 2\n"
        "  %s flightlogs_utf8.xml flightlogs_utf16le.xml 4 -separator 1 -opsys 2 -encoding 1\n"
        "  %s flightlogs_utf8.xml flightlogs.xsd       3 -separator 1 -opsys 2\n",
        prog, prog, prog, prog, prog);
}

static void die(const char *msg)
{
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}
