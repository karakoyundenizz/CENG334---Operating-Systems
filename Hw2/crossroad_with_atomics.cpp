#include "crossroad.h"

#include <pthread.h>

#include "hw2_output.h"
#include "monitor.h"

// my addition libraries
// atomic for keeping a line
#include <atomic>
#include <utility>
#include <vector>
// TODO: Define your global intersection state variables here
// e.g., dynamically allocated arrays for lane queues

// WE WILL ASSUME THAT THE INITIAL ORDER MAY DEPEND ON OS SCHEDULER BUT WE NEED TO KEEP THAT ORDER PROTECTED
// so if not turn these back into atomic arrays
std::atomic<unsigned>* next_ticket[2];
// general shared arrival time CHECK THE MAXIMUM CAR NUMBER
std::atomic<unsigned> arrival_time;




#define Max_car 100

class Controller : public Monitor {
        Condition*** lanes[2];
        std::vector<std::vector<unsigned>> next_to_serve;
        int active_dir;
        int active_car;
        std::vector<std::vector<std::pair<Direction, int>>> priority_buckets;
        std::vector<std::vector<int>> priority_array;
        int num_h_lanes, num_v_lanes;
        int max_pri;

       public:
        Controller(int h_lanes, int v_lanes, int* h_priorities, int* v_priorities) {
                next_to_serve.resize(2);
                priority_array.resize(2);

                next_to_serve[0].resize(h_lanes, 0);
                next_to_serve[1].resize(v_lanes, 0);
                priority_array[0].resize(h_lanes, 0);
                priority_array[1].resize(v_lanes, 0);

                num_h_lanes = h_lanes;
                num_v_lanes = v_lanes;

                active_car = 0;
                active_dir = -1;



                max_pri = 0;
                int min_pri = (1 << 30);
                for (int i = 0; i < h_lanes; i++) {
                        if (h_priorities[i] > max_pri) max_pri = h_priorities[i];
                        if (h_priorities[i] < min_pri) min_pri = h_priorities[i];
                }
                for (int i = 0; i < v_lanes; i++) {
                        if (v_priorities[i] > max_pri) max_pri = v_priorities[i];
                        if (v_priorities[i] < min_pri) min_pri = v_priorities[i];
                }

                int pri_range = max_pri - min_pri, pri = 0;
                priority_buckets.resize(pri_range + 1);

                for (int i = 0; i < h_lanes; i++) {
                        pri = h_priorities[i] - min_pri;
                        priority_array[0][i] = pri;
                        priority_buckets[pri].push_back(std::make_pair(Direction::DIR_HORIZONTAL, i));
                }

                for (int i = 0; i < v_lanes; i++) {
                        pri = v_priorities[i] - min_pri;
                        priority_array[1][i] = pri;
                        priority_buckets[pri].push_back(std::make_pair(Direction::DIR_VERTICAL, i));
                }

                max_pri -= min_pri;



                lanes[0] = new Condition**[h_lanes];
                lanes[1] = new Condition**[v_lanes];

                for (int i = 0; i < h_lanes; i++) {
                        lanes[0][i] = new Condition*[Max_car];
                        for (int j = 0; j < Max_car; j++) {
                                lanes[0][i][j] = new Condition(this);
                        }
                }

                for (int i = 0; i < v_lanes; i++) {
                        lanes[1][i] = new Condition*[Max_car];
                        for (int j = 0; j < Max_car; j++) {
                                lanes[1][i][j] = new Condition(this);
                        }
                }
        }

        bool is_higher_car_waiting(Direction dir, int lane, int our_turn) {
                int my_pri = priority_array[dir][lane];

                for (int i = max_pri; i > my_pri; i--) {
                        for (std::pair<Direction, int> j : priority_buckets[i]) {
                                Direction lane_dir = j.first;
                                int lane_num = j.second;

                                // either orthogonal direction or the same lane  but we are not the next
                                if (lane_dir != dir && next_to_serve[lane_dir][lane_num] < next_ticket[lane_dir][lane_num]) {
                                        return true;
                                }
                        }
                }
                return false;
        }

        int find_highest_pri() {
                for (int i = max_pri; i >= 0; i--) {
                        for (std::pair<Direction, int> j : priority_buckets[i]) {
                                Direction lane_dir = j.first;
                                int lane_num = j.second;
                                if (next_to_serve[lane_dir][lane_num] < next_ticket[lane_dir][lane_num]) {
                                        return lane_dir;
                                }
                        }
                }
                return -1;
        }

        void get_in(Direction dir, int lane, int our_turn) {
                __synchronized__;

                while ((active_dir != dir && active_car > 0) ||
                       our_turn != next_to_serve[dir][lane] ||
                       is_higher_car_waiting(dir, lane, our_turn)) {
                        // wwait on the lane condition
                        int condition_indx = our_turn % Max_car;
                        lanes[dir][lane][condition_indx]->wait();
                }

                // our turn
                active_dir = dir;
                active_car++;
        }

        void get_out(Direction dir, int lane) {
                __synchronized__;

                active_car--;
                next_to_serve[dir][lane]++;
                if (active_car == 0) {
                        active_dir = -1;



                        // find the next highest priority
                        int priority_dir = find_highest_pri();

                        if (priority_dir != -1) {
                                // notify all the waiting lanes in that direction
                                int num_lanes = (priority_dir == 0) ? num_h_lanes : num_v_lanes;
                                for (int i = 0; i < num_lanes; i++) {
                                        // notify that lane but i think we need to notify bassed on priority order
                                        if (next_to_serve[priority_dir][i] < next_ticket[priority_dir][i]) {
                                                unsigned next_car = next_to_serve[priority_dir][i];
                                                int condition_indx = next_car % Max_car;
                                                lanes[priority_dir][i][condition_indx]->notify();
                                        }
                                }
                        }
                } else {
                        // if there is car right behind me, check if it is good to go if it is wake it up else dont do anything
                        int car_behind = next_to_serve[dir][lane];
                        if (car_behind < next_ticket[dir][lane] && !is_higher_car_waiting(dir, lane, car_behind)) {
                                int condition_indx = car_behind % Max_car;
                                lanes[dir][lane][condition_indx]->notify();
                        }
                }
        }
};






// there is only 1 intersection


Controller* traffic_police;







void initialize_crossroad(int h_lanes, int v_lanes, int* h_pri, int* v_pri) {
        // TODO: Dynamically allocate your state arrays based on the lane counts

        // initialize the next_ticket holder arrays for each lane
        // horizontal
        next_ticket[0] = new std::atomic<unsigned>[h_lanes]();
        // vertical
        next_ticket[1] = new std::atomic<unsigned>[v_lanes]();


        traffic_police = new Controller(h_lanes, v_lanes, h_pri, v_pri);





        // TODO: Initialize your mutexes and condition variables
}

void arrive_crossroad(int car_id, Direction dir, int lane) {
        // 1. Log arrival
        hw2_write_output(car_id, ET_ARRIVE, dir, lane);

        // 2. Wait condition (Strict FIFO + Group Mutual Exclusion)




        // save the line number immediately
        unsigned our_turn = next_ticket[dir][lane]++;

        // increment the end ticket so that in controller
        // we know whether we reached the last car or there is more car at lane

        traffic_police->get_in(dir, lane, our_turn);




        // lock our lane and all the orthogonal lanes

        // when leaving notify all the higher priority lanes' locks first and then low priorities





        // 3. Log entry
        hw2_write_output(car_id, ET_ENTER, dir, lane);
}

void exit_crossroad(int car_id, Direction dir, int lane) {
        // 1. Update state
        traffic_police->get_out(dir, lane);
        // 2. Log exit
        hw2_write_output(car_id, ET_EXIT, dir, lane);

        // 3. Signal waiting threads
}