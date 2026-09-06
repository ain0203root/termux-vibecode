#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double ns_since(const struct timespec *a, const struct timespec *b) {
    return (double)(b->tv_sec - a->tv_sec) * 1e9 + (double)(b->tv_nsec - a->tv_nsec);
}

int main(int argc, char **argv) {
    long n = argc > 1 ? strtol(argv[1], NULL, 10) : 1000000L;
    volatile unsigned long long x = 0;
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC_RAW, &a);
    for (long i = 0; i < n; ++i) x += (unsigned long long)i;
    clock_gettime(CLOCK_MONOTONIC_RAW, &b);
    double ns = ns_since(&a, &b);
    printf("iterations=%ld\nns=%.0f\nns_per_iter=%.3f\nchecksum=%llu\n", n, ns, ns / (double)n, x);
    return 0;
}
