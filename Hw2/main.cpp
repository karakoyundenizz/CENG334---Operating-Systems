#include <iostream>
#include <fstream>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include "crossroad.h"
#include "hw2_output.h"

struct Car {
    int car_id;
    int pass_time;
    int return_time;
    int pattern_length;
    Direction* pattern_dirs;
    int* pattern_lanes;
    int repeat_count;
};

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
        std::cerr << "Usage: \n" << argv[0] << " <random_seed> <testcase_file>\n" << argv[0] << " <testcase_file>\n";
        return EXIT_FAILURE;
    }

    if(argc == 3)
        srand(atoi(argv[1]));
    hw2_init_output();

    const char* filename = (argc == 3) ? argv[2] : argv[1];
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open testcase file " << filename << "\n";
        return EXIT_FAILURE;
    }

    int h_lanes, v_lanes, car_count;
    if (!(infile >> h_lanes >> v_lanes >> car_count)) {
        std::cerr << "Error: Invalid testcase header format.\n";
        return EXIT_FAILURE;
    }

    // --- PARSE PRIORITIES ---
    int* h_pri = new int[h_lanes];
    for (int i = 0; i < h_lanes; i++) {
        infile >> h_pri[i];
    }

    int* v_pri = new int[v_lanes];
    for (int i = 0; i < v_lanes; i++) {
        infile >> v_pri[i];
    }

    // Initialize with the new API
    initialize_crossroad(h_lanes, v_lanes, h_pri, v_pri);

    Car* cars = new Car[car_count];
    pthread_t* threads = new pthread_t[car_count];

    // --- PARSE CAR ROUTING ---
    for (int i = 0; i < car_count; i++) {
        infile >> cars[i].car_id
            >> cars[i].pass_time
            >> cars[i].return_time
            >> cars[i].repeat_count
            >> cars[i].pattern_length;

        cars[i].pattern_dirs = new Direction[cars[i].pattern_length];
        cars[i].pattern_lanes = new int[cars[i].pattern_length];

        for (int j = 0; j < cars[i].pattern_length; j++) {
            char dir_char;
            infile >> dir_char >> cars[i].pattern_lanes[j];

            cars[i].pattern_dirs[j] = (dir_char == 'H' || dir_char == 'h') ? DIR_HORIZONTAL : DIR_VERTICAL;
        }
    }

    infile.close();

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
        delete[] cars[i].pattern_dirs;
        delete[] cars[i].pattern_lanes;
    }
    delete[] cars;
    delete[] threads;
    delete[] h_pri;
    delete[] v_pri;

    return EXIT_SUCCESS;
}