#pragma once
#include "RTALStatus.h"

class RTALBootManager
{
public:
    static RTALStatus begin();

private:
    static void printResult(const char* component, RTALStatus status);
};
