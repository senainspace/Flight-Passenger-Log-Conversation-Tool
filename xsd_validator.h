#ifndef XSD_VALIDATOR_H
#define XSD_VALIDATOR_H

/**
 * validate_xml_file – validate `xml_file` against `xsd_file` using libxml2.
 *
 * Internally calls the validate_xml_with_xsd() function provided in
 * validate.c (supplied by the instructor).
 *
 * @param xml_file  path to the XML document to validate
 * @param xsd_file  path to the XSD schema file
 *
 * Returns  0 if validation succeeds,
 *         >0 if validation fails (number of errors),
 *         -1 on fatal error (file not found, parse error, etc.).
 */
int validate_xml_file(const char *xml_file, const char *xsd_file);

#endif /* XSD_VALIDATOR_H */
