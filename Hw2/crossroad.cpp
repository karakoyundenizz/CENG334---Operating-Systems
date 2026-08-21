#include <pthread.h>

#include "crossroad.h"
#include "hw2_output.h"
#include "monitor.h"

// my addition libraries
// atomic for keeping a line
#include <queue>
#include <utility>
#include <vector>

// TODO: Define your global intersection state variables here
// e.g., dynamically allocated arrays for lane queues

// WE WILL ASSUME THAT THE INITIAL ORDER MAY DEPEND ON OS SCHEDULER BUT WE NEED TO KEEP THAT ORDER PROTECTED
// so if not turn these back into atomic arrays

// general shared arrival time CHECK THE MAXIMUM CAR NUMBER

using MinHeapQueue = std::priority_queue<
    std::pair<unsigned, Monitor::Condition*>,
    std::vector<std::pair<unsigned, Monitor::Condition*>>,
    std::greater<std::pair<unsigned, Monitor::Condition*>>>;



class Controller : public Monitor {
        std::vector<std::vector<MinHeapQueue>> waiting_list;
        std::vector<std::vector<std::pair<Direction, int>>> priority_buckets;
        std::vector<std::vector<int>> priority_array;
        std::vector<std::vector<bool>> is_occupied;

        unsigned global_time;
        int active_dir;
        int active_car;
        int num_h_lanes, num_v_lanes;
        int max_pri;

       public:
        Controller(int h_lanes, int v_lanes, int* h_priorities, int* v_priorities) {
                waiting_list.resize(2);
                priority_array.resize(2);
                is_occupied.resize(2);

                waiting_list[0].resize(h_lanes);
                waiting_list[1].resize(v_lanes);
                priority_array[0].resize(h_lanes, 0);
                priority_array[1].resize(v_lanes, 0);
                is_occupied[0].resize(h_lanes, false);
                is_occupied[1].resize(v_lanes, false);


                num_h_lanes = h_lanes;
                num_v_lanes = v_lanes;

                active_car = 0;
                active_dir = -1;

                global_time = 0;

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
        }




        bool is_higher_car_waiting(Direction dir, int lane, unsigned our_arrival_time) {
                // TODO do we need to wait for parallel prior cars too or not?
                int my_pri = priority_array[dir][lane];

                for (int i = max_pri; i > my_pri; i--) {
                        for (std::pair<Direction, int> j : priority_buckets[i]) {
                                Direction lane_dir = j.first;
                                int lane_num = j.second;
                                // either higher priority
                                if (lane_dir != dir && !waiting_list[lane_dir][lane_num].empty()) {
                                        return true;
                                }
                        }
                }

                // or same priority but earlier arrival time
                for (std::pair<Direction, int> j : priority_buckets[my_pri]) {
                        Direction lane_dir = j.first;
                        int lane_num = j.second;



                        if (lane_dir != dir && !waiting_list[lane_dir][lane_num].empty()) {
                                unsigned opp_arrival_time = waiting_list[lane_dir][lane_num].top().first;
                                if (our_arrival_time > opp_arrival_time) {
                                        return true;
                                }
                        }
                }

                return false;
        }

        int find_highest_pri() {
                for (int i = max_pri; i >= 0; i--) {
                        unsigned earliest = (unsigned)-1;
                        int highest_dir = -1;
                        for (std::pair<Direction, int> j : priority_buckets[i]) {
                                Direction lane_dir = j.first;
                                int lane_num = j.second;
                                if (!waiting_list[lane_dir][lane_num].empty() &&
                                    waiting_list[lane_dir][lane_num].top().first < earliest) {
                                        highest_dir = lane_dir;
                                        earliest = waiting_list[lane_dir][lane_num].top().first;
                                }
                        }
                        if (highest_dir != -1) {
                                return highest_dir;
                        }
                }
                return -1;
        }

        void get_in(int car_id, Direction dir, int lane) {
                __synchronized__;

                // 1. Log arrival
                hw2_write_output(car_id, ET_ARRIVE, dir, lane);

                global_time++;
                Condition* our_turn = new Condition(this);
                unsigned our_arrival_time = global_time;
                waiting_list[dir][lane].push(std::make_pair(global_time, our_turn));

                while ((active_dir != dir && active_car > 0) ||
                       waiting_list[dir][lane].top().first != our_arrival_time ||
                       is_occupied[dir][lane] ||
                       is_higher_car_waiting(dir, lane, our_arrival_time)) {
                        // wwait on our_turn condition
                        our_turn->wait();
                }

                // our turn
                // 3. Log entry
                hw2_write_output(car_id, ET_ENTER, dir, lane);
                is_occupied[dir][lane] = true;
                waiting_list[dir][lane].pop();
                delete our_turn;
                active_dir = dir;
                active_car++;
        }

        void get_out(int car_id, Direction dir, int lane) {
                __synchronized__;
                active_car--;
                is_occupied[dir][lane] = false;
                // 2. Log exit
                hw2_write_output(car_id, ET_EXIT, dir, lane);
                if (active_car == 0) {
                        active_dir = -1;

                        // find the next highest priority
                        int priority_dir = find_highest_pri();

                        if (priority_dir != -1) {
                                // notify all the waiting lanes in that direction
                                int num_lanes = (priority_dir == 0) ? num_h_lanes : num_v_lanes;
                                for (int i = 0; i < num_lanes; i++) {
                                        // TODO notify that lane but i think we need to notify bassed on priority order
                                        if (!waiting_list[priority_dir][i].empty()) {
                                                Condition* next_car = waiting_list[priority_dir][i].top().second;
                                                next_car->notify();
                                        }
                                }
                        }
                } else {
                        // still cars in the intersection with current direction
                        // if there is car right behind me, check if it is good to go if it is wake it up else dont do anything
                        if (!waiting_list[dir][lane].empty() && !is_higher_car_waiting(dir, lane, waiting_list[dir][lane].top().first)) {
                                Condition* next_car = waiting_list[dir][lane].top().second;
                                next_car->notify();
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



        traffic_police = new Controller(h_lanes, v_lanes, h_pri, v_pri);





        // TODO: Initialize your mutexes and condition variables
}

void arrive_crossroad(int car_id, Direction dir, int lane) {
        // 2. Wait condition (Strict FIFO + Group Mutual Exclusion)




        // save the line number immediately
        // increment the end ticket so that in controller
        // we know whether we reached the last car or there is more car at lane

        traffic_police->get_in(car_id, dir, lane);




        // lock our lane and all the orthogonal lanes

        // when leaving notify all the higher priority lanes' locks first and then low prioritie
}

void exit_crossroad(int car_id, Direction dir, int lane) {
        // 1. Update state
        traffic_police->get_out(car_id, dir, lane);


        // 3. Signal waiting threads
}