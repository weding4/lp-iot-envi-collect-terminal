#include "filter.h"

void SlidingFilter_Init(SlidingFilter *f)
{
    f->index = 0;
    f->sum = 0;
    f->count = 0;
    for(int i = 0; i < FILTER_WIN_SIZE; i++)
        f->buf[i] = 0;
}

void SlidingFilter_Update(SlidingFilter *f, uint16_t new_val)
{
    f->sum -= f->buf[f->index];           // 减去最旧值
    f->buf[f->index] = new_val;           // 存入新值
    f->sum += new_val;                    // 加上新值
    f->index = (f->index + 1) % FILTER_WIN_SIZE;

    if(f->count < FILTER_WIN_SIZE)
        f->count++;                       // 有效数据个数（用于初始未满时）
}

uint16_t SlidingFilter_GetAvg(SlidingFilter *f)
{
    if(f->count == 0) return 0;
    return (uint16_t)(f->sum / f->count);
}
