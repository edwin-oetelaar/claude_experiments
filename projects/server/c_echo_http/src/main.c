/**
 * @file main.c
 * @brief The network side: socket, bind, listen, accept, read, write, close.
 *
 * This server handles one client at a time ("iterative server"): accept a
 * connection, read the request, send the response, close, and only then
 * accept the next one. Simple and easy to follow, but one slow client keeps
 * everyone else waiting. Making it concurrent is one of the exercises.
 */

#include "app.h"
#include "http.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define DEFAULT_PORT    8080
#define LISTEN_BACKLOG  16
#define READ_TIMEOUT_S   5   // a client that sends nothing is dropped after this
#define DRAIN_TIMEOUT_MS 500 // how long to wait for a client to close after the response

/** Set by the SIGINT/SIGTERM handler; checked by the accept loop. */
static volatile sig_atomic_t s_stop = 0;

static void s_on_signal(int signo)
{
    (void)signo;
    s_stop = 1;
}

/**
 * @brief Install the stop handler *without* SA_RESTART, so a blocking accept()
 *        returns with EINTR on Ctrl+C and the loop can end cleanly.
 */
static void s_install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = s_on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    // Writing to a socket the client already closed must not kill the server.
    signal(SIGPIPE, SIG_IGN);
}

/** @return listening socket, or -1 after printing why. */
static int s_open_listener(const char *host, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid IPv4 address: %s\n", host);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    // Allow a restart right away instead of "Address already in use" for a minute.
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        fprintf(stderr, "bind %s:%d: %s\n", host, port, strerror(errno));
        close(fd);
        return -1;
    }
    if (listen(fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

/** @brief Send all @p len bytes; write() may send fewer than asked. */
static bool s_send_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        buf += n;
        len -= (size_t)n;
    }
    return true;
}

/**
 * @brief Close our sending side and read away what the client still sends.
 *
 * If a socket is closed while unread data sits in its receive buffer (a
 * request body we ignored, the rest of a too-large head), the kernel answers
 * with a TCP reset, and the client may lose the response we just sent. So:
 * signal "no more data" with shutdown(), then read until the client closes
 * too, for at most DRAIN_TIMEOUT_MS.
 */
static void s_drain(int fd)
{
    shutdown(fd, SHUT_WR);
    struct timeval timeout = {.tv_sec = 0, .tv_usec = DRAIN_TIMEOUT_MS * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    char sink[1024];
    while (recv(fd, sink, sizeof sink, 0) > 0) {
    }
}

/** @brief Read one request from @p fd, answer it and log one line. */
static void s_handle_client(int fd, app_state_t *state)
{
    char head[HTTP_MAX_HEAD];
    size_t used = 0;
    size_t head_len = 0;

    // TCP is a byte stream, not a message stream: a request can arrive in
    // pieces, so keep reading until the blank line that ends the head shows up.
    while (head_len == 0 && used < sizeof head) {
        ssize_t n = recv(fd, head + used, sizeof head - used, 0);
        if (n == 0) {
            return; // client closed without a (complete) request
        }
        if (n < 0) {
            if (errno == EINTR && !s_stop) {
                continue;
            }
            return; // timeout or error: drop the client
        }
        used += (size_t)n;
        head_len = http_find_head_end(head, used);
    }

    char out[APP_MAX_RESPONSE];
    size_t out_len;
    if (head_len == 0) {
        out_len = app_respond_error(state, 431, out, sizeof out);
    } else {
        out_len = app_respond(state, head, head_len, out, sizeof out);
    }
    if (!s_send_all(fd, out, out_len)) {
        fprintf(stderr, "send: %s\n", strerror(errno));
    }
    s_drain(fd);

    // Log the request line and the status ("HTTP/1.1 200 OK" -> chars 9..11).
    const char *eol = memchr(head, '\n', used);
    int line_len = eol != NULL ? (int)(eol - head) : (int)(used < 60 ? used : 60);
    if (line_len > 0 && head[line_len - 1] == '\r') {
        line_len--;
    }
    fprintf(stderr, "%.*s -> %.3s\n", line_len, head, out + 9);
}

static void s_usage(const char *prog)
{
    printf("usage: %s [--host ADDR] [--port N]\n"
           "  --host ADDR  IPv4 address to listen on (default 127.0.0.1; 0.0.0.0 = all)\n"
           "  --port N     TCP port (default %d; 0 = any free port)\n",
           prog, DEFAULT_PORT);
}

int main(int argc, char **argv)
{
    const char *host = "127.0.0.1";
    int port = DEFAULT_PORT;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            char *end = NULL;
            long p = strtol(argv[++i], &end, 10);
            if (*end != '\0' || p < 0 || p > 65535) {
                fprintf(stderr, "invalid port: %s\n", argv[i]);
                return EXIT_FAILURE;
            }
            port = (int)p;
        } else {
            s_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    s_install_signal_handlers();
    int listen_fd = s_open_listener(host, port);
    if (listen_fd < 0) {
        return EXIT_FAILURE;
    }

    // With --port 0 the kernel picks a port; ask which one it chose.
    struct sockaddr_in bound;
    socklen_t bound_len = sizeof bound;
    getsockname(listen_fd, (struct sockaddr *)&bound, &bound_len);
    printf("Listening on http://%s:%d\n", host, ntohs(bound.sin_port));
    fflush(stdout); // stdout is fully buffered when it is a pipe

    app_state_t state = {.server_name = "c_echo_http", .request_count = 0};
    while (!s_stop) {
        int client = accept(listen_fd, NULL, NULL);
        if (client < 0) {
            if (errno != EINTR) {
                perror("accept");
            }
            continue;
        }
        struct timeval timeout = {.tv_sec = READ_TIMEOUT_S, .tv_usec = 0};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        s_handle_client(client, &state);
        close(client);
    }

    close(listen_fd);
    fprintf(stderr, "stopped after %lu request(s)\n", state.request_count);
    return EXIT_SUCCESS;
}
