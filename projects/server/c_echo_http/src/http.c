/**
 * @file http.c
 * @brief Request-head parsing and small HTTP helpers.
 */

#include "http.h"

#include <assert.h>
#include <string.h>

/** @brief Strip leading and trailing spaces/tabs. */
static http_str_t s_trim(http_str_t s)
{
    while (s.len > 0 && (s.ptr[0] == ' ' || s.ptr[0] == '\t')) {
        s.ptr++;
        s.len--;
    }
    while (s.len > 0 && (s.ptr[s.len - 1] == ' ' || s.ptr[s.len - 1] == '\t')) {
        s.len--;
    }
    return s;
}

/**
 * @brief Take the next line from @p *cursor, without its "\n" or "\r\n".
 * @return false when there are no more lines.
 */
static bool s_next_line(http_str_t *cursor, http_str_t *line)
{
    if (cursor->len == 0) {
        return false;
    }
    const char *nl = memchr(cursor->ptr, '\n', cursor->len);
    size_t take = nl != NULL ? (size_t)(nl - cursor->ptr) : cursor->len;

    line->ptr = cursor->ptr;
    line->len = take;
    if (line->len > 0 && line->ptr[line->len - 1] == '\r') {
        line->len--;
    }

    size_t consumed = nl != NULL ? take + 1 : take;
    cursor->ptr += consumed;
    cursor->len -= consumed;
    return true;
}

/** @brief Split @p s at the first @p sep: @p head gets the part before, @p s the rest. */
static bool s_split(http_str_t *s, char sep, http_str_t *head)
{
    const char *p = memchr(s->ptr, sep, s->len);
    if (p == NULL) {
        return false;
    }
    head->ptr = s->ptr;
    head->len = (size_t)(p - s->ptr);
    s->len -= head->len + 1;
    s->ptr = p + 1;
    return true;
}

size_t http_find_head_end(const char *buf, size_t len)
{
    assert(buf != NULL || len == 0);
    for (size_t i = 0; i + 1 < len; ++i) {
        if (buf[i] != '\n') {
            continue;
        }
        if (buf[i + 1] == '\n') {
            return i + 2;
        }
        if (i + 2 < len && buf[i + 1] == '\r' && buf[i + 2] == '\n') {
            return i + 3;
        }
    }
    return 0;
}

http_parse_result_t http_parse_request(const char *buf, size_t len, http_request_t *req)
{
    assert(buf != NULL);
    assert(req != NULL);
    memset(req, 0, sizeof *req);

    http_str_t cursor = {buf, len};
    http_str_t line;

    // Request line: METHOD SP TARGET SP VERSION, nothing more.
    if (!s_next_line(&cursor, &line)) {
        return HTTP_PARSE_BAD_REQUEST;
    }
    if (!s_split(&line, ' ', &req->method) || !s_split(&line, ' ', &req->target)) {
        return HTTP_PARSE_BAD_REQUEST;
    }
    req->version = line;
    if (req->method.len == 0 || req->target.len == 0 || req->target.ptr[0] != '/' ||
        memchr(req->version.ptr, ' ', req->version.len) != NULL) {
        return HTTP_PARSE_BAD_REQUEST;
    }
    if (!http_str_eq(req->version, "HTTP/1.1") && !http_str_eq(req->version, "HTTP/1.0")) {
        // Garbage that merely sits where the version goes is a bad request, not a new version.
        if (req->version.len < 5 || memcmp(req->version.ptr, "HTTP/", 5) != 0) {
            return HTTP_PARSE_BAD_REQUEST;
        }
        return HTTP_PARSE_BAD_VERSION;
    }

    // Header lines: "Name: value", until the blank line.
    while (s_next_line(&cursor, &line) && line.len > 0) {
        http_header_t h;
        if (!s_split(&line, ':', &h.name) || h.name.len == 0) {
            return HTTP_PARSE_BAD_REQUEST;
        }
        if (h.name.ptr[h.name.len - 1] == ' ' || h.name.ptr[h.name.len - 1] == '\t') {
            return HTTP_PARSE_BAD_REQUEST; // "Name : value" is forbidden by RFC 9112
        }
        h.value = s_trim(line);
        if (req->header_count == HTTP_MAX_HEADERS) {
            return HTTP_PARSE_TOO_MANY_HEADERS;
        }
        req->headers[req->header_count++] = h;
    }
    return HTTP_PARSE_OK;
}

bool http_str_eq(http_str_t s, const char *lit)
{
    assert(lit != NULL);
    size_t n = strlen(lit);
    return s.len == n && memcmp(s.ptr, lit, n) == 0;
}

const char *http_reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 431: return "Request Header Fields Too Large";
    case 505: return "HTTP Version Not Supported";
    default: return "Unknown";
    }
}

size_t http_html_escape(const char *in, size_t len, char *out, size_t out_size)
{
    assert(in != NULL || len == 0);
    assert(out != NULL && out_size > 0);

    size_t w = 0;
    for (size_t i = 0; i < len; ++i) {
        const char *rep = NULL;
        switch (in[i]) {
        case '&': rep = "&amp;"; break;
        case '<': rep = "&lt;"; break;
        case '>': rep = "&gt;"; break;
        case '"': rep = "&quot;"; break;
        case '\'': rep = "&#39;"; break;
        default: break;
        }
        size_t n = rep != NULL ? strlen(rep) : 1;
        if (w + n >= out_size) {
            break; // no room: stop at a whole character, never half an entity
        }
        if (rep != NULL) {
            memcpy(out + w, rep, n);
        } else {
            out[w] = in[i];
        }
        w += n;
    }
    out[w] = '\0';
    return w;
}
