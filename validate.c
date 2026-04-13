/*
 * validate.c – XSD validation helper using libxml2.
 *
 * NOTE: This file is a placeholder.  The instructor will supply the
 * authoritative version.  Replace this file with the version provided
 * during the assignment and recompile.
 *
 * The function signature below must remain compatible with the one the
 * instructor delivers so that xsd_validator.c can call it unchanged.
 */

#include <stdio.h>
#include <libxml/xmlschemas.h>
#include <libxml/parser.h>

/**
 * validate_xml_with_xsd – validate `xml_file` against `xsd_file`.
 *
 * Returns  0  if the document is valid,
 *         >0  number of validation errors,
 *         -1  on fatal / parse errors.
 */
int validate_xml_with_xsd(const char *xml_file, const char *xsd_file)
{
    int result = 0;

    /* ── Parse the XSD schema ─────────────────────────────────────── */
    xmlSchemaParserCtxtPtr schema_parser =
        xmlSchemaNewParserCtxt(xsd_file);
    if (!schema_parser) {
        fprintf(stderr, "validate.c: cannot create schema parser for '%s'\n",
                xsd_file);
        return -1;
    }

    xmlSchemaPtr schema = xmlSchemaParse(schema_parser);
    xmlSchemaFreeParserCtxt(schema_parser);

    if (!schema) {
        fprintf(stderr, "validate.c: failed to parse XSD schema '%s'\n",
                xsd_file);
        return -1;
    }

    /* ── Parse the XML document ───────────────────────────────────── */
    xmlDocPtr doc = xmlReadFile(xml_file, NULL, 0);
    if (!doc) {
        fprintf(stderr, "validate.c: cannot parse XML file '%s'\n", xml_file);
        xmlSchemaFree(schema);
        return -1;
    }

    /* ── Validate ─────────────────────────────────────────────────── */
    xmlSchemaValidCtxtPtr valid_ctx = xmlSchemaNewValidCtxt(schema);
    if (!valid_ctx) {
        fprintf(stderr, "validate.c: cannot create validation context\n");
        xmlFreeDoc(doc);
        xmlSchemaFree(schema);
        return -1;
    }

    result = xmlSchemaValidateDoc(valid_ctx, doc);

    if (result == 0) {
        printf("validate.c: '%s' is valid.\n", xml_file);
    } else if (result > 0) {
        fprintf(stderr, "validate.c: '%s' failed validation (%d error(s)).\n",
                xml_file, result);
    } else {
        fprintf(stderr, "validate.c: internal validation error.\n");
    }

    /* ── Clean up ─────────────────────────────────────────────────── */
    xmlSchemaFreeValidCtxt(valid_ctx);
    xmlFreeDoc(doc);
    xmlSchemaFree(schema);
    xmlCleanupParser();

    return result;
}
