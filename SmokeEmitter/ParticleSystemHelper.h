#pragma once
#include <algorithm>

inline int Saturate(int value, int minValue, int maxValue) {
    return (std::min)((std::max)(value, minValue), maxValue);
}

inline float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}
