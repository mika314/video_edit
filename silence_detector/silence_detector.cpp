#include "silence_detector.h"
#include <fftw3.h>
#include <deque>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>
using namespace std;

std::set<Range, Cmp> silenceDetector(const std::vector<int16_t> &audio)
{
    std::set<Range, Cmp> result;
    const int SpecSize = 2048;

    fftw_complex *fftIn;
    fftw_complex *fftOut;
    fftw_plan plan;
    fftIn = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * SpecSize);
    fftOut = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * SpecSize);
    memset(fftIn, 0, sizeof(fftw_complex) * SpecSize);
    memset(fftOut, 0, sizeof(fftw_complex) * SpecSize);
    plan = fftw_plan_dft_1d(SpecSize, fftIn, fftOut, FFTW_FORWARD, FFTW_MEASURE);
    
    enum State { Voice, Silence } state = Silence;
    int silenceCount = 0;
    int start = 0;

    int percent = -1;

    for (size_t p = 0; p < audio.size() - SpecSize; p += SpecSize)
    {
        if (percent != static_cast<int>(p * 100 / audio.size()))
        {
            percent = p * 100 / audio.size();
            cout << percent << endl;
        }
        auto f = fftIn;
        for (auto i = begin(audio) + p; i < begin(audio) + p + SpecSize; ++i, ++f)
        {
            *f[0] = *i;
            *f[1] = 0;
        }
        fftw_execute(plan);
        double ave = 0;
        double sq = 0;
        int c = 0;
        for (auto f = fftOut; f < fftOut + SpecSize / 2; ++f)
        {
            double tmp = *f[0] * *f[0] + *f[1] * *f[1];
            double m = sqrt(tmp);
            ++c;
            if (c < SpecSize / 4 && c > 7)
            {
                sq += tmp;
                ave += m;
            }
        }
        if (sq / ave < 90000 || ave / (SpecSize / 4 - 7) < 0.001 * 0.1 * (32000 * SpecSize))
        {
            ++silenceCount;
            if (silenceCount > 6)
            {
                if (state == Voice)
                    start = p;
                state = Silence;
            }
        }
        else
        {
            if (state == Silence)
                if (static_cast<int>(p) - SpecSize > start)
                {
                    result.insert(Range(start, p - SpecSize));
                }
            state = Voice;
            silenceCount = 0;
        }
    }

    fftw_destroy_plan(plan);
    fftw_free(fftIn);
    fftw_free(fftOut);
    return result;
}
