#include "myutils.h"

struct slidingWindow
{
    uint32_t curPtr;
    uint32_t* window;
    double avg;
    size_t curSize;
    size_t windowSize;
};

bool isFull(const slidingWindowHandler handler)
{
    return handler->curSize == handler->windowSize;
}

slidingWindowHandler initWindow(const size_t windowSize)
{
    slidingWindowHandler handler = malloc(sizeof(struct slidingWindow));
    handler->window = malloc(windowSize * sizeof(uint32_t));
    handler->avg = 0;
    handler->curPtr = 0;
    handler->curSize = 0;
    handler->windowSize = windowSize;
    return handler;
}

void push(const slidingWindowHandler handler, const uint32_t val)
{
    if (isFull(handler))
    {
        handler->avg -= (double)handler->window[handler->curPtr] / handler->windowSize;
    }
    else
    {
        handler->curSize++;
    }
    handler->avg += (double)val / handler->windowSize;
    handler->window[handler->curPtr] = val;
    handler->curPtr = (handler->curPtr + 1) % handler->windowSize;
}

double getAvg(const slidingWindowHandler handler)
{
    if (!isFull(handler))
    {
        return handler->avg * handler->windowSize / handler->curSize;
    }
    return handler->avg;
}
