#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "xml.h"

/* =========================================================================
 * UTF-8 / UTF-16 helpers
 * ========================================================================= */

static int utf8_char_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if (c < 0xC0) return -1;
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    if (c < 0xF8) return 4;
    return -1;
}

static unsigned int utf8_decode(const unsigned char *s, int len)
{
    switch (len) {
        case 1: return s[0];
        case 2: return ((s[0] & 0x1F) << 6)  | (s[1] & 0x3F);
        case 3: return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        case 4: return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12)
                     | ((s[2] & 0x3F) << 6)  |  (s[3] & 0x3F);
        default: return 0xFFFD;
    }
}

static int cp_to_utf16le(unsigned int cp, unsigned char *buf)
{
    if (cp < 0x10000) {
        buf[0] = cp & 0xFF;
        buf[1] = (cp >> 8) & 0xFF;
        return 2;
    }
    cp -= 0x10000;
    unsigned int hi = 0xD800 | (cp >> 10);
    unsigned int lo = 0xDC00 | (cp & 0x3FF);
    buf[0] = hi & 0xFF;        buf[1] = (hi >> 8) & 0xFF;
    buf[2] = lo & 0xFF;        buf[3] = (lo >> 8) & 0xFF;
    return 4;
}

static int cp_to_utf16be(unsigned int cp, unsigned char *buf)
{
    if (cp < 0x10000) {
        buf[0] = (cp >> 8) & 0xFF;
        buf[1] = cp & 0xFF;
        return 2;
    }
    cp -= 0x10000;
    unsigned int hi = 0xD800 | (cp >> 10);
    unsigned int lo = 0xDC00 | (cp & 0x3FF);
    buf[0] = (hi >> 8) & 0xFF; buf[1] = hi & 0xFF;
    buf[2] = (lo >> 8) & 0xFF; buf[3] = lo & 0xFF;
    return 4;
}

/* Compute hex string for the first UTF-8 character of utf8_str. */
static void first_char_hex_utf8(const char *utf8_str, char *hex_out)
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

/* Compute hex string for the first character in UTF-16 LE bytes. */
static void first_char_hex_utf16le(const char *utf8_str, char *hex_out)
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

/* Compute hex string for the first character in UTF-16 BE bytes. */
static void first_char_hex_utf16be(const char *utf8_str, char *hex_out)
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

/* =========================================================================
 * XML WRITER helpers
 * ========================================================================= */

static const char *status_to_emoji(uint8_t status)
{
    switch (status) {
        case STATUS_BOARDED:   return "\xF0\x9F\x9F\xA2";           /* 🟢 */
        case STATUS_CANCELLED: return "\xF0\x9F\x94\xB4";           /* 🔴 */
        case STATUS_DELAYED:   return "\xE2\x9A\xA0\xEF\xB8\x8F";  /* ⚠️ */
        default:               return "UNKNOWN";
    }
}

static const char *cabin_to_str(uint8_t cabin)
{
    switch (cabin) {
        case CABIN_ECONOMY:  return "ECONOMY";
        case CABIN_BUSINESS: return "BUSINESS";
        case CABIN_FIRST:    return "FIRST";
        default:             return "ECONOMY";
    }
}

/* Copy basename of path into buf, stripping directory and extension. */
static void basename_no_ext(const char *path, char *buf, size_t buf_size)
{
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    strncpy(buf, base, buf_size - 1);
    buf[buf_size - 1] = '\0';
    char *dot = strrchr(buf, '.');
    if (dot) *dot = '\0';
}

/* =========================================================================
 * write_xml – build UTF-8 XML from binary records using libxml2
 * ========================================================================= */

int write_xml(const char *output_file,
              const PassengerRecord *records, int count)
{
    char root_name[256];
    basename_no_ext(output_file, root_name, sizeof(root_name));

    xmlDocPtr  doc  = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST root_name);
    xmlDocSetRootElement(doc, root);

    for (int i = 0; i < count; i++) {
        const PassengerRecord *r = &records[i];

        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%d", i + 1);
        xmlNodePtr entry = xmlNewChild(root, NULL, BAD_CAST "entry", NULL);
        xmlNewProp(entry, BAD_CAST "id", BAD_CAST id_str);

        xmlNodePtr ticket = xmlNewChild(entry, NULL, BAD_CAST "ticket", NULL);
        xmlNewChild(ticket, NULL, BAD_CAST "ticket_id",   BAD_CAST r->ticket_id);
        xmlNewChild(ticket, NULL, BAD_CAST "destination", BAD_CAST r->destination);
        xmlNewChild(ticket, NULL, BAD_CAST "app_ver",     BAD_CAST r->app_ver);

        xmlNodePtr metrics = xmlNewChild(entry, NULL, BAD_CAST "metrics", NULL);
        xmlNewProp(metrics, BAD_CAST "status",      BAD_CAST status_to_emoji(r->status));
        xmlNewProp(metrics, BAD_CAST "cabin_class", BAD_CAST cabin_to_str(r->cabin_class));

        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%.2f", (double)r->baggage_weight);
        xmlNewChild(metrics, NULL, BAD_CAST "baggage_weight", BAD_CAST tmp);
        snprintf(tmp, sizeof(tmp), "%d", r->loyalty_points);
        xmlNewChild(metrics, NULL, BAD_CAST "loyalty_points", BAD_CAST tmp);
        snprintf(tmp, sizeof(tmp), "%d", r->seat_num);
        xmlNewChild(metrics, NULL, BAD_CAST "seat_num", BAD_CAST tmp);

        xmlNewChild(entry, NULL, BAD_CAST "timestamp", BAD_CAST r->timestamp);

        char hex_buf[16];
        first_char_hex_utf8(r->passenger_name, hex_buf);
        xmlNodePtr pname = xmlNewChild(entry, NULL,
                                       BAD_CAST "passenger_name",
                                       BAD_CAST r->passenger_name);
        xmlNewProp(pname, BAD_CAST "current_encoding", BAD_CAST "UTF-8");
        xmlNewProp(pname, BAD_CAST "first_char_hex",   BAD_CAST hex_buf);
    }

    int rc = xmlSaveFormatFileEnc(output_file, doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return (rc > 0) ? 0 : -1;
}

/* =========================================================================
 * XML ENCODING CONVERSION helpers
 * ========================================================================= */

typedef void (*HexFn)(const char *, char *);

/* Walk the tree and update current_encoding + first_char_hex on every
   <passenger_name> element. hex_fn receives the text content as UTF-8. */
static void update_pname_attrs(xmlNodePtr node,
                                const char *enc_label, HexFn hex_fn)
{
    for (xmlNodePtr cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE &&
            xmlStrcmp(cur->name, BAD_CAST "passenger_name") == 0)
        {
            xmlChar *content = xmlNodeGetContent(cur);
            char hex_buf[16] = "00";
            if (content) {
                hex_fn((const char *)content, hex_buf);
                xmlFree(content);
            }
            xmlSetProp(cur, BAD_CAST "current_encoding", BAD_CAST enc_label);
            xmlSetProp(cur, BAD_CAST "first_char_hex",   BAD_CAST hex_buf);
        }
        update_pname_attrs(cur->children, enc_label, hex_fn);
    }
}

/* Replace the encoding="..." value in the XML declaration. */
static char *replace_xml_encoding_decl(const char *buf, const char *new_enc)
{
    const char *decl_start = strstr(buf, "<?xml");
    if (!decl_start) {
        size_t total = strlen("<?xml version=\"1.0\" encoding=\"\"?>\n")
                       + strlen(new_enc) + strlen(buf) + 1;
        char *out = malloc(total);
        if (!out) return NULL;
        snprintf(out, total, "<?xml version=\"1.0\" encoding=\"%s\"?>\n%s",
                 new_enc, buf);
        return out;
    }

    const char *decl_end = strstr(decl_start, "?>");
    if (!decl_end) return strdup(buf);
    decl_end += 2;

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

/* Write a UTF-8 buffer to a file encoded as UTF-16 LE or BE (with BOM). */
static int write_as_utf16(FILE *out, const char *utf8_data,
                           size_t len, int big_endian)
{
    if (big_endian) { fputc(0xFE, out); fputc(0xFF, out); }
    else            { fputc(0xFF, out); fputc(0xFE, out); }

    const unsigned char *p   = (const unsigned char *)utf8_data;
    const unsigned char *end = p + len;
    while (p < end) {
        int clen = utf8_char_len(*p);
        if (clen < 0 || p + clen > end) { p++; continue; }
        unsigned int cp = utf8_decode(p, clen);
        p += clen;
        unsigned char buf[4];
        int nbytes = big_endian ? cp_to_utf16be(cp, buf)
                                : cp_to_utf16le(cp, buf);
        fwrite(buf, 1, (size_t)nbytes, out);
    }
    return 0;
}

/* =========================================================================
 * convert_xml_encoding – public API
 * ========================================================================= */

int convert_xml_encoding(const char *input_file,
                         const char *output_file, int encoding_type)
{
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
        update_pname_attrs(root, "UTF-8", first_char_hex_utf8);
        rc = xmlSaveFormatFileEnc(output_file, doc, "UTF-8", 1);
        rc = (rc > 0) ? 0 : -1;
    } else {
        int big_endian    = (encoding_type == ENCODING_UTF16BE);
        const char *label = big_endian ? "UTF-16BE" : "UTF-16LE";
        HexFn hex_fn      = big_endian ? first_char_hex_utf16be
                                       : first_char_hex_utf16le;

        update_pname_attrs(root, label, hex_fn);

        xmlChar *utf8_buf = NULL;
        int      utf8_len = 0;
        xmlDocDumpMemoryEnc(doc, &utf8_buf, &utf8_len, "UTF-8");
        if (!utf8_buf) {
            fprintf(stderr, "Error: failed to dump XML to memory\n");
            xmlFreeDoc(doc);
            return -1;
        }

        char *modified = replace_xml_encoding_decl((const char *)utf8_buf, "UTF-16");
        xmlFree(utf8_buf);
        if (!modified) {
            fprintf(stderr, "Error: out of memory\n");
            xmlFreeDoc(doc);
            return -1;
        }

        FILE *out = fopen(output_file, "wb");
        if (!out) {
            perror(output_file);
            free(modified);
            xmlFreeDoc(doc);
            return -1;
        }

        rc = write_as_utf16(out, modified, strlen(modified), big_endian);
        fclose(out);
        free(modified);
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return rc;
}
