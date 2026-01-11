#include <stdio.h>
#include <stdlib.h>

#include "proxy.h"
#include "logger/logger.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    logger_init(LOG_DEBUG);

    proxy_config_t cfg;
    cfg.listen_port = atoi(argv[1]);
    cfg.worker_count = 8;

    if (cfg.listen_port <= 0 || cfg.listen_port > 65535) {
        log_error("Invalid port: %d", cfg.listen_port);
        logger_finalize();
        return 1;
    }
    log_info("starting proxy on port %d with %d workers", cfg.listen_port, cfg.worker_count);

    int rc = proxy_run(&cfg);
    if (rc != 0) 
        log_error("proxy failed with code %d", rc);

    logger_finalize();
    return rc;
}