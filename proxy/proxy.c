#include "proxy.h"
#include "logger/logger.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct client_ctx {
    int client_fd;
} client_ctx_t;

static ssize_t send_all(int fd, const void *buf, size_t len) 
{
    const char *p = buf;
    size_t left   = len;

    while (left > 0) 
    {
        ssize_t n = send(fd, p, left, 0);
        if (n < 0) 
        {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        p += n;
        left -= (size_t)n;
    }
    return (ssize_t)(len - left);
}

static char *memmem_simple(char *haystack, size_t haystack_len, const char *needle, size_t needle_len)
{
    if (needle_len == 0 || haystack_len < needle_len) return NULL;

    for (size_t i = 0; i + needle_len <= haystack_len; ++i) 
        if (memcmp(haystack + i, needle, needle_len) == 0) return haystack + i;
    return NULL;
}

static char *find_host_header(char *buf, size_t len) 
{
    const char *needle = "Host:";
    size_t nlen = strlen(needle);

    char *p = memmem_simple(buf, len, needle, nlen);
    if (!p) return NULL;

    p += nlen;
    while (p < buf + len && (*p == ' ' || *p == '\t')) p++;
    return p;
}

static int create_listen_socket(int port) 
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) 
    {
        log_error("socket failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        log_error("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
    {
        log_error("bind on port %d failed: %s", port, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 64) < 0) 
    {
        log_error("listen failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

static int connect_to_origin(const char *host, int port) 
{
    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) 
    {
        log_error("gethostbyname(%s) failed", host);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) 
    {
        log_error("socket for origin failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
    {
        log_error("connect_to_origin(%s:%d) failed: %s", host, port, strerror(errno));
        close(fd);
        return -1;
    }

    log_debug("connected to origin %s:%d, fd=%d", host, port, fd);
    return fd;
}

static void handle_client(void *arg) 
{
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int cfd = ctx->client_fd;
    free(ctx);

    log_debug("client fd=%d: handler started", cfd);

    char   buf[8192];
    size_t used = 0;

    for (;;) 
    {
        ssize_t n = recv(cfd, buf + used, sizeof(buf) - used, 0);
        if (n < 0) 
        {
            if (errno == EINTR) continue;
            log_error("client fd=%d: recv failed: %s", cfd, strerror(errno));
            close(cfd);
            return;
        }
        if (n == 0) 
        {
            log_info("client fd=%d: closed connection before headers", cfd);
            close(cfd);
            return;
        }

        used += (size_t)n;

        if (memmem_simple(buf, used, "\r\n\r\n", 4) != NULL) break;

        if (used == sizeof(buf)) 
        {
            log_error("client fd=%d: headers too large", cfd);
            close(cfd);
            return;
        }
    }

    char *line_end = memmem_simple(buf, used, "\r\n", 2);
    if (!line_end) 
    {
        log_error("client fd=%d: no CRLF in request line", cfd);
        close(cfd);
        return;
    }
    *line_end = '\0';

    char *method = buf;
    char *sp1 = strchr(method, ' ');
    if (!sp1) 
    {
        log_error("client fd=%d: bad request line (no space after method)", cfd);
        close(cfd);
        return;
    }
    *sp1 = '\0';

    char *path = sp1 + 1;
    char *sp2  = strchr(path, ' ');
    if (!sp2) 
    {
        log_error("client fd=%d: bad request line (no space before HTTP)", cfd);
        close(cfd);
        return;
    }
    *sp2 = '\0';

    if (strcmp(method, "GET") != 0) 
    {
        log_info("client fd=%d: unsupported method '%s', closing", cfd, method);
        close(cfd);
        return;
    }

    char *uri = path;
    if (strncmp(path, "http://", 7) == 0) 
    {
        char *p = path + 7;
        while (*p && *p != '/') p++;
        if (*p == '\0') uri = (char *)"/";
        else uri = p;
    }

    char *headers_start = line_end + 2;
    size_t headers_len  = used - (size_t)(headers_start - buf);

    char *host_start = find_host_header(headers_start, headers_len);
    if (!host_start) 
    {
        log_error("client fd=%d: Host header not found", cfd);
        close(cfd);
        return;
    }

    char *p = host_start;
    char *buf_end = buf + used;
    while (p < buf_end && *p != '\r' && *p != '\n') p++;
    
    *p = '\0';

    char host[256];
    size_t host_len = strlen(host_start);
    if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
    memcpy(host, host_start, host_len);
    host[host_len] = '\0';

    log_debug("client fd=%d: method=%s host=%s uri=%s", cfd, method, host, uri);

    int ofd = connect_to_origin(host, 80);
    if (ofd < 0) 
    {
        const char *resp =
            "HTTP/1.0 502 Bad Gateway\r\n"
            "Connection: close\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

        (void)send_all(cfd, resp, strlen(resp));
        close(cfd);
        return;
    }

    char req[4096];
    int req_len = snprintf(req, sizeof(req),
                           "GET %s HTTP/1.0\r\n"
                           "Host: %s\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           uri, host);
    if (req_len <= 0 || req_len >= (int)sizeof(req)) 
    {
        log_error("client fd=%d: failed to build HTTP/1.0 request", cfd);
        close(ofd);
        close(cfd);
        return;
    }

    log_debug("client fd=%d: sending request to origin:\n%.*s", cfd, req_len, req);

    if (send_all(ofd, req, (size_t)req_len) < 0) 
    {
        log_error("client fd=%d: send_all to origin failed: %s", cfd, strerror(errno));
        close(ofd);
        close(cfd);
        return;
    }

    char serv_buf[8192];
    for (;;) 
    {
        ssize_t n = recv(ofd, serv_buf, sizeof(serv_buf), 0);
        if (n < 0) 
        {
            if (errno == EINTR) continue;
            log_error("client fd=%d: recv from origin failed: %s", cfd, strerror(errno));
            break;
        }
        if (n == 0) 
        {
            log_debug("client fd=%d: origin closed connection", cfd);
            break;
        }

        if (send_all(cfd, serv_buf, (size_t)n) < 0) 
        {
            log_error("client fd=%d: send_all to client failed: %s", cfd, strerror(errno));
            break;
        }
    }

    close(ofd);
    close(cfd);
    log_debug("client fd=%d: handler finished", cfd);
}


int proxy_run(const proxy_config_t *cfg) 
{
    signal(SIGPIPE, SIG_IGN);

    threadPool_t *pool = threadpoll_init(cfg->worker_count);
    if (!pool) 
    {
        log_error("failed to create thread pool");
        return 1;
    }

    int lfd = create_listen_socket(cfg->listen_port);
    if (lfd < 0) 
    {
        log_error("failed to create listen socket on port %d", cfg->listen_port);
        threadpool_stop(pool);
        return 1;
    }

    log_info("proxy listening on port %d", cfg->listen_port);

    for (;;) 
    {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        int cfd = accept(lfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (cfd < 0) 
        {
            if (errno == EINTR) continue;
            log_error("accept failed: %s", strerror(errno));
            break;
        }

        log_debug("accepted client fd=%d", cfd);

        client_ctx_t *ctx = malloc(sizeof(*ctx));
        if (!ctx) 
        {
            log_error("malloc client_ctx failed");
            close(cfd);
            continue;
        }

        ctx->client_fd = cfd;

        if (threadpool_submit_task(pool, handle_client, ctx) != 0) 
        {
            log_error("threadPoolSubmit failed for client fd=%d", cfd);
            close(cfd);
            free(ctx);
            continue;
        }
    }

    close(lfd);
    threadpool_stop(pool);
    log_info("proxy stopped");
    return 0;
}