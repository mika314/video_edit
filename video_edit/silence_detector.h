#pragma once
#include <vector>
#include <set>
#include <cstdint>

class Cmp
{
public:
    bool operator()(const std::pair<int, int> &x, const std::pair<int, int> &y) { return x.first < y.first && x.second < y.first; }
};

std::set<std::pair<int, int>, Cmp> silenceDetector(const std::vector<int16_t> &audio);
