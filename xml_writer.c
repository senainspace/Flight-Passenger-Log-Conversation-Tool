#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "xml_writer.h"
#include "xml_encoding.h"

/* ── Status code → emoji UTF-8 string ───────────────────────────────── */
static const char *status_to_emoji(uint8_t status)
{
    switch (status) {
        case STATUS_BOARDED:   return "\xF0\x9F\x9F\xA2";           /* 🟢 */
        case STATUS_CANCELLED: return "\xF0\x9F\x94\xB4";           /* 🔴 */
        case STATUS_DELAYED:   return "\xE2\x9A\xA0\xEF\xB8\x8F";  /* ⚠️ */
        default:               return "UNKNOWN";
    }
}

/* ── Cabin class code → string ───────────────────────────────────────── */
static const char *cabin_to_str(uint8_t cabin)
{
    switch (cabin) {
        case CABIN_ECONOMY:  return "ECONOMY";
        case CABIN_BUSINESS: return "BUSINESS";
        case CABIN_FIRST:    return "FIRST";
        default:             return "ECONOMY";
    }
}

/* ── Strip file extension, return base name ─────────────────────────── */
/*
 * Copies `path` into `buf` (up to buf_size-1 chars), then truncates at the
 * last '.' found after the last '/' or '\'.
 * Also strips any leading directory components.
 */
static void basename_no_ext(const char *path, char *buf, size_t buf_size)
{
    /* Find last path separator */
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }

    strncpy(buf, base, buf_size - 1);
    buf[buf_size - 1] = '\0';

    /* Remove extension */
    char *dot = strrchr(buf, '.');
    if (dot) *dot = '\0';
}

/* ── Compute first_char_hex for UTF-8 ───────────────────────────────── */
void compute_first_char_hex_utf8(const char *utf8_str, char *hex_out)
{
    if (!utf8_str || !utf8_str[0]) {
        strcpy(hex_out, "00");
        return;
    }

    int len = utf8_char_len((unsigned char)utf8_str[0]);
    if (len < 0) len = 1;

    hex_out[0] = '\0';
    for (int i = 0; i < len; i++) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%02X", (unsigned char)utf8_str[i]);
        strcat(hex_out, tmp);
    }
}

/* ── Main write_xml implementation ──────────────────────────────────── */
int write_xml(const char *output_file,
              const PassengerRecord *records,
              int count)
{
    /* Derive root element name from output filename */
    char root_name[256];
    basename_no_ext(output_file, root_name, sizeof(root_name));

    /* Create document */
    xmlDocPtr  doc  = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST root_name);
    xmlDocSetRootElement(doc, root);

    for (int i = 0; i < count; i++) {
        const PassengerRecord *r = &records[i];

        /* <entry id="N"> */
        xmlNodePtr entry = xmlNewChild(root, NULL, BAD_CAST "entry", NULL);
        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%d", i + 1);
        xmlNewProp(entry, BAD_CAST "id", BAD_CAST id_str);

        /* <ticket> */
        xmlNodePtr ticket = xmlNewChild(entry, NULL, BAD_CAST "ticket", NULL);
        xmlNewChild(ticket, NULL, BAD_CAST "ticket_id",   BAD_CAST r->ticket_id);
        xmlNewChild(ticket, NULL, BAD_CAST "destination", BAD_CAST r->destination);
        xmlNewChild(ticket, NULL, BAD_CAST "app_ver",     BAD_CAST r->app_ver);

        /* <metrics status="🟢" cabin_class="ECONOMY"> */
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

        /* <timestamp> */
        xmlNewChild(entry, NULL, BAD_CAST "timestamp", BAD_CAST r->timestamp);

        /* <passenger_name current_encoding="UTF-8" first_char_hex="..."> */
        char hex_buf[16];
        compute_first_char_hex_utf8(r->passenger_name, hex_buf);

        xmlNodePtr pname = xmlNewChild(entry, NULL,
                                       BAD_CAST "passenger_name",
                                       BAD_CAST r->passenger_name);
        xmlNewProp(pname, BAD_CAST "current_encoding", BAD_CAST "UTF-8");
        xmlNewProp(pname, BAD_CAST "first_char_hex",   BAD_CAST hex_buf);
    }

    /* Save as UTF-8 with indentation */
    int rc = xmlSaveFormatFileEnc(output_file, doc, "UTF-8", 1);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    return (rc > 0) ? 0 : -1;
}
