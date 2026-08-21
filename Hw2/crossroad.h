#ifndef CROSSROAD_H_
#define CROSSROAD_H_

#include "hw2_output.h"

#ifdef __cplusplus
extern "C" {
#endif

	// Initializes any mutexes, condition variables, and state arrays.
	// Passed the total number of lanes in each direction and the total cars.
	void initialize_crossroad(int h_lanes, int v_lanes, int* h_pri, int* v_pri);

	// Handles the arrival, queuing, and entry of a car.
	void arrive_crossroad(int car_id, Direction dir, int lane);

	// Handles the departure of a car and wakes up any waiting threads.
	void exit_crossroad(int car_id, Direction dir, int lane);

#ifdef __cplusplus
}
#endif

#endif