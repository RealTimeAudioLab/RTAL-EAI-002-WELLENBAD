#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALMemorySnapshot
{
    uint32_t internalFree;
    uint32_t internalMinimumFree;
    uint32_t internalLargestBlock;
    uint32_t psramTotal;
    uint32_t psramFree;
    uint32_t psramLargestBlock;
};

class RTALSystemInfo
{
public:
    static RTALStatus selfTest();
    static RTALMemorySnapshot memory();
    static void printBootReport();
    static void printRuntimeReport();
};
