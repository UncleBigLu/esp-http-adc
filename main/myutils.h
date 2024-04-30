//
// Created by unclebiglu on 4/22/24.
//

#ifndef MYUTILS_H
#define MYUTILS_H

#include "stdint.h"
#include "stdbool.h"

#define WINDOW_SIZE 128


struct SlidingWindow{
    uint32_t curPtr;
    uint32_t window[WINDOW_SIZE];
    double avg;
    uint32_t curSize;
};

struct SlidingWindow initWindow();

bool isFull(const struct SlidingWindow);

void push(struct SlidingWindow* s, const uint32_t val);



#endif //MYUTILS_H
