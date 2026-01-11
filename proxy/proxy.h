#ifndef CACHE_PROXY_H
#define CACHE_PROXY_H

#include "threadpool/threadpool.h"

typedef struct proxy_config {
    int listen_port;
    int worker_count;
} proxy_config_t;

int proxy_run(const proxy_config_t *cfg);

#endif