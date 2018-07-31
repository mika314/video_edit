#include "screen_grabber.hpp"
extern "C" {
#include <libavdevice/avdevice.h>
}

#include "const.hpp"
#include <iostream>
#include <sstream>

static int clamp(int x)
{
  return std::min(255, std::max(x, 0));
}

static void webcamGrabber(std::vector<unsigned char> &webcamCapture, bool &done)
{
  auto webcamFormatContext = [&]() {
    AVFormatContext *formatContext = nullptr;
    auto fileName = "/dev/video0";
    auto format = "v4l2";
    auto inputFormat = av_find_input_format(format);
    if (!inputFormat)
    {
      std::cerr << "Unknown input format: " << format << std::endl;
      throw - 0x10;
    }

    AVDictionary *format_opts = nullptr;
    av_dict_set(&format_opts, "framerate", std::to_string(OutputFrameRate).c_str(), 0);
    std::ostringstream resolution;
    resolution << WebcamWidth << "x" << WebcamHeight;
    av_dict_set(&format_opts, "video_size", resolution.str().c_str(), 0);
    auto err = avformat_open_input(&formatContext, fileName, inputFormat, &format_opts);
    if (err != 0)
    {
      std::cout << "Could not open input " << fileName << std::endl;
      throw - 0x11;
    }

    av_dump_format(formatContext, 0, fileName, 0);
    av_dict_free(&format_opts);
    return formatContext;
  }();

  while (!done)
  {
    AVPacket webcamPacket;
    auto err = av_read_frame(webcamFormatContext, &webcamPacket);
    if (err == 0)
    {
      for (auto j = 0; j < WebcamHeight; ++j)
        for (auto i = 0; i < WebcamWidth; ++i)
        {
          auto y = webcamPacket.data[j * WebcamWidth * 2 + i * 2];
          auto u = webcamPacket.data[j * WebcamWidth * 2 + (i & 0xfffe) * 2 + 1];
          auto v = webcamPacket.data[j * WebcamWidth * 2 + (i | 0x1) * 2 + 1];
          auto c = y - 16;
          auto d = u - 128;
          auto e = v - 128;
          auto r = clamp((298 * c + 409 * e + 128) >> 8);
          auto g = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
          auto b = clamp((298 * c + 516 * d + 128) >> 8);

          webcamCapture[(i + j * WebcamWidth) * 4 + 0] = b;
          webcamCapture[(i + j * WebcamWidth) * 4 + 1] = g;
          webcamCapture[(i + j * WebcamWidth) * 4 + 2] = r;
        }
    }
    av_packet_unref(&webcamPacket);
  }

  avformat_free_context(webcamFormatContext);
}

ScreenGrabber::ScreenGrabber(int inputWidth, int inputHeight)
{
  avcodec_register_all();
  avdevice_register_all();
#if CONFIG_AVFILTER
  avfilter_register_all();
#endif
  av_register_all();

  webcamCapture.resize(WebcamWidth * WebcamHeight * 4);
  auto fileName = ":0.0+65,126";
  if (inputHeight == 1080)
    fileName = ":0.0+0,74";
  auto format = "x11grab";
  auto inputFormat = av_find_input_format(format);
  if (!inputFormat)
  {
    std::cerr << "Unknown input format: '" << format << "'" << std::endl;
    exit(1);
  }

  AVDictionary *format_opts = NULL;
  av_dict_set(&format_opts, "framerate", std::to_string(OutputFrameRate).c_str(), 0);
  std::string resolution = std::to_string(inputWidth) + "x" + std::to_string(inputHeight);
  av_dict_set(&format_opts, "video_size", resolution.c_str(), 0);
  int len = avformat_open_input(&formatContext, fileName, inputFormat, &format_opts);
  if (len != 0)
  {
    std::cerr << "Could not open input " << fileName << std::endl;
    throw - 0x10;
  }
  if (avformat_find_stream_info(formatContext, NULL) < 0)
  {
    std::cerr << "Could not read stream information from " << fileName << std::endl;
    throw - 0x11;
  }
  av_dump_format(formatContext, 0, fileName, 0);
  av_dict_free(&format_opts);

  width = formatContext->streams[0]->codecpar->width;
  height = formatContext->streams[0]->codecpar->height;
  std::cout << "YUV4MPEG2 W" << width << " H" << height << " F" << OutputFrameRate
            << ":1 Ip A0:0 C420jpeg XYSCSS=420JPEG\n";
  yuv.resize(width * height * 3 / 2);
  memset(&packet, 0, sizeof(packet));
  webcamThread =
    std::make_unique<std::thread>(webcamGrabber, std::ref(webcamCapture), std::ref(done));
}

bool ScreenGrabber::tick()
{
  if (done)
    return false;
  av_packet_unref(&packet);
  auto err = av_read_frame(formatContext, &packet);
  if (err != 0)
    return false;
  auto timeBase = formatContext->streams[0]->time_base;
  if (initialPts == -1)
    initialPts = packet.pts;
  if (++totalFrames % 100 == 0)
  {
    std::clog << totalFrames * timeBase.den / (packet.pts - initialPts) / timeBase.num << "FPS\n";
    totalFrames = 0;
    initialPts = -1;
  }
  if (currentTs == -1)
  {
    currentTs = packet.pts + timeBase.den / timeBase.num / OutputFrameRate;
    reminer += timeBase.den % (timeBase.num * OutputFrameRate);
    if (reminer >= timeBase.num * OutputFrameRate)
    {
      reminer -= timeBase.num * OutputFrameRate;
      ++currentTs;
    }
  }
  auto screenCapture = packet.data;
  const auto X = width - WebcamWidth - 10;
  const auto Y = height - WebcamHeight - 10;
  for (auto j = 0; j < WebcamHeight; ++j)
    memcpy(&screenCapture[(X + (j + Y) * width) * 4],
           &webcamCapture[j * WebcamWidth * 4],
           WebcamWidth * 4);
  rgb2yuv(screenCapture + 0 * 4 * width * height / 4,
          begin(yuv) + 0 * width * height / 4,
          begin(yuv) + 0 * width * height / 4 / 4 + width * height,
          begin(yuv) + 0 * width * height / 4 / 4 + 5 * width * height / 4,
          height / 2);

  while (packet.pts > currentTs)
  {
    std::cout << "FRAME\n";
    std::cout.write((const char *)&yuv[0], yuv.size());
    currentTs += timeBase.den / timeBase.num / OutputFrameRate;
    reminer += timeBase.den % (timeBase.num * OutputFrameRate);
    if (reminer >= timeBase.num * OutputFrameRate)
    {
      reminer -= timeBase.num * OutputFrameRate;
      ++currentTs;
    }
  }
  return true;
}

ScreenGrabber::~ScreenGrabber()
{
  webcamThread->join();
  av_packet_unref(&packet);
  avformat_free_context(formatContext);
}

void ScreenGrabber::rgb2yuv(unsigned char *frame,
                            std::vector<unsigned char>::iterator Y,
                            std::vector<unsigned char>::iterator U,
                            std::vector<unsigned char>::iterator V,
                            int n)
{
  for (auto y = 0; y < n; ++y)
  {
    for (auto x = 0; x < width / 2; ++x)
    {
      auto b0 = *(frame++);
      auto g0 = *(frame++);
      auto r0 = *(frame++);
      ++frame;
      *Y++ = ((66 * r0 + 129 * g0 + 25 * b0 + 128) >> 8) + 16;
      auto b1 = *(frame++);
      auto g1 = *(frame++);
      auto r1 = *(frame++);
      ++frame;
      *Y++ = ((66 * r1 + 129 * g1 + 25 * b1 + 128) >> 8) + 16;
      auto b = (b0 + b1) / 2;
      auto g = (g0 + g1) / 2;
      auto r = (r0 + r1) / 2;
      *U++ = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
      *V++ = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
    }
    for (auto x = 0; x < width; ++x)
    {
      auto b0 = *(frame++);
      auto g0 = *(frame++);
      auto r0 = *(frame++);
      ++frame;
      *Y++ = ((66 * r0 + 129 * g0 + 25 * b0 + 128) >> 8) + 16;
    }
  }
}
