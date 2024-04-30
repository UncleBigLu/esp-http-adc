//
// Created by unclebiglu on 4/22/24.
//
#include "myutils.h"

bool isFull(const struct SlidingWindow s){
    return s.curSize == WINDOW_SIZE;
}

struct SlidingWindow initWindow()
{
    struct SlidingWindow s = {
        .curPtr = 0,
        .avg = 0,
        .curSize = 0
    };
    return s;
}

void push(struct SlidingWindow* s, const uint32_t val)
{
    if(isFull(*s))
    {
        s->avg -= (double)s->window[s->curPtr] / WINDOW_SIZE;
    }else
    {
        s->curSize++;
    }
    s->avg += (double)val / WINDOW_SIZE;
    s->window[s->curPtr] = val;
    s->curPtr = (s->curPtr+1) % WINDOW_SIZE;
}
