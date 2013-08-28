#include "silence_detector.h"
#include <deque>
#include <cmath>
using namespace std;

std::set<std::pair<int, int>, Cmp> silenceDetector(const std::vector<int16_t> &audio)
{
    enum { BuffSize = 44100 / 8 };

    std::set<std::pair<int, int>, Cmp> res;
    deque<short> d;
    int state = 0;
    float averageVelosity = 0;
    long long fadeIn = 0;
    int s = 0;
    int start = -1;
    int end;
    short oldV = 0;
    short v = 0;
    short noizeLevel = 0x8000 * 45 / 1000;
    int cutOff = 0;
    for (auto v1: audio)
    {
        d.push_back(v1);

        if (cutOff < 0)
        {
            if (v1 > noizeLevel)
                noizeLevel = v1;
            ++cutOff;
        }

        averageVelosity = abs(d.back());
        if (averageVelosity > noizeLevel * 115 / 100)
        {
            state = -1;
            if (fadeIn == -2)
            {
                fadeIn = 44100LL / 32;
                oldV = v;
            }
            if (start != -1)
            {
                res.insert(std::make_pair(start, end));
                start = -1;
            }
            v = d.front();
            if (fadeIn > 0)
            {
                v = (v * (44100LL / 32 - fadeIn) + oldV * fadeIn) / (44100LL / 32);
                fadeIn--;
            }
            d.pop_front();
        }
        else
        {
            if (state == -1)
                state = BuffSize + BuffSize;
            if (state > 0)
            {
                if (start != -1)
                {
                    res.insert(std::make_pair(start, end));
                    start = -1;
                }
                v = d.front();
                if (fadeIn > 0)
                {
                    v = (v * (44100LL / 32 - fadeIn) + oldV * fadeIn) / (44100LL / 32);
                    fadeIn--;
                }
                d.pop_front();
                --state;
            }
            else
            {
                if (d.size() > BuffSize)
                {
                    d.pop_front();
                    if (start == -1)
                        start = s - BuffSize;
                    else
                    {
                        end = s - BuffSize;
                        fadeIn = -2;
                    }
                }
            }
        }
        ++s;
    }
    return res;
}
