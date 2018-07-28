#include <GL/glut.h>
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C"
{
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}
#include <iostream>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>

using namespace std;
int inputWidth;
int inputHeight;
int width = 0;
int height = 0;
vector<unsigned char> screenCapture;
vector<unsigned char> webcamCapture;
bool done = false;
std::mutex mtx;

int OutputFrameRate = 30;

const int Border = 5;

const auto WebcamWidth = 320;
const auto WebcamHeight = 240;


void display()
{
  glClear(GL_COLOR_BUFFER_BIT);
  {
    std::lock_guard<std::mutex> guard(mtx);
    glTexImage2D(
      GL_TEXTURE_2D, 0, 3, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, screenCapture.data());
  }
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(Border, Border);
  
  glTexCoord2f(1, 0);
  glVertex2f(width + Border, Border);

  glTexCoord2f(1, 1);
  glVertex2f(width + Border, height + Border);

  glTexCoord2f(0, 1);
  glVertex2f(Border, height + Border);
  glEnd();
  glutSwapBuffers();
}

void timer(int = 0)
{
  glutTimerFunc(500, timer, 0);
  glutPostRedisplay();
}

void reshape(int w, int h)
{
  glViewport(0, 0, w, h);  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, h, 0, -1.0, 1.0);
}

void rgb2yuv(std::vector<unsigned char>::iterator frame,
          std::vector<unsigned char>::iterator Y,
          std::vector<unsigned char>::iterator U,
          std::vector<unsigned char>::iterator V, int n)
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

static int clamp(int x)
{
  return std::min(255, std::max(x, 0));
}

void webcamGrabber()
{
  auto webcamFormatContext = [&]() {
    AVFormatContext *formatContext = nullptr;
    auto fileName = "/dev/video0";
    auto format = "v4l2";
    auto inputFormat = av_find_input_format(format);
    if (!inputFormat)
    {
      std::cerr << "Unknown input format: " << format << std::endl;
      throw -0x10;
    }

    AVDictionary *format_opts = nullptr;
    av_dict_set(&format_opts, "framerate", "1000", 0);
    std::ostringstream resolution;
    resolution << WebcamWidth << "x" << WebcamHeight;
    av_dict_set(&format_opts, "video_size", resolution.str().c_str(), 0);
    auto err = avformat_open_input(&formatContext, fileName, inputFormat, &format_opts);
    if (err != 0)
    {
      std::cout << "Could not open input " << fileName << std::endl;
      throw -0x11;
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
    av_free_packet(&webcamPacket);
  }

  avformat_free_context(webcamFormatContext);
}

void grabber()
{
  auto formatContext = [&]() {
    AVFormatContext *formatContext = nullptr;
    auto fileName = ":0.0+65,126";
    if (inputHeight == 1080)
      fileName = ":0.0+0,74";
    auto format = "x11grab";
    auto inputFormat = av_find_input_format(format);
    if (!inputFormat)
    {
      cerr << "Unknown input format: '" << format << "'" << endl;
      exit(1);
    }

    AVDictionary *format_opts = NULL;
    av_dict_set(&format_opts, "framerate", "1000", 0);
    string resolution = to_string(inputWidth) + "x" + to_string(inputHeight);
    av_dict_set(&format_opts, "video_size", resolution.c_str(), 0);
    int len = avformat_open_input(&formatContext, fileName, inputFormat, &format_opts);
    if (len != 0)
    {
      cerr << "Could not open input " << fileName << endl;
      throw -0x10;
    }
    if (avformat_find_stream_info(formatContext, NULL) < 0)
    {
      cerr << "Could not read stream information from " << fileName << endl;
      throw -0x11;
    }
    av_dump_format(formatContext, 0, fileName, 0);
    av_dict_free(&format_opts);
    return formatContext;
  }();

  vector<unsigned char> yuv;
  width = formatContext->streams[0]->codec->width;
  height = formatContext->streams[0]->codec->height;
  screenCapture.resize(width * height * 4);
  fill(begin(screenCapture), end(screenCapture), 0);
  cout << "YUV4MPEG2 W" << width << " H" << height << " F" << OutputFrameRate
       << ":1 Ip A0:0 C420jpeg XYSCSS=420JPEG\n";
  yuv.resize(width * height * 3 / 2);
    
  AVPacket packet;
  int64_t currentTs = -1;
  int reminer = 0;
  int totalFrames = 0;
  int64_t initialPts = -1;
  while (av_read_frame(formatContext, &packet) == 0 && !done)
  {
    auto timeBase = formatContext->streams[0]->time_base;
    if (initialPts == -1)
      initialPts = packet.pts;
    if (++totalFrames % 100 == 0)
    {
      std::clog << totalFrames * timeBase.den / (packet.pts - initialPts) / timeBase.num << "FPS"
                << std::endl;
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
    {
      std::lock_guard<std::mutex> guard(mtx);
      memcpy(screenCapture.data(), packet.data, width * height * 4);
      {
        const auto X = width - WebcamWidth - 10;
        const auto Y = height - WebcamHeight - 10;
        for (auto j = 0; j < WebcamHeight; ++j)
          memcpy(&screenCapture[(X + (j + Y) * width) * 4],
                 &webcamCapture[j * WebcamWidth * 4],
                 WebcamWidth * 4);
      }
    }
    if (packet.pts > currentTs)
    {
      rgb2yuv(begin(screenCapture) + 0 * 4 * width * height / 4,
              begin(yuv) + 0 * width * height / 4,
              begin(yuv) + 0 * width * height / 4 / 4 + width * height,
              begin(yuv) + 0 * width * height / 4 / 4 + 5 * width * height / 4,
              height / 2);

      while (packet.pts > currentTs)
      {
        cout << "FRAME\n";
        cout.write((const char *)&yuv[0], yuv.size());
        currentTs += timeBase.den / timeBase.num / OutputFrameRate;
        reminer += timeBase.den % (timeBase.num * OutputFrameRate);
        if (reminer >= timeBase.num * OutputFrameRate)
        {
          reminer -= timeBase.num * OutputFrameRate;
          ++currentTs;
        }
      }
    }
    av_free_packet(&packet);
  }
  avformat_free_context(formatContext);
}

thread *t;
thread *webcamThread;

void bye()
{
  clog << "bye" << endl;
  done = true;
  t->join();
  webcamThread->join();
}

int main(int argc, char **argv)
{
  webcamCapture.resize(WebcamWidth * WebcamHeight * 4);
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
  if (argc == 2 && argv[1] == string("480"))
  {
    inputWidth = 854;
    inputHeight = 480;
  }
  else if (argc == 1 || (argc == 2 && argv[1] == string("720")))
  {
    inputWidth = 1280;
    inputHeight = 720;
  }
  else if (argc == 2 && argv[1] == string("1080"))
  {
    inputWidth = 1920;
    inputHeight = 1080;
  }
  glutInitWindowSize(inputWidth + Border * 2, inputHeight + Border * 2);
  glutInitWindowPosition(1280 + 65, 0);
  glutCreateWindow("Screen Capture");
  glClearColor(0, 0.5, 0, 1.0);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho (0, inputWidth + Border * 2, inputHeight + Border * 2, 0, -1, 1);
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glEnable(GL_TEXTURE_2D);
  timer();
  avcodec_register_all();
  avdevice_register_all();
#if CONFIG_AVFILTER
  avfilter_register_all();
#endif
  av_register_all();
  t = new thread(grabber);
  webcamThread = new thread(webcamGrabber);
  atexit(bye);
  glutMainLoop();
}
