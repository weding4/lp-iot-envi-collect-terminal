#ifndef __FILTER_H
#define __FILTER_H

#include <stdint.h>

#define FILTER_WIN_SIZE  4   // ´°¿Ú´óÐ¡

typedef struct {
    uint16_t buf[FILTER_WIN_SIZE];
    uint8_t  index;
    uint32_t sum;
    uint8_t  count;
} SlidingFilter;

void SlidingFilter_Init(SlidingFilter *f);
void SlidingFilter_Update(SlidingFilter *f, uint16_t new_val);
uint16_t SlidingFilter_GetAvg(SlidingFilter *f);

#endif
