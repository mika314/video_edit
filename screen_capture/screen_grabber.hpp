#pragma once
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C"
{
#include <libavformat/avformat.h>
}
#include <vector>
#include <memory>
#include <thread>

class ScreenGrabber
{
public:
  ScreenGrabber(int inputWidth, int inputHeight);
  bool tick();
  ~ScreenGrabber();
  AVPacket packet;
  bool done = false;
  int width = 0;
  int height = 0;

private:
  AVFormatContext *formatContext = nullptr;
  std::vector<unsigned char> yuv;

  int64_t currentTs = -1;
  int reminer = 0;
  int totalFrames = 0;
  int64_t initialPts = -1;
  std::vector<unsigned char> webcamCapture;
  std::unique_ptr<std::thread> webcamThread;
  void rgb2yuv(unsigned char *frame,
               std::vector<unsigned char>::iterator Y,
               std::vector<unsigned char>::iterator U,
               std::vector<unsigned char>::iterator V,
               int n);
};
