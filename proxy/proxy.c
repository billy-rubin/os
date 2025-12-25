#include "cache/cache.h"
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
    cache_table_t *cache;
} client_ctx_t;

static void *gc_thread_loop(void *arg) {
    cache_table_t *cache = (cache_table_t *)arg;
    log_info("GC thread started");
    
    while (1) {
        sleep(1); 
        cache_evict_if_needed(cache);
    }
    return NULL;
}

static ssize_t send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t left   = len;

    while (left > 0) {
        ssize_t n = send(fd, p, left, 0);
        if (n < 0) {
            if (errno == EINTR) 
                continue;
            return -1;
        }
        if (n == 0) break;
        p += n;
        left -= (size_t)n;
    }
    return (ssize_t)(len - left);
}

static char *memmem_simple(char *haystack, size_t haystack_len, const char *needle, size_t needle_len) {
    if (needle_len == 0 || haystack_len < needle_len) 
        return NULL;

    for (size_t i = 0; i + needle_len <= haystack_len; ++i) 
        if (memcmp(haystack + i, needle, needle_len) == 0) return haystack + i;
    return NULL;
}

static char *find_host_header(char *buf, size_t len) {
    const char *needle = "Host:";
    size_t nlen = strlen(needle);

    char *p = memmem_simple(buf, len, needle, nlen);
    if (!p) 
        return NULL;

    p += nlen;
    while (p < buf + len && (*p == ' ' || *p == '\t')) 
        p++;
    return p;
}

static int create_listen_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        log_error("socket failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_error("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("bind on port %d failed: %s", port, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 64) < 0) {
        log_error("listen failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

static int connect_to_origin(const char *host, int port) {
    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        log_error("gethostbyname(%s) failed", host);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        log_error("socket for origin failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("connect_to_origin(%s:%d) failed: %s", host, port, strerror(errno));
        close(fd);
        return -1;
    }

    log_debug("connected to origin %s:%d, fd=%d", host, port, fd);
    return fd;
}

static void stream_from_cache(cache_entry_t *entry, int client_fd) {
    size_t offset = 0;
    char   tmp[4096];

    for (;;) {
        pthread_mutex_lock(&entry->lock);

        while (offset == entry->size && !entry->complete && !entry->failed) 
            pthread_cond_wait(&entry->cond, &entry->lock);

        if (entry->failed) {
            pthread_mutex_unlock(&entry->lock);
            log_error("stream_from_cache: entry failed, stop streaming");
            break;
        }

        if (offset == entry->size && entry->complete) {
            pthread_mutex_unlock(&entry->lock);
            log_debug("stream_from_cache: finished for fd=%d", client_fd);
            break;
        }

        size_t avail = entry->size - offset;
        size_t chunk = (avail < sizeof(tmp)) ? avail : sizeof(tmp);
        memcpy(tmp, entry->data + offset, chunk);
        offset += chunk;

        pthread_mutex_unlock(&entry->lock);

        if (send_all(client_fd, tmp, chunk) < 0) {
            log_error("stream_from_cache: send_all to client fd=%d failed: %s", client_fd, strerror(errno));
            if (errno == EPIPE || errno == ECONNRESET) {
                log_info("stream_from_cache: client fd=%d closed connection", client_fd);
            } else {
                log_error("stream_from_cache: send_all to client fd=%d failed: %s",
                          client_fd, strerror(errno));
            }
            break;
        }
    }
}

static void handle_client(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int cfd = ctx->client_fd;
    cache_table_t *cache = ctx->cache;
    
    log_debug("client fd=%d: handler started", cfd);

    char   buf[8192];
    size_t used = 0;

    for (;;) {
        ssize_t n = recv(cfd, buf + used, sizeof(buf) - used, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            log_error("client fd=%d: recv failed: %s", cfd, strerror(errno));
            goto cleanup;
        }

        if (n == 0) {
            log_info("client fd=%d: closed connection before headers", cfd);
            goto cleanup;
        }

        used += (size_t)n;

        if (memmem_simple(buf, used, "\r\n\r\n", 4) != NULL) 
            break;

        if (used == sizeof(buf)) {
            log_error("client fd=%d: headers too large", cfd);
            goto cleanup;
        }
    }

    char *line_end = memmem_simple(buf, used, "\r\n", 2);
    if (!line_end) {
        log_error("client fd=%d: no CRLF in request line", cfd);
        goto cleanup;
    }
    *line_end = '\0';

    char *method = buf;
    char *sp1 = strchr(method, ' ');
    if (!sp1) {
        log_error("client fd=%d: bad request line (no space after method)", cfd);
        goto cleanup;
    }
    *sp1 = '\0';

    char *path = sp1 + 1;
    char *sp2  = strchr(path, ' ');
    if (!sp2) {
        log_error("client fd=%d: bad request line (no space before HTTP)", cfd);
        goto cleanup;
    }
    *sp2 = '\0';

    if (strcmp(method, "GET") != 0) {
        log_info("client fd=%d: unsupported method '%s', closing", cfd, method);
        goto cleanup;
    }

    char *uri = path;
    if (strncmp(path, "http://", 7) == 0) {
        char *p = path + 7;
        while (*p && *p != '/') p++;
        if (*p == '\0') uri = (char *)"/";
        else uri = p;
    }

    char *headers_start = line_end + 2;
    size_t headers_len  = used - (size_t)(headers_start - buf);

    char *host_start = find_host_header(headers_start, headers_len);
    if (!host_start) {
        log_error("client fd=%d: Host header not found", cfd);
        goto cleanup;
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

    char key[512];
    snprintf(key, sizeof(key), "%s %s", host, uri);

    int am_writer = 0;
    cache_entry_t *entry = cache_start_or_join(cache, key, &am_writer);
    if (!entry) {
        log_error("client fd=%d: cache_start_or_join failed for key='%s'", cfd, key);
        goto cleanup;
    }

    log_debug("client fd=%d: key='%s', am_writer=%d", cfd, key, am_writer);

    if (!am_writer) {
        log_debug("client fd=%d: reader for key='%s', streaming from cache", cfd, key);
        stream_from_cache(entry, cfd);
        cache_release(cache, entry);
        goto cleanup;
    }

    int ofd = connect_to_origin(host, 80);
    if (ofd < 0) {
        const char *resp = "HTTP/1.0 502 Bad Gateway\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        (void)send_all(cfd, resp, strlen(resp));
        cache_failed(entry);
        cache_release(cache, entry);
        goto cleanup;
    }

    log_info("client fd=%d: writer for key='%s', origin fd=%d", cfd, key, ofd);

    char req[4096];
    int req_len = snprintf(req, sizeof(req),
                           "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                           uri, host);
    
    if (req_len <= 0 || req_len >= (int)sizeof(req)) {
        log_error("client fd=%d: failed to build HTTP/1.0 request", cfd);
        cache_failed(entry);
        cache_release(cache, entry);
        close(ofd);
        goto cleanup;
    }

    if (send_all(ofd, req, (size_t)req_len) < 0) {
        log_error("client fd=%d: send_all to origin failed: %s", cfd, strerror(errno));
        cache_failed(entry);
        cache_release(cache, entry);
        close(ofd);
        goto cleanup;
    }

    char serv_buf[8192];
    int client_alive = 1;
    for (;;) {
        ssize_t n = recv(ofd, serv_buf, sizeof(serv_buf), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            log_error("client fd=%d: recv from origin failed: %s", cfd, strerror(errno));
            cache_failed(entry);
            break;
        }
        if (n == 0) {
            log_debug("client fd=%d: origin closed connection, marking complete", cfd);
            cache_complete(entry);
            break;
        }

        if (cache_add(entry, serv_buf, (size_t)n, cache) != 0) {
            log_error("client fd=%d: cache_add failed", cfd);
            cache_failed(entry);
            break;
        }

        if (client_alive && send_all(cfd, serv_buf, (size_t)n) < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                log_info("client fd=%d: closed connection while writing, continue caching", cfd);
            } else {
                log_error("client fd=%d: send_all to client failed: %s", cfd, strerror(errno));
            }
            client_alive = 0;
        }
    }
    close(ofd);
    cache_release(cache, entry);

cleanup:
    close(cfd);
    free(ctx); 
    log_debug("client fd=%d: handler finished", cfd);
}

int proxy_run(const proxy_config_t *cfg) {
    signal(SIGPIPE, SIG_IGN);

    cache_table_t cache;
    if (cache_table_init(&cache) != 0) {
        log_error("failed to init cache table");
        return 1;
    }
    pthread_t gc_thread;
    if (pthread_create(&gc_thread, NULL, gc_thread_loop, &cache) != 0) {
        log_error("failed to create GC thread");
        cache_table_destroy(&cache);
        return 1;
    }
    pthread_detach(gc_thread);
    threadPool_t *pool = threadpoll_init(cfg->worker_count);
    if (!pool) {
        log_error("failed to create thread pool");
        cache_table_destroy(&cache);
        return 1;
    }

    int lfd = create_listen_socket(cfg->listen_port);
    if (lfd < 0) {
        log_error("failed to create listen socket on port %d", cfg->listen_port);
        threadpool_stop(pool);
        cache_table_destroy(&cache);
        return 1;
    }

    log_info("proxy listening on port %d", cfg->listen_port);

    for (;;) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        int cfd = accept(lfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            log_error("accept failed: %s", strerror(errno));
            break;
        }

        log_debug("accepted client fd=%d", cfd);

        client_ctx_t *ctx = malloc(sizeof(*ctx));
        if (!ctx) {
            log_error("malloc client_ctx failed");
            close(cfd);
            continue;
        }

        ctx->client_fd = cfd;
        ctx->cache = &cache;

        if (threadpool_submit_task(pool, handle_client, ctx) != 0) {
            log_error("threadPoolSubmit failed for client fd=%d", cfd);
            close(cfd);
            free(ctx);
            continue;
        }
    }

    close(lfd);
    threadpool_stop(pool);
    cache_table_destroy(&cache);
    log_info("proxy stopped");
    return 0;
}