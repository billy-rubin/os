#include "proxy.h"
#include "logger/logger.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
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


static void handle_client(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int cfd = ctx->client_fd;
    free(ctx);

    log_debug("client fd=%d: handler started", cfd);

    char buf[4096];

    ssize_t n = recv(cfd, buf, sizeof(buf), 0);
    if (n < 0) 
        log_error("client fd=%d: recv failed: %s", cfd, strerror(errno));
    else if (n == 0) 
        log_info("client fd=%d: closed connection immediately", cfd);
    else 
        log_debug("client fd=%d: received %zd bytes (first bytes of request)", cfd, n);
    
    close(cfd);
    log_debug("client fd=%d: handler finished", cfd);
}


int proxy_run(const proxy_config_t *cfg) {
    signal(SIGPIPE, SIG_IGN);

    threadPool_t *pool = threadpoll_init(cfg->worker_count);
    if (!pool) {
        log_error("failed to create thread pool");
        return 1;
    }

    int lfd = create_listen_socket(cfg->listen_port);
    if (lfd < 0) {
        log_error("failed to create listen socket on port %d", cfg->listen_port);
        threadpool_stop(pool);
        return 1;
    }

    log_info("proxy listening on port %d", cfg->listen_port);

    for (;;) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        int cfd = accept(lfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (cfd < 0) {
            if (errno == EINTR) 
                continue;
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

        if (threadpool_submit_task(pool, handle_client, ctx) != 0) {
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