#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "xml_encoding.h"

/* ═══════════════════════════════════════════════════════════════════════
 * UTF-8 / UTF-16 helper utilities
 * ═══════════════════════════════════════════════════════════════════════ */

int utf8_char_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if (c < 0xC0) return -1; /* continuation byte – invalid as leading byte */
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    if (c < 0xF8) return 4;
    return -1;
}

unsigned int utf8_decode(const unsigned char *s, int len)
{
    switch (len) {
        case 1: return s[0];
        case 2: return ((s[0] & 0x1F) << 6)  | (s[1] & 0x3F);
        case 3: return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        case 4: return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12)
                     | ((s[2] & 0x3F) << 6)  | (s[3] & 0x3F);
        default: return 0xFFFD; /* replacement character */
    }
}

/* ── Write one Unicode code-point as UTF-16 LE into `buf`.
 *   Returns the number of bytes written (2 or 4). */
static int cp_to_utf16le(unsigned int cp, unsigned char *buf)
{
    if (cp < 0x10000) {
        buf[0] = cp & 0xFF;
        buf[1] = (cp >> 8) & 0xFF;
        return 2;
    }
    /* Surrogate pair */
    cp -= 0x10000;
    unsigned int high = 0xD800 | (cp >> 10);
    unsigned int low  = 0xDC00 | (cp & 0x3FF);
    buf[0] = high & 0xFF;        buf[1] = (high >> 8) & 0xFF;
    buf[2] = low  & 0xFF;        buf[3] = (low  >> 8) & 0xFF;
    return 4;
}

/* ── Write one Unicode code-point as UTF-16 BE into `buf`.
 *   Returns the number of bytes written (2 or 4). */
static int cp_to_utf16be(unsigned int cp, unsigned char *buf)
{
    if (cp < 0x10000) {
        buf[0] = (cp >> 8) & 0xFF;
        buf[1] = cp & 0xFF;
        return 2;
    }
    cp -= 0x10000;
    unsigned int high = 0xD800 | (cp >> 10);
    unsigned int low  = 0xDC00 | (cp & 0x3FF);
    buf[0] = (high >> 8) & 0xFF; buf[1] = high & 0xFF;
    buf[2] = (low  >> 8) & 0xFF; buf[3] = low  & 0xFF;
    return 4;
}

/* ── Convert a UTF-8 buffer to a UTF-16 LE / BE file (with BOM).
 *   big_endian == 0  → LE (BOM FF FE)
 *   big_endian != 0  → BE (BOM FE FF)
 */
static int write_utf8_as_utf16(FILE *out,
                                const char *utf8_data,
                                size_t len,
                                int big_endian)
{
    /* BOM */
    if (big_endian) {
        fputc(0xFE, out); fputc(0xFF, out);
    } else {
        fputc(0xFF, out); fputc(0xFE, out);
    }

    const unsigned char *p   = (const unsigned char *)utf8_data;
    const unsigned char *end = p + len;

    while (p < end) {
        int clen = utf8_char_len(*p);
        if (clen < 0 || p + clen > end) {
            /* Skip invalid byte */
            p++;
            continue;
        }
        unsigned int cp = utf8_decode(p, clen);
        p += clen;

        unsigned char utf16buf[4];
        int nbytes = big_endian ? cp_to_utf16be(cp, utf16buf)
                                : cp_to_utf16le(cp, utf16buf);
        fwrite(utf16buf, 1, (size_t)nbytes, out);
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * first_char_hex helpers for UTF-16
 * ═══════════════════════════════════════════════════════════════════════ */

void compute_first_char_hex_utf16le(const char *utf8_str, char *hex_out)
{
    hex_out[0] = '\0';
    if (!utf8_str || !utf8_str[0]) { strcpy(hex_out, "0000"); return; }

    int clen = utf8_char_len((unsigned char)utf8_str[0]);
    if (clen < 0) clen = 1;
    unsigned int cp = utf8_decode((const unsigned char *)utf8_str, clen);

    unsigned char buf[4];
    int nbytes = cp_to_utf16le(cp, buf);
    for (int i = 0; i < nbytes; i++) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%02X", buf[i]);
        strcat(hex_out, tmp);
    }
}

void compute_first_char_hex_utf16be(const char *utf8_str, char *hex_out)
{
    hex_out[0] = '\0';
    if (!utf8_str || !utf8_str[0]) { strcpy(hex_out, "0000"); return; }

    int clen = utf8_char_len((unsigned char)utf8_str[0]);
    if (clen < 0) clen = 1;
    unsigned int cp = utf8_decode((const unsigned char *)utf8_str, clen);

    unsigned char buf[4];
    int nbytes = cp_to_utf16be(cp, buf);
    for (int i = 0; i < nbytes; i++) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%02X", buf[i]);
        strcat(hex_out, tmp);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Update passenger_name attributes in the libxml2 tree
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * Walk the document tree, find all <passenger_name> elements, and update
 * their current_encoding and first_char_hex attributes.
 *
 * `encoding_label`  – e.g. "UTF-16-LE", "UTF-16-BE", "UTF-8"
 * `hex_fn`          – function pointer that computes first_char_hex given a
 *                     UTF-8 string (libxml2 stores content as UTF-8 internally)
 */
typedef void (*HexFn)(const char *, char *);

static void update_passenger_name_attrs(xmlNodePtr node,
                                        const char *encoding_label,
                                        HexFn       hex_fn)
{
    for (xmlNodePtr cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE &&
            xmlStrcmp(cur->name, BAD_CAST "passenger_name") == 0)
        {
            /* Get text content (UTF-8) */
            xmlChar *content = xmlNodeGetContent(cur);
            char hex_buf[16] = "00";
            if (content) {
                hex_fn((const char *)content, hex_buf);
                xmlFree(content);
            }
            xmlSetProp(cur, BAD_CAST "current_encoding",
                       BAD_CAST encoding_label);
            xmlSetProp(cur, BAD_CAST "first_char_hex",
                       BAD_CAST hex_buf);
        }
        /* Recurse into children */
        update_passenger_name_attrs(cur->children, encoding_label, hex_fn);
    }
}

/* ── UTF-8 first_char_hex (defined in xml_writer.c, declared in xml_writer.h,
 *    but we need it here too; redeclare as static to avoid the dependency) */
static void first_char_hex_utf8_local(const char *utf8_str, char *hex_out)
{
    hex_out[0] = '\0';
    if (!utf8_str || !utf8_str[0]) { strcpy(hex_out, "00"); return; }
    int len = utf8_char_len((unsigned char)utf8_str[0]);
    if (len < 0) len = 1;
    for (int i = 0; i < len; i++) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%02X", (unsigned char)utf8_str[i]);
        strcat(hex_out, tmp);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Replace "encoding=..." in the XML declaration inside a char buffer
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * Returns a newly malloc'd copy of `buf` with the encoding attribute in
 * the XML declaration replaced by `new_enc`.
 * Caller must free() the result.
 */
static char *replace_encoding_decl(const char *buf, const char *new_enc)
{
    /* Locate the XML declaration */
    const char *decl_start = strstr(buf, "<?xml");
    if (!decl_start) {
        /* No declaration – prepend one */
        size_t prefix_len = strlen("<?xml version=\"1.0\" encoding=\"\"?>\n");
        size_t total = prefix_len + strlen(new_enc) + strlen(buf) + 1;
        char *out = malloc(total);
        if (!out) return NULL;
        snprintf(out, total, "<?xml version=\"1.0\" encoding=\"%s\"?>\n%s",
                 new_enc, buf);
        return out;
    }

    const char *decl_end = strstr(decl_start, "?>");
    if (!decl_end) return strdup(buf);
    decl_end += 2; /* include "?>" */

    /* Build new declaration */
    char new_decl[128];
    snprintf(new_decl, sizeof(new_decl),
             "<?xml version=\"1.0\" encoding=\"%s\"?>", new_enc);

    size_t before_len = (size_t)(decl_start - buf);
    size_t after_len  = strlen(decl_end);
    size_t new_len    = before_len + strlen(new_decl) + after_len + 1;

    char *out = malloc(new_len);
    if (!out) return NULL;
    memcpy(out, buf, before_len);
    strcpy(out + before_len, new_decl);
    memcpy(out + before_len + strlen(new_decl), decl_end, after_len + 1);
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════ */

int convert_xml_encoding(const char *input_file,
                         const char *output_file,
                         int         encoding_type)
{
    /* libxml2 auto-detects encoding from BOM / XML declaration */
    xmlDocPtr doc = xmlReadFile(input_file, NULL,
                                XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) {
        fprintf(stderr, "Error: cannot parse XML file '%s'\n", input_file);
        return -1;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        fprintf(stderr, "Error: empty XML document '%s'\n", input_file);
        xmlFreeDoc(doc);
        return -1;
    }

    int rc = 0;

    if (encoding_type == ENCODING_TO_UTF8) {
        /* ── UTF-16 → UTF-8 ────────────────────────────────────────── */
        update_passenger_name_attrs(root, "UTF-8",
                                    first_char_hex_utf8_local);
        rc = xmlSaveFormatFileEnc(output_file, doc, "UTF-8", 1);
        rc = (rc > 0) ? 0 : -1;

    } else {
        /* ── UTF-8 → UTF-16 LE or BE ───────────────────────────────── */
        int big_endian = (encoding_type == ENCODING_UTF16BE);
        const char *enc_label = big_endian ? "UTF-16-BE" : "UTF-16-LE";
        HexFn hex_fn = big_endian ? compute_first_char_hex_utf16be
                                  : compute_first_char_hex_utf16le;

        update_passenger_name_attrs(root, enc_label, hex_fn);

        /* Dump the modified document to a UTF-8 memory buffer */
        xmlChar *utf8_buf = NULL;
        int      utf8_len = 0;
        xmlDocDumpMemoryEnc(doc, &utf8_buf, &utf8_len, "UTF-8");

        if (!utf8_buf) {
            fprintf(stderr, "Error: failed to dump XML document to memory\n");
            xmlFreeDoc(doc);
            return -1;
        }

        /* Replace encoding="UTF-8" with encoding="UTF-16" in the declaration */
        char *modified = replace_encoding_decl((const char *)utf8_buf, "UTF-16");
        xmlFree(utf8_buf);

        if (!modified) {
            fprintf(stderr, "Error: out of memory during encoding replacement\n");
            xmlFreeDoc(doc);
            return -1;
        }

        /* Write as UTF-16 LE/BE with BOM */
        FILE *out = fopen(output_file, "wb");
        if (!out) {
            perror(output_file);
            free(modified);
            xmlFreeDoc(doc);
            return -1;
        }

        rc = write_utf8_as_utf16(out, modified, strlen(modified), big_endian);
        fclose(out);
        free(modified);
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return rc;
}
