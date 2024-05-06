#ifndef MYUTILS_H
#define MYUTILS_H

#include "stdint.h"
#include "stdbool.h"
#include <stdio.h>


struct slidingWindow;

typedef struct slidingWindow* slidingWindowHandler;

slidingWindowHandler initWindow(size_t windowSize);

bool isFull(slidingWindowHandler);

void push(slidingWindowHandler, uint32_t val);

void freeWindow(slidingWindowHandler);

double getAvg(slidingWindowHandler);

#endif //MYUTILS_H
