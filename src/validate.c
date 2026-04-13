#include <stdio.h>
#include <libxml/xmlschemastypes.h>

/*
 * validate_xml_with_xsd – validate xml_file against xsd_file using libxml2.
 *
 * Returns  0  if the document validates successfully,
 *         >0  (positive error count) if validation fails,
 *         -1  on a fatal error (cannot open/parse either file).
 */
int validate_xml_with_xsd(const char *xml_file, const char *xsd_file)
{
    xmlDocPtr doc;
    xmlSchemaPtr schema = NULL;
    xmlSchemaParserCtxtPtr pctxt;

    xmlLineNumbersDefault(1);

    pctxt  = xmlSchemaNewParserCtxt(xsd_file);
    schema = xmlSchemaParse(pctxt);
    xmlSchemaFreeParserCtxt(pctxt);

    if (!schema) {
        fprintf(stderr, "Could not parse XSD schema: %s\n", xsd_file);
        return -1;
    }

    doc = xmlReadFile(xml_file, NULL, 0);
    if (!doc) {
        fprintf(stderr, "Could not parse XML: %s\n", xml_file);
        xmlSchemaFree(schema);
        return -1;
    }

    xmlSchemaValidCtxtPtr vctxt;
    int ret;

    vctxt = xmlSchemaNewValidCtxt(schema);
    ret   = xmlSchemaValidateDoc(vctxt, doc);

    if (ret == 0)
        printf("%s validates\n", xml_file);
    else if (ret > 0)
        printf("%s fails to validate\n", xml_file);
    else
        printf("%s validation generated an internal error\n", xml_file);

    xmlSchemaFreeValidCtxt(vctxt);
    xmlFreeDoc(doc);

    if (schema != NULL)
        xmlSchemaFree(schema);

    xmlSchemaCleanupTypes();
    xmlCleanupParser();

    return ret;
}
