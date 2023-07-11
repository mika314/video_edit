#include "silence_detector.h"
#include <iostream>
#include <pocketsphinx.h>
#include <sstream>
using namespace std;

std::set<Range, Cmp> silenceDetector(const std::vector<int16_t> &wav)
{
  std::set<Range, Cmp> result;

  const auto config = []() {
    auto ret = ps_config_init(nullptr);
    ps_default_search_args(ret);
    ps_config_set_str(ret, "lm", nullptr);
    ps_config_set_str(
      ret,
      "allphone",
      "/home/mika/prj/video_edit/silence_detector/pocketsphinx-model/en-us/en-us-phone.lm.bin");
    ps_config_set_str(
      ret, "hmm", "/home/mika/prj/video_edit/silence_detector/pocketsphinx-model/en-us/en-us");
    ps_config_set_bool(ret, "backtrace", TRUE);
    ps_config_set_float(ret, "beam", 1e-20);
    ps_config_set_float(ret, "lw", 2.0);

    return ret;
  }();
  const auto decoder = [&config]() {
    auto ret = ps_init(config);
    if (!ret)
      throw std::runtime_error("PocketSphinx decoder init failed");
    return ret;
  }();
  const auto ep = []() {
    auto ret = ps_endpointer_init(0.f, 0.45f, PS_VAD_LOOSE, 0, 0);
    if (!ret)
      throw std::runtime_error("PocketSphinx endpointer init failed");
    return ret;
  }();

  enum State { Voice, Silence } state = Silence;
  int start = 0;

  const auto fs = static_cast<int>(ps_endpointer_frame_size(ep));
  std::vector<int16_t> buf;
  for (size_t p = 0; p < wav.size() - fs; p += fs)
  {
    for (auto i = begin(wav) + p; i < begin(wav) + p + fs; ++i)
      buf.push_back(*i);
    const auto prevInSpeech = ps_endpointer_in_speech(ep);
    auto speech = ps_endpointer_process(ep, buf.data());
    buf.clear();
    if (!speech)
    {
      if (state == Voice)
        start = p;
      state = Silence;
      continue;
    }
    if (!prevInSpeech)
      ps_start_utt(decoder);
    const auto ret = ps_process_raw(decoder, speech, fs, FALSE, FALSE);
    if (ret < 0)
      throw std::runtime_error("ps_process_raw() failed");
    const auto hyp = ps_get_hyp(decoder, nullptr);
    if (hyp)
    {
      std::istringstream st(hyp);
      std::string tmp;
      std::string phoneme = "SIL";
      while (std::getline(st, tmp, ' '))
      {
        if (tmp[0] != '+')
          phoneme = tmp;
      }
      if (phoneme == "SIL")
      {
        if (state == Voice)
          start = p;
        state = Silence;
      }
      else
      {
        const auto padding = 8 * fs;
        if (state == Silence)
          if (static_cast<int>(p) - padding > start)
          {
            auto dur = (p - padding - start) / 44100.f;
            if (dur > 0.2f)
            {
              const auto speedup = std::min(15, std::max(static_cast<int>(dur * 15), 2));
              cout << p * 100 / wav.size() << "% " << start << " - " << p - padding << " " << dur << " x"
                   << speedup << endl;
              result.insert(Range(start, p - padding, speedup));
            }
          }
        state = Voice;
      }
    }
    else
    {
      if (state == Voice)
        start = p;
      state = Silence;
    }
    if (!ps_endpointer_in_speech(ep))
    {
      ps_end_utt(decoder);
      ps_get_hyp(decoder, nullptr);
    }
  }

  ps_endpointer_free(ep);
  ps_free(decoder);
  ps_config_free(config);
  return result;
}
