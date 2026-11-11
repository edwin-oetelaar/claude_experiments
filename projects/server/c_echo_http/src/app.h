/**
 * @file app.h
 * @brief What the server answers: from request bytes to response bytes.
 *
 * No sockets here. app_respond() gets the raw request head and writes the
 * complete response (status line, headers and body) into a buffer. That keeps
 * the interesting part testable without a network, and main.c only moves bytes.
 *
 * Routes:
 *   GET /     HTML page that shows the request, with every header escaped
 *   GET /raw  the request head exactly as received, as text/plain
 *   HEAD      same as GET, without the body
 *   other     404 for unknown paths, 405 for other methods
 */

#pragma once

#include <stddef.h>

/** Room for the largest body: every header byte escaped (x6) plus the page itself. */
#define APP_MAX_BODY     65536
/** Room for a whole response: status line and headers plus the body. */
#define APP_MAX_RESPONSE (APP_MAX_BODY + 1024)

typedef struct {
    const char *server_name;     /**< sent in the Server header */
    unsigned long request_count; /**< responses sent so far; shown in X-Request-Count */
} app_state_t;

/**
 * @brief Build the response to the request head @p head of @p head_len bytes.
 * @param out Buffer of at least APP_MAX_RESPONSE bytes.
 * @return Length of the response in @p out.
 */
size_t app_respond(app_state_t *state, const char *head, size_t head_len, char *out,
                   size_t out_size);

/**
 * @brief Build an error response that needs no parsed request, e.g. 431 for a head that is too big.
 * @return Length of the response in @p out.
 */
size_t app_respond_error(app_state_t *state, int status, char *out, size_t out_size);
