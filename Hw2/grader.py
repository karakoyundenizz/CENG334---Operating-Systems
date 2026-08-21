import numpy as np
from enum import Enum, unique, auto
import sys
import os
from dataclasses import dataclass, field


@unique
class Direction(Enum):
    HORIZONTAL = auto()
    VERTICAL = auto()


@unique
class Problem(Enum):
    NONE = auto()
    REAR_END = auto()
    T_BONE = auto()
    FIFO_VIOLATION = auto()
    PRIORITY_VIOLATION = auto()
    PRIORITY_FIFO_VIOLATION = auto()
    NOT_WAITING = auto()
    NOT_DRIVING = auto()
    WRONG_LANE = auto()
    DUPLICATE_CAR = auto()
    BUSY_WAIT = auto()


@dataclass
class PrintConfig:
    """Configuration for when to print the visual ASCII intersection state."""
    every_event: bool = False
    specific_event_indices: list[int] = field(default_factory=list)     # e.g., [1, 5, 10] for the 1st, 5th, and 10th events
    time_range: tuple[int, int] = None                                  # e.g., (100000, 200000)
    on_violation: bool = False                                          # Prints Before & After states


class Intersection:
    def __init__(self, h: int, v: int, h_pri: list[int], v_pri: list[int]):
        self.h = h
        self.v = v
        self.waiting: dict[tuple[Direction, int], list[int]] = {}
        self.driving: dict[tuple[Direction, int], list[int]] = {}

        for i in range(h):
            self.waiting[(Direction.HORIZONTAL, i)] = []
            self.driving[(Direction.HORIZONTAL, i)] = []
        for i in range(v):
            self.waiting[(Direction.VERTICAL, i)] = []
            self.driving[(Direction.VERTICAL, i)] = []

        self.grid = np.zeros((h, v), dtype=int)

        self.priorities: dict[tuple[Direction, int], int] = {}
        for i in range(h):
            self.priorities[(Direction.HORIZONTAL, i)] = h_pri[i]
        for i in range(v):
            self.priorities[(Direction.VERTICAL, i)] = v_pri[i]

        self.global_clock = 0
        self.arrival_times: dict[int, int] = {}

    def resetGrid(self):
        self.grid = np.zeros((self.h, self.v), dtype=int)
        for h in range(self.h):
            self.grid[h, :] += len(self.driving[(Direction.HORIZONTAL, h)])
        for v in range(self.v):
            self.grid[:, v] += len(self.driving[(Direction.VERTICAL, v)])

    def arrive(self, car: int, direct: Direction, lane: int) -> list[tuple[Problem, list]]:
        retval = []
        if (direct, lane) not in self.driving or (direct, lane) not in self.waiting:
            retval.append((Problem.WRONG_LANE, [direct, lane]))
        for key in self.waiting.keys():
            if car in self.waiting[key]:
                retval.append((Problem.DUPLICATE_CAR, [0, key, self.waiting[key]]))

        for key in self.driving.keys():
            if car in self.driving[key]:
                retval.append((Problem.DUPLICATE_CAR, [1, key, self.driving[key]]))

        if len(retval) > 0:
            return retval

        self.waiting[(direct, lane)].append(car)
        self.arrival_times[car] = self.global_clock
        self.global_clock += 1

        if len(retval) == 0:
            retval.append((Problem.NONE, []))

        return retval

    def enter(self, car: int, direct: Direction, lane: int) -> list[tuple[Problem, list]]:
        retval = []

        if (direct, lane) not in self.driving or (direct, lane) not in self.waiting:
            retval.append((Problem.WRONG_LANE, [direct, lane]))
            return retval

        if car not in self.waiting[(direct, lane)]:
            retval.append((Problem.NOT_WAITING, self.waiting[(direct, lane)]))
            return retval

        if self.waiting[(direct, lane)][0] != car:
            retval.append((Problem.FIFO_VIOLATION, self.waiting[(direct, lane)]))

        my_pri = self.priorities[(direct, lane)]
        my_time = self.arrival_times[car]

        for (other_d, other_l), queue in self.waiting.items():
            if other_d != direct and len(queue) > 0:
                other_car = queue[0]
                other_pri = self.priorities[(other_d, other_l)]
                other_time = self.arrival_times[other_car]

                if other_pri > my_pri:
                    retval.append((Problem.PRIORITY_VIOLATION, [(direct, lane), car, (other_d, other_l), other_car]))

                elif other_pri == my_pri and other_time < my_time:
                    retval.append(
                        (Problem.PRIORITY_FIFO_VIOLATION, [(direct, lane), car, (other_d, other_l), other_car]))

        self.waiting[(direct, lane)].remove(car)
        self.driving[(direct, lane)].append(car)

        if len(self.driving[(direct, lane)]) > 1:
            retval.append((Problem.REAR_END, [(direct, lane), self.driving[(direct, lane)]]))

        for d, l in self.driving.keys():
            if d != direct and len(self.driving[(d, l)]) != 0:
                retval.append((Problem.T_BONE, [(d, l), self.driving[d, l]]))

        self.resetGrid()

        if len(retval) == 0:
            retval.append((Problem.NONE, []))

        return retval

    def exit(self,  car: int, direct: Direction, lane: int) -> list[tuple[Problem, list]]:
        retval = []

        if (direct, lane) not in self.driving or (direct, lane) not in self.waiting:
            retval.append((Problem.WRONG_LANE, [direct, lane]))
            return retval

        if car not in self.driving[(direct, lane)]:
            retval.append((Problem.NOT_DRIVING, self.driving[(direct, lane)]))
            return retval

        self.driving[(direct, lane)].remove(car)
        
        self.resetGrid()

        if len(retval) == 0:
            retval.append((Problem.NONE, []))

        return retval
    
    def getCanEnter(self):
        retval = []

        for (d, l), queue in self.waiting.items():
            if not queue:
                continue

            car = queue[0]
            my_pri = self.priorities[(d, l)]
            my_time = self.arrival_times[car]

            path_clear = True
            if d == Direction.HORIZONTAL and self.grid[l, :].sum() > 0:
                path_clear = False
            elif d == Direction.VERTICAL and self.grid[:, l].sum() > 0:
                path_clear = False

            if not path_clear:
                continue

            priority_clear = True
            for (other_d, other_l), o_queue in self.waiting.items():
                if other_d != d and len(o_queue) > 0:
                    other_car = o_queue[0]
                    other_pri = self.priorities[(other_d, other_l)]
                    other_time = self.arrival_times[other_car]

                    if other_pri > my_pri:
                        priority_clear = False
                        break
                    elif other_pri == my_pri and other_time < my_time:
                        priority_clear = False
                        break

            if priority_clear:
                retval.append((d, l, car))

        return retval

    def render(self, current_time=None, event_info=None) -> str:
        """Renders an ASCII representation of the intersection similar to the PDF."""
        lines = []
        header = f"====== CROSSROAD STATE "
        if current_time is not None:
            header += f"[@ T={current_time}] "
        if event_info is not None:
            header += f"| Action: {event_info} "
        header += "=" * max(0, 55 - len(header))
        lines.append(header)

        v_header = "             "
        v_arrows = "             "
        for v in range(self.v):
            pri = self.priorities[(Direction.VERTICAL, v)]
            v_header += f" V{v}(P:{pri}) ".center(11)
            v_arrows += "    v      ".center(11)
        lines.append(v_header)
        lines.append(v_arrows)

        border = "            +" + "---------+" * self.v
        lines.append(border)

        for h in reversed(range(self.h)):
            row_pri = self.priorities[(Direction.HORIZONTAL, h)]
            wait_q = self.waiting[(Direction.HORIZONTAL, h)]
            drive_q = self.driving[(Direction.HORIZONTAL, h)]

            wait_str = f"W:{wait_q}"[:10].rjust(10)
            row_lbl = f"H{h}(P:{row_pri}) >".rjust(11)

            row_cells = ""
            for v in range(self.v):
                cell_cars = self.driving[(Direction.HORIZONTAL, h)] + self.driving[(Direction.VERTICAL, v)]
                if len(cell_cars) == 0:
                    cell_str = "         "
                elif len(cell_cars) == 1:
                    cell_str = f"[C{cell_cars[0]}]".center(9)
                else:
                    cell_str = " !!X!! ".center(9)
                row_cells += f"{cell_str}|"

            lines.append(f"{row_lbl} |{row_cells} D:{drive_q}")
            lines.append(f" {wait_str} |" + "         |" * self.v)
            lines.append(border)

        v_wait_row = "             "
        for v in range(self.v):
            wq = self.waiting[(Direction.VERTICAL, v)]
            v_wait_row += f"W:{wq}"[:9].center(10) + " "
        lines.append(v_wait_row)

        v_drive_row = "             "
        for v in range(self.v):
            dq = self.driving[(Direction.VERTICAL, v)]
            v_drive_row += f"D:{dq}"[:9].center(10) + " "
        lines.append(v_drive_row)

        lines.append("=" * max(55, len(border)))
        return "\n".join(lines)


def parse_testcase(testcase_path: str):
    """
    Parses the testcase to extract grid dimensions, priorities,
    and the expected number of events per car.
    """
    with open(testcase_path, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]

    h_lanes, v_lanes, car_count = map(int, lines[0].split())
    h_pri = list(map(int, lines[1].split()))
    v_pri = list(map(int, lines[2].split()))

    expected_counts = {}

    # Parse each car's routing data starting from line 3
    for i in range(3, 3 + car_count):
        parts = lines[i].split()
        car_id = int(parts[0])
        repeat_count = int(parts[3])
        pattern_length = int(parts[4])

        # Each car must do a full Arrive -> Enter -> Exit cycle for every step of its route
        total_events = repeat_count * pattern_length
        expected_counts[car_id] = {
            'A': total_events,
            'E': total_events,
            'X': total_events
        }

    return h_lanes, v_lanes, car_count, h_pri, v_pri, expected_counts


def evaluate_student_output(testcase_path: str, student_output_path: str, p_config: PrintConfig = None):
    if not os.path.exists(student_output_path):
        return ["CRITICAL: Output file not found. Did the program crash?"], []

    p_config = p_config or PrintConfig()

    h_lanes, v_lanes, car_count, h_pri, v_pri, expected_counts = parse_testcase(testcase_path)
    intersection = Intersection(h_lanes, v_lanes, h_pri, v_pri)
    actual_counts = {i: {'A': 0, 'E': 0, 'X': 0} for i in range(car_count)}

    logic_violations = []

    # Store the initial blank state in case the first event is a violation
    prev_state_str = intersection.render(current_time=0, event_info="INIT")

    with open(student_output_path, 'r') as f:
        for line_num, line in enumerate(f, 1):
            parts = line.strip().split()
            if len(parts) != 4:
                logic_violations.append(f"Event {line_num}: Malformed output -> '{line.strip()}'")
                continue

            timestamp = int(parts[0])
            event_char = parts[1]
            car_id = int(parts[2])
            dir_lane = parts[3]

            dir_char = dir_lane[0]
            lane_idx = int(dir_lane[1:])
            direct = Direction.HORIZONTAL if dir_char == 'H' else Direction.VERTICAL

            if car_id in actual_counts and event_char in actual_counts[car_id]:
                actual_counts[car_id][event_char] += 1
            else:
                logic_violations.append(f"Event {line_num}: Unknown car_id {car_id} or event {event_char}.")
                continue

            # Process Event
            problems = []
            if event_char == 'A':
                problems = intersection.arrive(car_id, direct, lane_idx)
            elif event_char == 'E':
                problems = intersection.enter(car_id, direct, lane_idx)
            elif event_char == 'X':
                problems = intersection.exit(car_id, direct, lane_idx)

            # Generate new visual state with the Event Index
            event_str = f"Event {line_num}: {event_char} Car {car_id} -> {dir_lane}"
            current_state_str = intersection.render(current_time=timestamp, event_info=event_str)

            # Check for violations
            is_violation = False
            for prob, details in problems:
                if prob != Problem.NONE:
                    is_violation = True
                    logic_violations.append(
                        f"Event {line_num} [Time: {timestamp}]: {prob.name} triggered by {event_str}. Details: {details}"
                    )

            # Trigger Visualization Logic
            should_print = False
            if p_config.every_event:
                should_print = True
            elif line_num in p_config.specific_event_indices:
                should_print = True
            elif p_config.time_range and (p_config.time_range[0] <= timestamp <= p_config.time_range[1]):
                should_print = True

            if p_config.on_violation and is_violation:
                print(f"\n\n{'=' * 25} VIOLATION DETECTED {'=' * 25}")
                print(f"Time: {timestamp} | Trigger: {event_str}")
                print("\n--- STATE BEFORE ---")
                print(prev_state_str)
                print("\n--- STATE AFTER (CRASH/VIOLATION) ---")
                print(current_state_str)
                print(f"{'=' * 72}\n")
            elif should_print:
                print("\n" + current_state_str)

            prev_state_str = current_state_str

    count_violations = []
    for car_id in range(car_count):
        for ev in ['A', 'E', 'X']:
            exp = expected_counts[car_id][ev]
            act = actual_counts[car_id][ev]
            if exp != act:
                count_violations.append(f"Count Error - Car {car_id}: Expected {exp} '{ev}' events, but logged {act}.")

    return logic_violations, count_violations


# ==========================================
# Example Usage
# ==========================================
if __name__ == "__main__":

    # Example Configs:
    # PrintConfig(every_event=True)                   -> Prints every single line
    # PrintConfig(specific_event_indices=[1, 5, 10])  -> Prints exactly the 1st, 5th, and 10th logged events
    # PrintConfig(time_range=(200000, 300000))        -> Prints only events in this time window
    # PrintConfig(on_violation=True)                  -> Prints Before & After states of crashes/blocks

    config = PrintConfig(on_violation=True)

    logic_errors, count_errors = evaluate_student_output("testcase1.txt", "output1.txt", p_config=config)

    print("\n--- Logic & Synchronization Violations ---")
    if not logic_errors:
        print("None! Intersection rules perfectly respected.")
    for err in logic_errors:
        print(err)

    print("\n--- Event Count Violations (Deadlocks/Crashes) ---")
    if not count_errors:
        print("None! All threads completed their exact routes.")
    for err in count_errors:
        print(err)
        