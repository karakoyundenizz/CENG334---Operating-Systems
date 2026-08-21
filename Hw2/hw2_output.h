#ifndef HW2_OUTPUT_H_
#define HW2_OUTPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		DIR_HORIZONTAL = 0,
		DIR_VERTICAL = 1
	} Direction;

	typedef enum {
		ET_ARRIVE = 0,
		ET_ENTER = 1,
		ET_EXIT = 2
	} EventType;

	void hw2_init_output(void);

	void hw2_write_output(int car_id, EventType event, Direction dir, int lane);

#ifdef __cplusplus
}
#endif

#endif