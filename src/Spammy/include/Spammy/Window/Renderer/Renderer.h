#pragma once
#include "Spammy/Headers.h"

class WindowRenderer {
public:
    virtual ~WindowRenderer() {};
    virtual bool create(HWND hwnd) = 0;
    virtual void cleanup() = 0;
    virtual void reset() = 0;
    virtual void setSize(size_t width, size_t height) = 0;
    virtual void render() = 0;
    virtual void beginFrame() = 0;
    virtual bool isCreated() { return false; }
};
