/**
 * @file http_test.c
 * @brief Tests for the parser and the responses, without a network.
 *
 * CHECK instead of assert(): assert() vanishes in a Release build (NDEBUG),
 * and the tests must check something in every build.
 */

#include "app.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);               \
            ++s_failures;                                                                          \
        }                                                                                          \
    } while (0)

static http_parse_result_t s_parse(const char *text, http_request_t *req)
{
    return http_parse_request(text, strlen(text), req);
}

static void test_find_head_end(void)
{
    CHECK(http_find_head_end("GET / HTTP/1.1\r\nHost: x\r\n\r\n", 27) == 27);
    CHECK(http_find_head_end("GET / HTTP/1.1\n\n", 16) == 16);
    CHECK(http_find_head_end("GET / HTTP/1.1\r\nHost: x\r\n", 25) == 0); // not complete yet
    CHECK(http_find_head_end("", 0) == 0);
}

static void test_parse_ok(void)
{
    http_request_t req;
    CHECK(s_parse("GET /a?b=1 HTTP/1.1\r\nHost: example\r\nX-Pad:   spaced  \r\n\r\n", &req) ==
          HTTP_PARSE_OK);
    CHECK(http_str_eq(req.method, "GET"));
    CHECK(http_str_eq(req.target, "/a?b=1"));
    CHECK(http_str_eq(req.version, "HTTP/1.1"));
    CHECK(req.header_count == 2);
    CHECK(http_str_eq(req.headers[0].name, "Host"));
    CHECK(http_str_eq(req.headers[0].value, "example"));
    CHECK(http_str_eq(req.headers[1].value, "spaced")); // surrounding whitespace trimmed
}

static void test_parse_errors(void)
{
    http_request_t req;
    CHECK(s_parse("HELLO\r\n\r\n", &req) == HTTP_PARSE_BAD_REQUEST);
    CHECK(s_parse("GET nopath HTTP/1.1\r\n\r\n", &req) == HTTP_PARSE_BAD_REQUEST);
    CHECK(s_parse("GET / HTTP/1.1 extra\r\n\r\n", &req) == HTTP_PARSE_BAD_REQUEST);
    CHECK(s_parse("GET / HTTP/1.1\r\nNoColon\r\n\r\n", &req) == HTTP_PARSE_BAD_REQUEST);
    CHECK(s_parse("GET / HTTP/1.1\r\nName : v\r\n\r\n", &req) == HTTP_PARSE_BAD_REQUEST);
    CHECK(s_parse("GET / HTTP/2.0\r\n\r\n", &req) == HTTP_PARSE_BAD_VERSION);

    char many[4096] = "GET / HTTP/1.1\r\n";
    for (int i = 0; i <= HTTP_MAX_HEADERS; ++i) {
        strcat(many, "X: y\r\n");
    }
    strcat(many, "\r\n");
    CHECK(s_parse(many, &req) == HTTP_PARSE_TOO_MANY_HEADERS);
}

static void test_html_escape(void)
{
    char out[64];
    const char *evil = "<b>\"Tom & 'Jerry'\"</b>";
    http_html_escape(evil, strlen(evil), out, sizeof out);
    CHECK(strcmp(out, "&lt;b&gt;&quot;Tom &amp; &#39;Jerry&#39;&quot;&lt;/b&gt;") == 0);

    char tiny[5]; // "a" + "&lt;" would need 6 bytes with the NUL
    http_html_escape("a<b", 3, tiny, sizeof tiny);
    CHECK(strcmp(tiny, "a") == 0); // "&lt;" does not fit: never half an entity
}

static void test_responses(void)
{
    static char out[APP_MAX_RESPONSE];
    app_state_t st = {.server_name = "test", .request_count = 0};

    const char *get = "GET / HTTP/1.1\r\nX-Evil: <script>\r\n\r\n";
    size_t n = app_respond(&st, get, strlen(get), out, sizeof out);
    out[n] = '\0';
    CHECK(strncmp(out, "HTTP/1.1 200 OK\r\n", 17) == 0);
    CHECK(strstr(out, "X-Request-Count: 1\r\n") != NULL);
    CHECK(strstr(out, "&lt;script&gt;") != NULL);
    CHECK(strstr(out, "<script>") == NULL);

    const char *raw = "GET /raw HTTP/1.1\r\n\r\n";
    n = app_respond(&st, raw, strlen(raw), out, sizeof out);
    out[n] = '\0';
    CHECK(strstr(out, "X-Request-Count: 2\r\n") != NULL);
    CHECK(n >= strlen(raw) && strcmp(out + n - strlen(raw), raw) == 0); // body is the request

    const char *post = "POST / HTTP/1.1\r\n\r\n";
    n = app_respond(&st, post, strlen(post), out, sizeof out);
    out[n] = '\0';
    CHECK(strncmp(out, "HTTP/1.1 405 ", 13) == 0);
    CHECK(strstr(out, "Allow: GET, HEAD\r\n") != NULL);

    const char *head = "HEAD /missing HTTP/1.1\r\n\r\n";
    n = app_respond(&st, head, strlen(head), out, sizeof out);
    out[n] = '\0';
    CHECK(strncmp(out, "HTTP/1.1 404 ", 13) == 0);
    CHECK(strcmp(out + n - 4, "\r\n\r\n") == 0); // HEAD: headers only, no body
}

int main(void)
{
    test_find_head_end();
    test_parse_ok();
    test_parse_errors();
    test_html_escape();
    test_responses();

    if (s_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", s_failures);
        return EXIT_FAILURE;
    }
    printf("all http tests passed\n");
    return EXIT_SUCCESS;
}
