#include "silence_detector.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <fftw3.h>
#include <iostream>
using namespace std;

std::set<Range, Cmp> silenceDetector(const std::vector<int16_t> &audio)
{
  std::set<Range, Cmp> result;
  const int SpecSize = 2048;
  const auto CutFreq = 200.0f;

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
    double aveSum = 0;
    double sqSum = 0;
    int c = 0;
    for (auto f = fftOut + static_cast<int>(CutFreq * SpecSize / 44100.0f); f < fftOut + SpecSize / 2; ++f)
    {
      double tmp = *f[0] * *f[0] + *f[1] * *f[1];
      double m = sqrt(tmp);
      ++c;
      if (c < SpecSize / 4 && c > 7)
      {
        sqSum += tmp;
        aveSum += m;
      }
    }

    const auto sq = sqSum / aveSum;
    const auto ave = aveSum / (SpecSize / 4 - 7);
    const auto aveMinThreshold = 0.00002 * (32000 * SpecSize);
    const auto aveMaxThreshold = 0.00005 * (32000 * SpecSize);

    if ((sq < 20000 && ave < aveMaxThreshold) || ave < aveMinThreshold)
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
      cout << "voice " << ave << " " << aveMinThreshold << endl;
      if (state == Silence)
        if (static_cast<int>(p) - SpecSize > start)
        {
          cout << start << " " << p - SpecSize << endl;
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
