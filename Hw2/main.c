#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "crossroad.h"
#include "hw2_output.h"

typedef struct {
    int car_id;
    int pass_time;
    int return_time;
    int pattern_length;
    Direction* pattern_dirs;
    int* pattern_lanes;
    int repeat_count;
} Car;

void* carThread(void* arg) {
    Car* car = (Car*)arg;

    for (int i = 0; i < car->repeat_count; i++) {
        for (int j = 0; j < car->pattern_length; j++) {

            // Sleep to simulate driving around before arriving
            if (car->return_time > 0) {
                usleep((rand() % car->return_time) + car->return_time);
            }

            arrive_crossroad(car->car_id, car->pattern_dirs[j], car->pattern_lanes[j]);

            // Sleep to simulate the physical time taken to pass through
            if (car->pass_time > 0) {
                usleep((rand() % car->pass_time) + car->pass_time);
            }

            exit_crossroad(car->car_id, car->pattern_dirs[j], car->pattern_lanes[j]);
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: \n%s <random_seed> <testcase_file>\n%s <testcase_file>\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 3) {
        srand(atoi(argv[1]));
    }

    hw2_init_output();

    const char* filename = (argc == 3) ? argv[2] : argv[1];
    FILE* infile = fopen(filename, "r");
    if (!infile) {
        fprintf(stderr, "Error: Could not open testcase file %s\n", filename);
        return EXIT_FAILURE;
    }

    int h_lanes, v_lanes, car_count;
    if (fscanf(infile, "%d %d %d", &h_lanes, &v_lanes, &car_count) != 3) {
        fprintf(stderr, "Error: Invalid testcase header format.\n");
        fclose(infile);
        return EXIT_FAILURE;
    }

    // --- PARSE PRIORITIES ---
    int* h_pri = (int*)malloc(h_lanes * sizeof(int));
    for (int i = 0; i < h_lanes; i++) {
        fscanf(infile, "%d", &h_pri[i]);
    }

    int* v_pri = (int*)malloc(v_lanes * sizeof(int));
    for (int i = 0; i < v_lanes; i++) {
        fscanf(infile, "%d", &v_pri[i]);
    }

    // Initialize with the API
    initialize_crossroad(h_lanes, v_lanes, h_pri, v_pri);

    Car* cars = (Car*)malloc(car_count * sizeof(Car));
    pthread_t* threads = (pthread_t*)malloc(car_count * sizeof(pthread_t));

    // --- PARSE CAR ROUTING ---
    for (int i = 0; i < car_count; i++) {
        fscanf(infile, "%d %d %d %d %d",
            &cars[i].car_id,
            &cars[i].pass_time,
            &cars[i].return_time,
            &cars[i].repeat_count,
            &cars[i].pattern_length);

        cars[i].pattern_dirs = (Direction*)malloc(cars[i].pattern_length * sizeof(Direction));
        cars[i].pattern_lanes = (int*)malloc(cars[i].pattern_length * sizeof(int));

        for (int j = 0; j < cars[i].pattern_length; j++) {
            char dir_char;
            // The leading space in " %c" is required to skip newlines/spaces
            fscanf(infile, " %c %d", &dir_char, &cars[i].pattern_lanes[j]);

            cars[i].pattern_dirs[j] = (dir_char == 'H' || dir_char == 'h') ? DIR_HORIZONTAL : DIR_VERTICAL;
        }
    }

    fclose(infile);

    // Start threads
    for (int i = 0; i < car_count; i++) {
        pthread_create(&threads[i], NULL, carThread, (void*)&cars[i]);
    }

    // Wait for all cars to complete their routes
    for (int i = 0; i < car_count; i++) {
        pthread_join(threads[i], NULL);
    }

    // Clean up memory
    for (int i = 0; i < car_count; i++) {
        free(cars[i].pattern_dirs);
        free(cars[i].pattern_lanes);
    }
    free(cars);
    free(threads);
    free(h_pri);
    free(v_pri);

    return EXIT_SUCCESS;
}