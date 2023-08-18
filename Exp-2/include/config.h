#ifndef _CONFIG_H
#define _CONFIG_H

/************** parameters of tasks **************/
#define HH_THRESHOLD 100 //0.000005
#define HH_ESTIMATE_THRESHOLD (HH_THRESHOLD / MEASURE_POINT) // 0.000001
#define SS_THRESHOLD 10
#define SS_ESTIMATE_THRESHOLD 2

/************** parameters of data **************/
// Do not edit this part!
#define DATA_T_SIZE 36
#define CHARKEY_LEN 13

/************** parameters of sketches **************/
#define CM_DEPTH 4
#define CM_WIDTH (1 << 19)
#define CM_TYPE uint32_t

#define SM_DEPTH 4
#define SM_WIDTH (1 << 19)
#define SM_TYPE uint32_t

#define HP_DEPTH 4
#define HP_WIDTH (61681 * 2)
#define HP_TYPE uint32_t

#define MV_DEPTH 4
#define MV_WIDTH (49932 * 2) 
#define MV_TYPE int32_t

#define LC_TYPE uint32_t
#define LC_WIDTH (1 << 26)

#define HLL_TYPE uint8_t
#define HLL_WIDTH (1 << 24)

#define SS_DEPTH 4
#define SS_WIDTH /*((1 << 10) + (1 << 7))*64*/ (116508 / 4)
#define SS_B (417 / 4)
#define SS_BB (417 * 2 / 4)
#define SS_C 3
#define SS_TYPE uint32_t

#define VBF_TYPE uint8_t
#define VBF_WIDTH (3276)

/************** parameters of windows **************/
#define WINDOW_SIZE 0.5 // window size: 500 millisecond
#define MEASURE_POINT 5 // sub-window size: 1/5 window size
#define NO_OPERATION_TIME 25 // C&R time: 1/25 window size (20 ms)
#define WINDOW_NUM 1

#endif
