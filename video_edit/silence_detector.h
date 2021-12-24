#pragma once
#include <cstdint>
#include <set>
#include <vector>

struct Range
{
  Range(int aStart, int aEnd, int aSpeedUp = 600) : start(aStart), end(aEnd), speedUp(aSpeedUp) {}
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
