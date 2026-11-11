/**
 * @file http.h
 * @brief Parse an HTTP/1.x request head without copying it.
 *
 * A request head is plain text:
 *
 *     GET /path HTTP/1.1\r\n
 *     Host: localhost\r\n
 *     \r\n
 *
 * The parser does not copy strings. Every field is an http_str_t: a pointer
 * into the caller's buffer plus a length. That is fast and needs no malloc,
 * but the fields are only valid while that buffer is.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/** Largest request head we accept, terminating blank line included. */
#define HTTP_MAX_HEAD    8192
/** Most header lines we accept in one request. */
#define HTTP_MAX_HEADERS 32

/** @brief A slice of a buffer: not NUL-terminated, print with "%.*s". */
typedef struct {
    const char *ptr;
    size_t len;
} http_str_t;

typedef struct {
    http_str_t name;
    http_str_t value;
} http_header_t;

typedef struct {
    http_str_t method;  /**< "GET", "HEAD", ... */
    http_str_t target;  /**< "/path?query" */
    http_str_t version; /**< "HTTP/1.1" */
    http_header_t headers[HTTP_MAX_HEADERS];
    size_t header_count;
} http_request_t;

typedef enum {
    HTTP_PARSE_OK = 0,
    HTTP_PARSE_BAD_REQUEST,      /**< malformed request line or header line */
    HTTP_PARSE_BAD_VERSION,      /**< not HTTP/1.0 or HTTP/1.1 */
    HTTP_PARSE_TOO_MANY_HEADERS, /**< more than HTTP_MAX_HEADERS */
} http_parse_result_t;

/**
 * @brief Find the blank line that ends the request head.
 *
 * Accepts "\r\n\r\n" as the standard says, and also "\n\n" so that a request
 * typed by hand in `nc` works too.
 *
 * @return Length of the head including the blank line, or 0 if it is not complete yet.
 */
size_t http_find_head_end(const char *buf, size_t len);

/**
 * @brief Parse a complete request head of @p len bytes (as found by http_find_head_end).
 * @param req Filled on HTTP_PARSE_OK; its slices point into @p buf.
 */
http_parse_result_t http_parse_request(const char *buf, size_t len, http_request_t *req);

/** @brief True if @p s equals the C string @p lit exactly. */
bool http_str_eq(http_str_t s, const char *lit);

/** @brief The standard reason phrase for @p status, e.g. 404 -> "Not Found". */
const char *http_reason(int status);

/**
 * @brief Copy @p len bytes of @p in to @p out, escaping & < > " ' for HTML.
 *
 * Text from a client must be escaped before it goes into a page; otherwise a
 * header like `X-Name: <script>...</script>` runs as code in the browser (XSS).
 *
 * @return Bytes written (without the terminating NUL). Output is always
 *         NUL-terminated and is cut off when @p out_size is too small.
 */
size_t http_html_escape(const char *in, size_t len, char *out, size_t out_size);
