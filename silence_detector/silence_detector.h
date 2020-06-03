#pragma once
#include <vector>
#include <set>
#include <cstdint>

struct Range
{
    Range(int aStart, int aEnd, int aSpeedUp = 6):
        start(aStart),
        end(aEnd),
        speedUp(aSpeedUp)
    {}
    int start;
    int end;
    int speedUp;
};

class Cmp
{
public:
    bool operator()(const Range &x, const Range &y) const { return x.start < y.start && x.end < y.start; }
};

std::set<Range, Cmp> silenceDetector(const std::vector<int16_t> &audio);
