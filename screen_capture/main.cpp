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
#include <sys/time.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>

using namespace std;
int inputWidth;
int inputHeight;
int width = 0;
int height = 0;
vector<unsigned char> bgr;
bool done = false;

int OutputFrameRate = 30;

const int Border = 5;

void display()
{
  glClear(GL_COLOR_BUFFER_BIT);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, &bgr[0]);
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
  glutTimerFunc(100, timer, 0);
  glutPostRedisplay();
}

void reshape(int w, int h)
{
  glViewport(0, 0, w, h);  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, h, 0, -1.0, 1.0);
}

void rgb2yuv(std::vector<short>::iterator sum,
          std::vector<unsigned char>::iterator rgb,
          std::vector<unsigned char>::iterator Y,
          std::vector<unsigned char>::iterator U,
          std::vector<unsigned char>::iterator V, int n, int c)
{
  for (auto y = 0; y < n; ++y)
  {
    for (auto x = 0; x < width / 2; ++x)
    {
      auto &b0 = *(rgb++);
      b0 = *sum / c;
      *sum = 0;
      ++sum;
      auto &g0 = *(rgb++);
      g0 = *sum / c;
      *sum = 0;
      ++sum;
      auto &r0 = *(rgb++);
      r0 = *sum / c;
      *sum = 0;
      ++sum;
      *Y++ = ((66 * r0 + 129 * g0 + 25 * b0 + 128) >> 8) + 16;
      auto &b1 = *(rgb++);
      b1 = *sum / c;
      *sum = 0;
      ++sum;
      auto &g1 = *(rgb++);
      g1 = *sum / c;
      *sum = 0;
      ++sum;
      auto &r1 = *(rgb++);
      r1 = *sum / c;
      *sum = 0;
      ++sum;
      *Y++ = ((66 * r1 + 129 * g1 + 25 * b1 + 128) >> 8) + 16;
      auto b = (b0 + b1) / 2;
      auto g = (g0 + g1) / 2;
      auto r = (r0 + r1) / 2;
      *U++ = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
      *V++ = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
    }
    for (auto x = 0; x < width; ++x)
    {
      *rgb = *sum / c;
      *sum = 0;
      ++sum;
      auto b0 = *(rgb++);
      *rgb = *sum / c;
      *sum = 0;
      ++sum;
      auto g0 = *(rgb++);
      *rgb = *sum / c;
      *sum = 0;
      ++sum;
      auto r0 = *(rgb++);
      *Y++ = ((66 * r0 + 129 * g0 + 25 * b0 + 128) >> 8) + 16;
    }
  }
}

void ave(std::vector<short>::iterator sum, unsigned char *rgb, int n)
{
  for (int i = 0; i < n; ++i)
  {
    *sum++ += *rgb++;
    *sum++ += *rgb++;
    *sum++ += *rgb++;
    ++rgb;
  }
}

void grabber()
{
  avcodec_register_all();
  avdevice_register_all();
#if CONFIG_AVFILTER
  avfilter_register_all();
#endif
  av_register_all();
  AVFormatContext *formatContext;

  formatContext = NULL;
  auto fileName = ":0.0+65,126";
  if (inputHeight == 1080)
    fileName = ":0.0+0,0";
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

  if (len != 0) {
    cerr << "Could not open input " << fileName << endl;;
    throw -0x10;
  }

  if (avformat_find_stream_info(formatContext, NULL) < 0) {
    cerr << "Could not read stream information from " <<  fileName << endl;
    throw -0x11;
  }
  av_dump_format(formatContext, 0, fileName, 0);

  vector<short> sum;
  vector<unsigned char> yuv;
  width = formatContext->streams[0]->codec->width;
  height = formatContext->streams[0]->codec->height;
  bgr.resize(width * height * 3);
  sum.resize(width * height * 3);
  fill(begin(sum), end(sum), 0);
  cout << "YUV4MPEG2 W" << width << " H" << height << " F" << OutputFrameRate << ":1 Ip A0:0 C420jpeg XYSCSS=420JPEG\n";
  yuv.resize(width * height * 3 / 2);
    
  AVPacket packet;
  int64_t currentTs = -1;
  int reminer = 0;
  int c = 0;
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
    std::thread t1(&ave, begin(sum), packet.data, width * height / 4);
    std::thread t2(&ave, begin(sum) + 3 * width * height / 4, packet.data + 4 * width * height / 4, width * height / 4);
    std::thread t3(&ave, begin(sum) + 2 * 3 * width * height / 4, packet.data + 2 * 4 * width * height / 4, width * height / 4);
    ave(begin(sum) + 3 * 3 * width * height / 4, packet.data + 3 * 4 * width * height / 4, width * height / 4);
    t1.join();
    t2.join();
    t3.join();
    ++c;
    if (packet.pts > currentTs)
    {
      std::thread t1(&rgb2yuv, begin(sum), begin(bgr), begin(yuv), begin(yuv) + width * height, begin(yuv) + 5 * width * height / 4, height / 2 / 4, c);
      std::thread t2(&rgb2yuv, begin(sum) + 3 * width * height / 4, begin(bgr) + 3 * width * height / 4, begin(yuv) + width * height / 4, begin(yuv) + width * height + width * height / 4 / 4, begin(yuv) + 5 * width * height / 4 + width * height / 4 / 4, height / 2 / 4, c);
      std::thread t3(&rgb2yuv, begin(sum) + 2 * 3 * width * height / 4, begin(bgr) + 2 * 3 * width * height / 4, begin(yuv) + 2 * width * height / 4, begin(yuv) + width * height + 2 * width * height / 4 / 4, begin(yuv) + 5 * width * height / 4 + 2 * width * height / 4 / 4, height / 2 / 4, c);
      rgb2yuv(begin(sum) + 3 * 3 * width * height / 4, begin(bgr) + 3 * 3 * width * height / 4, begin(yuv) + 3 * width * height / 4, begin(yuv) + width * height + 3 * width * height / 4 / 4, begin(yuv) + 5 * width * height / 4 + 3 * width * height / 4 / 4, height / 2 / 4, c);
      t1.join();
      t2.join();
      t3.join();
      c = 0;

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
  }
  av_dict_free(&format_opts);
  avformat_free_context(formatContext);
}

thread *t;

void bye()
{
  clog << "bye" << endl;
  done = true;
  t->join();
}

int main(int argc, char **argv)
{
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
  t = new thread(grabber);
  atexit(bye);
  glutMainLoop();
}
