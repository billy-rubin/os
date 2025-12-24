#include <stdio.h>

#include "proxy.h"
#include "logger/logger.h"

int main(void) {
    logger_init(LOG_DEBUG);

    proxy_config_t cfg;
    cfg.listen_port = 8080;
    cfg.worker_count = 8;

    log_info("starting proxy on port %d with %d workers", cfg.listen_port, cfg.worker_count);

    int rc = proxy_run(&cfg);
    if (rc != 0) log_error("proxy failed with code %d", rc);

    logger_finalize();
    return rc;
}