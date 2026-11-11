/**
 * @file app.c
 * @brief Routing and response building.
 */

#include "app.h"
#include "http.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** @brief A growing text buffer with a hard limit; writes past the limit are dropped. */
typedef struct {
    char *buf;
    size_t len;
    size_t size;
} s_builder_t;

static void s_append(s_builder_t *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void s_append(s_builder_t *b, const char *fmt, ...)
{
    assert(b->len < b->size);
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(b->buf + b->len, b->size - b->len, fmt, args);
    va_end(args);
    if (n < 0) {
        return;
    }
    // vsnprintf reports what it *wanted* to write; never move past the end.
    size_t room = b->size - b->len - 1;
    b->len += (size_t)n < room ? (size_t)n : room;
}

static void s_append_escaped(s_builder_t *b, http_str_t s)
{
    b->len += http_html_escape(s.ptr, s.len, b->buf + b->len, b->size - b->len);
}

/** @brief Status line and headers, then the body unless @p with_body is false (HEAD). */
static size_t s_finish(app_state_t *state, int status, const char *content_type,
                       const char *extra_headers, const char *body, size_t body_len,
                       bool with_body, char *out, size_t out_size)
{
    state->request_count++;
    s_builder_t r = {out, 0, out_size};
    s_append(&r,
             "HTTP/1.1 %d %s\r\n"
             "Server: %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "X-Request-Count: %lu\r\n"
             "Connection: close\r\n"
             "%s"
             "\r\n",
             status, http_reason(status), state->server_name, content_type, body_len,
             state->request_count, extra_headers);
    if (with_body && body_len <= r.size - r.len) {
        memcpy(r.buf + r.len, body, body_len);
        r.len += body_len;
    }
    return r.len;
}

static size_t s_plain(app_state_t *state, int status, const char *extra_headers, bool with_body,
                      char *out, size_t out_size)
{
    char body[128];
    int n = snprintf(body, sizeof body, "%d %s\n", status, http_reason(status));
    return s_finish(state, status, "text/plain; charset=utf-8", extra_headers, body, (size_t)n,
                    with_body, out, out_size);
}

static size_t s_page(app_state_t *state, const http_request_t *req, bool with_body, char *out,
                     size_t out_size)
{
    char body[APP_MAX_BODY]; // 64 KiB on the stack: fine on Linux (8 MiB stack), and thread-safe
    s_builder_t b = {body, 0, sizeof body};

    s_append(&b,
             "<!doctype html>\n<html><head><meta charset=\"utf-8\"><title>Echo</title>\n"
             "<style>body{font-family:system-ui,sans-serif;background:#111;color:#ddd;"
             "max-width:60rem;margin:2rem auto;padding:0 1rem}"
             "code,td{font-family:ui-monospace,monospace}"
             "table{border-collapse:collapse;width:100%%}"
             "td{border-bottom:1px solid #333;padding:.3rem .6rem;vertical-align:top;"
             "word-break:break-all}"
             "td:first-child{color:#8f8;white-space:nowrap}a{color:#8cf}</style></head>\n"
             "<body><h1>Echo from %s</h1>\n<p>You are request number <b>%lu</b>. "
             "Your browser sent:</p>\n<p><code>",
             state->server_name, state->request_count + 1);
    s_append_escaped(&b, req->method);
    s_append(&b, " ");
    s_append_escaped(&b, req->target);
    s_append(&b, " ");
    s_append_escaped(&b, req->version);
    s_append(&b, "</code></p>\n<table>\n");
    for (size_t i = 0; i < req->header_count; ++i) {
        s_append(&b, "<tr><td>");
        s_append_escaped(&b, req->headers[i].name);
        s_append(&b, "</td><td>");
        s_append_escaped(&b, req->headers[i].value);
        s_append(&b, "</td></tr>\n");
    }
    s_append(&b, "</table>\n<p>See the exact bytes: <a href=\"/raw\">/raw</a></p>\n"
                 "</body></html>\n");

    return s_finish(state, 200, "text/html; charset=utf-8", "", body, b.len, with_body, out,
                    out_size);
}

size_t app_respond(app_state_t *state, const char *head, size_t head_len, char *out,
                   size_t out_size)
{
    assert(state != NULL && head != NULL && out != NULL);
    assert(out_size >= APP_MAX_RESPONSE);

    http_request_t req;
    switch (http_parse_request(head, head_len, &req)) {
    case HTTP_PARSE_OK: break;
    case HTTP_PARSE_BAD_REQUEST: return app_respond_error(state, 400, out, out_size);
    case HTTP_PARSE_BAD_VERSION: return app_respond_error(state, 505, out, out_size);
    case HTTP_PARSE_TOO_MANY_HEADERS: return app_respond_error(state, 431, out, out_size);
    }

    bool is_get = http_str_eq(req.method, "GET");
    bool is_head = http_str_eq(req.method, "HEAD");
    if (!is_get && !is_head) {
        return s_plain(state, 405, "Allow: GET, HEAD\r\n", true, out, out_size);
    }
    if (http_str_eq(req.target, "/")) {
        return s_page(state, &req, is_get, out, out_size);
    }
    if (http_str_eq(req.target, "/raw")) {
        return s_finish(state, 200, "text/plain; charset=utf-8", "", head, head_len, is_get, out,
                        out_size);
    }
    return s_plain(state, 404, "", is_get, out, out_size);
}

size_t app_respond_error(app_state_t *state, int status, char *out, size_t out_size)
{
    assert(state != NULL && out != NULL);
    return s_plain(state, status, "", true, out, out_size);
}
