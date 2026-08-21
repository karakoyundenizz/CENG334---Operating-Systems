#include "hw2_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>

static struct timeval g_start_time;

void hw2_init_output(void) {
    gettimeofday(&g_start_time, NULL);
}

static long get_timestamp(void) {
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    return (cur_time.tv_sec - g_start_time.tv_sec) * 1000000
        + (cur_time.tv_usec - g_start_time.tv_usec);
}

void hw2_write_output(int car_id, EventType event, Direction dir, int lane) {
    static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

    if (g_start_time.tv_sec == 0 && g_start_time.tv_usec == 0) {
        fprintf(stderr, "FATAL: hw2_init_output() was not called!\n");
        exit(EXIT_FAILURE);
    }

    // Map the safely prefixed enums to the parser-friendly characters
    char event_char = (event == ET_ARRIVE) ? 'A' : (event == ET_ENTER) ? 'E' : 'X';
    char dir_char = (dir == DIR_HORIZONTAL) ? 'H' : 'V';

    // Lock the output stream to prevent tearing
    pthread_mutex_lock(&mut);

    // Format: [TIMESTAMP] [EVENT] [CAR_ID] [DIRECTION][LANE]
    // Example: 1045000 E 3 H1
    printf("%ld %c %d %c%d\n", get_timestamp(), event_char, car_id, dir_char, lane);

    pthread_mutex_unlock(&mut);
}
