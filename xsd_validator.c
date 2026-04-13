#include <stdio.h>
#include "xsd_validator.h"

/* Forward declaration of the function provided by validate.c */
int validate_xml_with_xsd(const char *xml_file, const char *xsd_file);

int validate_xml_file(const char *xml_file, const char *xsd_file)
{
    if (!xml_file || !xsd_file) {
        fprintf(stderr, "xsd_validator: NULL file path argument\n");
        return -1;
    }
    return validate_xml_with_xsd(xml_file, xsd_file);
}
