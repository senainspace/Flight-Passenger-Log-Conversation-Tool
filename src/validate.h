#ifndef VALIDATE_H
#define VALIDATE_H

/* Validate xml_file against xsd_file using libxml2.
   Returns 0 on success, >0 if validation fails, -1 on fatal error. */
int validate_xml_with_xsd(const char *xml_file, const char *xsd_file);

#endif /* VALIDATE_H */
