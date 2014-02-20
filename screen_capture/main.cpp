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
vector<unsigned char> bgra;
bool done = false;

int OutputFrameRate = 24;

const int Border = 5;

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, &bgra[0]);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(Border, Border);
  
    glTexCoord2f(1, 0);
    glVertex2f(width / 3 + Border, Border);

    glTexCoord2f(1, 1);
    glVertex2f(width / 3 + Border, height / 3 + Border);

    glTexCoord2f(0, 1);
    glVertex2f(Border, height / 3 + Border);
    glEnd();
    glutSwapBuffers();
}

void timer(int = 0)
{
    glutTimerFunc(0, timer, 0);
    glutPostRedisplay();
}

void reshape(int w, int h)
{
    glViewport(0, 0, w, h);  
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1.0, 1.0);
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

    vector<int> sum;
    vector<unsigned char> yuv;
    bgra.resize(formatContext->streams[0]->codec->width * formatContext->streams[0]->codec->height * 4);
    sum.resize(bgra.size());
    fill(begin(sum), end(sum), 0);
    width = formatContext->streams[0]->codec->width;
    height = formatContext->streams[0]->codec->height;
    cout << "YUV4MPEG2 W" << width << " H" << height << " F" << OutputFrameRate << ":1 Ip A0:0 C420jpeg XYSCSS=420JPEG\n";
    yuv.resize(width * height * 3 / 2);
    
    AVPacket packet;
    int64_t currentTs = -1;
    int reminer = 0;
    int c = 0;
    while (av_read_frame(formatContext, &packet) == 0 && !done)
    {
        auto timeBase = formatContext->streams[0]->time_base;
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
        auto y = packet.data;
        for (auto x = begin(sum); x != end(sum); ++x, ++y)
            *x += *y;
        ++c;
        if (packet.pts > currentTs)
        {
            transform(begin(sum), end(sum), begin(bgra), [c](int x){ return x / c; });
            fill(begin(sum), end(sum), 0);
            c = 0;
            auto tmp = begin(bgra);
            for (auto i = begin(yuv); i < begin(yuv) + width * height; ++i)
            {
                auto b = *(tmp++);
                auto g = *(tmp++);
                auto r = *(tmp++);
                tmp++;
                *i = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            }
            auto tmpRgb = begin(bgra);
            auto tmpYuv = begin(yuv) + width * height;
            for (auto y = 0; y < height / 2; ++y)
            {
                for (auto x = 0; x < width / 2; ++x)
                {
                    auto b = *(tmpRgb++);
                    auto g = *(tmpRgb++);
                    auto r = *(tmpRgb++);
                    tmpRgb += 1;
                    b = (b + *(tmpRgb++)) / 2;
                    g = (g + *(tmpRgb++)) / 2;
                    r = (r + *(tmpRgb++)) / 2;
                    tmpRgb += 1;
                    *tmpYuv = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                    ++tmpYuv;
                }
                tmpRgb += 4 * width;
            }
            tmpRgb = begin(bgra);
            tmpYuv = begin(yuv) + 5 * width * height / 4;
            for (auto y = 0; y < height / 2; ++y)
            {
                for (auto x = 0; x < width / 2; ++x)
                {
                    auto b = *(tmpRgb++);
                    auto g = *(tmpRgb++);
                    auto r = *(tmpRgb++);
                    tmpRgb += 1;
                    b = (b + *(tmpRgb++)) / 2;
                    g = (g + *(tmpRgb++)) / 2;
                    r = (r + *(tmpRgb++)) / 2;
                    tmpRgb += 1;
                    *tmpYuv = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                    ++tmpYuv;
                }
                tmpRgb += 4 * width;
            }

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
    glutInitWindowSize(inputWidth / 3 + Border * 2, inputHeight / 3 + Border * 2);
    glutInitWindowPosition(1280 + 65, 0);
    glutCreateWindow("Screen Capture");
    glClearColor(0, 0.5, 0, 1.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho (0, inputWidth / 3 + Border * 2, inputHeight / 3 + Border * 2, 0, -1, 1);
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
