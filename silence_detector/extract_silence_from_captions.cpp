#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

struct Range
{
  Range(int aStart, int aEnd, int aSpeedUp) : start(aStart), end(aEnd), speedUp(aSpeedUp) {}
  int start;
  int end;
  int speedUp;
};

class Cmp
{
public:
  bool operator()(const Range &x, const Range &y) const { return x.start < y.start && x.end < y.start; }
};

static int convertToMilliseconds(const std::string &time)
{
  int hours, minutes, seconds, milliseconds;
  sscanf(time.c_str(), "%d:%d:%d,%d", &hours, &minutes, &seconds, &milliseconds);
  return ((hours * 60 * 60 + minutes * 60 + seconds) * 1000 + milliseconds);
}

std::set<Range, Cmp> extractSilencesFromCaptions(std::string fileName)
{
  std::set<Range, Cmp> silences;
  std::ifstream file(fileName);
  std::string line;
  int64_t previousEnd = 0;

  while (std::getline(file, line))
  {
    std::istringstream iss(line);
    std::string start, end, text;
    std::getline(iss, start, '-');
    std::getline(iss, end, ' ');
    std::getline(iss, text);

    int64_t startMs = convertToMilliseconds(start) - 1000;
    int64_t endMs = convertToMilliseconds(end) + 500;

    if (startMs - previousEnd >= 200)
    {                                                           // if silence is longer than 0.2 seconds
      float silenceDuration = (startMs - previousEnd) / 1000.f; // convert to seconds
      int speedUp = static_cast<int>(std::min(std::max(silenceDuration * 10, 2.f), 6.f));
      silences.insert(Range(previousEnd * 44100 / 1000, startMs * 44100 / 1000, speedUp));
    }
    previousEnd = endMs;
  }

  return silences;
}
