#include "silence_detector.h"
#include <GL/glut.h>
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
extern "C"
{
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}
#include <pulse/simple.h>
#include <pulse/error.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <thread>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fftw3.h>

using namespace std;

vector<int16_t> audio;

set<Range, Cmp > rmList;
vector<vector<pair<int16_t, int16_t> > > minMax;
vector<pair<void *, size_t> > thumbs;
int thumbLinesize;
int thumbHeight;
int thumbWidth;
size_t width;
size_t height;
double x = 0;
float zoom = 1.0f;

const int SpecSize = 2048;

enum { Stop, Playing } state = Stop;

volatile int pos = 0;
volatile pa_usec_t latency;

string fileName;

int lastXX = -1;
int lastXX2 = -1;
int sampleRate;
vector<unsigned char> rgb;
bool follow = false;
fftw_complex *fftIn;
fftw_complex *fftOut;
fftw_plan plan;
const int Fps = 24;

void drawSpec()
{
    auto p = pos;
    auto l = latency;
    p = p - l * sampleRate / 1000000;
    if (p < 0)
        p = 0;
    if (p >= static_cast<int>(audio.size()) - SpecSize)
        p = audio.size() - SpecSize;
    auto f = fftIn;
    for (auto i = begin(audio) + p; i < begin(audio) + p + SpecSize; ++i, ++f)
    {
        *f[0] = *i;
        *f[1] = 0;
    }
    fftw_execute(plan);
    vector<double> s;
    double ave = 0;
    double sq = 0;
    for (auto f = fftOut; f < fftOut + SpecSize / 2; ++f)
    {
        double tmp = *f[0] * *f[0] + *f[1] * *f[1];
        double m = sqrt(tmp);
        s.push_back(m);
        if (s.size() < SpecSize / 4 && s.size() > 7)
        {
            sq += tmp;
            ave += m;
        }
    }
    const double Max = 0.1 * (32000 * SpecSize);
    const double dmax = 1;

    glLoadIdentity();
    glOrtho(0, width, -Max * thumbHeight / (height - thumbHeight), Max, -1, 1);
    glColor3f(0.0, 0.6, 0.0);
    glBegin(GL_LINES);
    for (size_t x = 0; x < width; ++x)
    {
        double m = s[x * SpecSize / width /  4];
        glVertex2f(x, m);
        glVertex2f(x, 0);
    }
    glEnd();
    glLoadIdentity();
    glOrtho(0, width, -dmax * thumbHeight / (height - thumbHeight), dmax, -1, 1);
    if (sq / ave / 2000000.0 < 0.03 || ave / (SpecSize / 4 - 7) < 0.001 * Max)
        glColor3f(0.5, 0.5, 0.0);
    else
        glColor3f(0.7, 0.0, 0.0);
    glBegin(GL_LINES);
    glVertex2f(0, sq / ave / 2000000.0);
    glVertex2f(width, sq / ave / 2000000.0);
    glEnd();
}

void display()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho (0, width, 0x8000, -0x8000, -1, 1);
    auto p = pos;
    auto l = latency;
    auto sx = (p - x - l * sampleRate / 1000000) / zoom;
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    for (auto r: rmList)
    {
        if (r.speedUp < 100)
            glColor3f(0.2, 0.0, 0.0);
        else
            glColor3f(0.9, 0.0, 0.0);
        int x1 = (r.start - x) / zoom;
        int x2 = (r.end - x) / zoom;
        glVertex2f(x1, -0x8000);
        glVertex2f(x2, -0x8000);
        glVertex2f(x2, 0x8000);
        glVertex2f(x1, 0x8000);
    }
    glEnd();
    if (lastXX != -1 && lastXX2 != -1)
    {
        if (lastXX2 < lastXX)
            glColor3f(0.4, 0.1, 0.1);
        else
            glColor3f(0.0, 0.3, 0.0);
        glBegin(GL_QUADS);
        glVertex2f(lastXX, -0x8000);
        glVertex2f(lastXX2, -0x8000);
        glVertex2f(lastXX2, 0x8000);
        glVertex2f(lastXX, 0x8000);
        glEnd();
    }
    glColor3f(0, 0, 0.5);
    glBegin(GL_LINE_STRIP);
    for (size_t sx = 0; sx < width; ++sx)
    {
        int x1 = sx * zoom + x;
        int x2 = (sx + 1) * zoom + x;
        if (x1 < 0)
            x1 = 0;
        if (x1 >= static_cast<int>(audio.size()))
            x1 = audio.size() - 1;
        if (x2 < 0)
            x2 = 0;
        if (x2 >= static_cast<int>(audio.size()))
            x2 = audio.size() - 1;
        auto mm = make_pair(audio[x1], audio[x1]);
        while (x1 < x2)
        {
            int l = 0;
            while ((x1 & (1 << l)) == 0 && x1 + (1 << l) < x2)
                ++l;
            if (l == 0)
            {
                if (mm.first > audio[x1])
                    mm.first = audio[x1];
                if (mm.second < audio[x1])
                    mm.second = audio[x1];
                ++x1;
            }
            else
            {
                if (mm.first > minMax[l - 1][x1 >> l].first)
                    mm.first = minMax[l - 1][x1 >> l].first;
                if (mm.second < minMax[l - 1][x1 >> l].second)
                    mm.second = minMax[l - 1][x1 >> l].second;
                x1 += (1 << l);
            }
        }
        glVertex2f(sx, mm.first);
        glVertex2f(sx, mm.second);
    }
    glEnd();
    const int linesize = thumbWidth * 3;
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
    for (size_t sx = 0; sx < width; sx += thumbWidth)
    {
        int x1 = (sx * zoom + x) * Fps / sampleRate;
        int x2 = ((sx + thumbWidth) * zoom + x) * Fps / sampleRate; 
        if (x1 < 0)
            x1 = 0;
        if (x1 >= static_cast<int>(audio.size() * Fps / sampleRate))
            x1 = audio.size() * Fps / sampleRate - 1;
        if (x2 < 0)
            x2 = 0;
        if (x2 >= static_cast<int>(audio.size() * Fps / sampleRate))
            x2 = audio.size() * Fps / sampleRate - 1;
        vector<int> mm(thumbLinesize * thumbHeight);
        int maxL = 0;
        vector<pair<int *, int> > ptrs;
        while (x1 < x2)
        {
            int l = 0;
            while ((x1 & (1 << l)) == 0 && x1 + (1 << l) < x2)
                ++l;
            {
                while (static_cast<int>(thumbs.size()) <= l)
                {
                    int f = open((fileName + ".thum" + to_string(thumbs.size())).c_str(), O_RDONLY);
                    if (f < 0)
                        throw runtime_error("cannot open the file: " + fileName + ".thum" + to_string(thumbs.size()));
                    auto fileSize = lseek(f, 0, SEEK_END);
                    if (fileSize == (off_t) -1)
                        throw runtime_error("cannot determine the file size: " + fileName + ".thum" + to_string(thumbs.size()));
                    auto p = mmap(nullptr, fileSize, PROT_READ, MAP_SHARED, f, 0);
                    close(f);
                    thumbs.push_back(make_pair(p, fileSize));
                }
                auto tmp = 1LL * (x1 >> l) * thumbLinesize * thumbHeight;
                if (tmp * sizeof(int) < thumbs[l].second)
                {
                    ptrs.push_back(make_pair((int *)thumbs[l].first + tmp, l));
                    if (maxL < l)
                        maxL = l;
                }
                x1 += (1 << l);
            }
        }
        int count = 0;
        for (auto ptr: ptrs)
            if (ptr.second >= maxL - 4)
            {
                for (size_t i = 0; i < mm.size(); ++i)
                    mm[i] += *(ptr.first++);
                count += (1 << ptr.second);
            }
        if (count > 0)
        {
            rgb.resize(linesize * thumbHeight);
            for (int y = 0; y < thumbHeight; ++y)
                for (int x = 0; x < thumbWidth; ++x)
                {
                    rgb[y * linesize + x * 3] = mm[y * thumbLinesize + x * 3] / count;
                    rgb[y * linesize + x * 3 + 1] = mm[y * thumbLinesize + x * 3 + 1] / count;
                    rgb[y * linesize + x * 3 + 2] = mm[y * thumbLinesize + x * 3 + 2] / count;
                }
            glTexImage2D(GL_TEXTURE_2D, 0, 3, thumbWidth, thumbHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, &rgb[0]);
            glBegin(GL_QUADS);

            glTexCoord2f(0, 1.0f - 1.0f / thumbHeight);
            glVertex2f(sx, height);

            glTexCoord2f(1.0f - 1.0f / thumbWidth, 1.0f - 1.0f / thumbHeight);
            glVertex2f(sx + thumbWidth - 1, height);

            glTexCoord2f(1.0f - 1.0f / thumbWidth, 0);
            glVertex2f(sx + thumbWidth - 1, height - (thumbHeight - 1));

            glTexCoord2f(0, 0);
            glVertex2f(sx, height - (thumbHeight - 1));

            glEnd();
        }
    }
    if (thumbs.size() > 0)
    {
        rgb.resize(linesize * thumbHeight);
        int64_t offset = 1LL * thumbLinesize * thumbHeight * ((p - l * sampleRate / 1000000) * Fps / sampleRate);
        if (offset < 0)
            offset = 0;
        if (offset * sizeof(int) > thumbs[0].second - thumbLinesize * thumbHeight * sizeof(int))
            offset = 0;
        int *tmp = (int *)thumbs[0].first + offset;
        for (int y = 0; y < thumbHeight; ++y)
            for (int x = 0; x < thumbWidth; ++x)
            {
                rgb[y * linesize + x * 3] = tmp[y * thumbLinesize + x * 3];
                rgb[y * linesize + x * 3 + 1] = tmp[y * thumbLinesize + x * 3 + 1];
                rgb[y * linesize + x * 3 + 2] = tmp[y * thumbLinesize + x * 3 + 2];
            }
        glTexImage2D(GL_TEXTURE_2D, 0, 3, thumbWidth, thumbHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, &rgb[0]);
        glBegin(GL_QUADS);

        glTexCoord2f(1.0f - 1.0f / thumbWidth, 0);
        glVertex2f(width - 1, 0);

        glTexCoord2f(0, 0);
        glVertex2f(width - thumbWidth * 2,  0);

        glTexCoord2f(0, 1.0f - 1.0f / thumbHeight);
        glVertex2f(width - thumbWidth * 2, (thumbHeight * 2 - 1));

        glTexCoord2f(1.0f - 1.0f / thumbWidth, 1.0f - 1.0f / thumbHeight);
        glVertex2f(width - 1, (thumbHeight * 2 - 1));

        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    drawSpec();
    glLoadIdentity();
    glOrtho(0, width, 1, 0, -1, 1);
    glColor3f(1, 0, 0);
    glBegin(GL_LINES);
    glVertex2f(sx, 1);
    glVertex2f(sx, 0);
    glEnd();
    glutSwapBuffers();
}

void reshape(int w, int h)
{
    width = w;
    height = h;
    glViewport(0, 0, w, h);  
}

bool fileExists(const string &name) 
{
    return ifstream(name).good();
}

struct Frame
{
    Frame(std::string fileName):
        file(fileName)
    {
        if (!file.is_open())
            throw runtime_error(string("file ") + fileName + " is not open ");
    }
    vector<int> data;
    int count;
    ofstream file;
};

void readVideoFile(string fileName)
{
    cout << "Reading video: " << fileName << endl;
    av_register_all();
    AVFormatContext *formatContext;

    formatContext = NULL;
    int len = avformat_open_input(&formatContext, fileName.c_str(), nullptr, nullptr);

    if (len != 0) 
    {
        cerr << "Could not open input " << fileName << endl;;
        throw -0x10;
    }
    
    if (avformat_find_stream_info(formatContext, NULL) < 0) 
    {
        cerr << "Could not read stream information from " <<  fileName << endl;
        throw -0x11;
    }
    av_dump_format(formatContext, 0, fileName.c_str(), 0);

    int audioStreamIndex = -1;
    int videoStreamIndex = -1;

    for (unsigned i = 0; i < formatContext->nb_streams; ++i)
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if (audioStreamIndex == -1)
                audioStreamIndex = i;
        }
        else if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (videoStreamIndex == -1)
                videoStreamIndex = i;
        }
    if (audioStreamIndex == -1)
    {
        cerr << "File does not have audio stream" << endl;
        throw -0x34;
    }
    if (videoStreamIndex == -1)
    {
        cerr << "File does not have video stream" << endl;
        throw -0x34;
    }

    auto codec = formatContext->streams[audioStreamIndex]->codec;
    AVCodecContext *audioDecodec;
    {
        if(codec->codec_id == 0)
        {
            cerr << "-0x30" << endl;
            throw -0x30;
        }
        AVCodec* c = avcodec_find_decoder(codec->codec_id);
        if (c == NULL)
        {
            cerr << "Could not find decoder ID " << codec->codec_id << endl;
            throw -0x31;
        }
        audioDecodec = avcodec_alloc_context3(c);
        if (audioDecodec == NULL)
        {
            cerr << "Could not alloc context for decoder " << c->name << endl;
            throw -0x32;
        }
        avcodec_copy_context(audioDecodec, codec);
        int ret = avcodec_open2(audioDecodec, c, NULL);
        if (ret < 0)
        {
            cerr << "Could not open stream decoder " << c->name;
            throw -0x33;
        }
    }
    codec = formatContext->streams[videoStreamIndex]->codec;
    AVCodecContext *videoDecodec;
    {
        if(codec->codec_id == 0)
        {
            cerr << "-0x30" << endl;
            throw -0x30;
        }
        AVCodec* c = avcodec_find_decoder(codec->codec_id);
        if (c == NULL)
        {
            cerr << "Could not find decoder ID " << codec->codec_id << endl;
            throw -0x31;
        }
        videoDecodec = avcodec_alloc_context3(c);
        if (videoDecodec == NULL)
        {
            cerr << "Could not alloc context for decoder " << c->name << endl;
            throw -0x32;
        }
        avcodec_copy_context(videoDecodec, codec);
        int ret = avcodec_open2(videoDecodec, c, NULL);
        if (ret < 0)
        {
            cerr << "Could not open stream decoder " << c->name;
            throw -0x33;
        }
    }
    sampleRate = audioDecodec->sample_rate;
    const auto channels = audioDecodec->channels;
    AVPacket packet;
    thumbHeight = 128;
    thumbWidth = videoDecodec->width * thumbHeight / videoDecodec->height;
    struct SwsContext *swsContext = sws_getContext(videoDecodec->width, videoDecodec->height, videoDecodec->pix_fmt, 
                                                   thumbWidth, thumbHeight, PIX_FMT_RGB24,
                                                   SWS_BICUBIC, NULL, NULL, NULL);
    if (swsContext == NULL) 
    {
        ostringstream err;
        err << "Could not create swscale context for " << videoDecodec->width << "x" << videoDecodec->height;
        throw runtime_error(err.str());
    }

    AVFrame *rgbFrame = avcodec_alloc_frame();
    if (!rgbFrame)
        throw runtime_error("Could not allocate memory for RGB frame");
    rgbFrame->width = thumbWidth;
    rgbFrame->height = thumbHeight;
    rgbFrame->format = PIX_FMT_RGB24;
    auto numBytes = avpicture_get_size((PixelFormat)rgbFrame->format, rgbFrame->width, rgbFrame->height);
    vector<shared_ptr<Frame> > levels;
    uint8_t *buffer = (uint8_t *)av_malloc(numBytes);
    avpicture_fill((AVPicture *)rgbFrame, buffer, (PixelFormat)rgbFrame->format, rgbFrame->width, rgbFrame->height);
    thumbLinesize = rgbFrame->linesize[0];
    bool isThumbCached = fileExists(fileName + ".thum0");
    bool firstAudioFrame = true;
    while (av_read_frame(formatContext, &packet) == 0)
    {
        if (packet.stream_index == audioStreamIndex)
        {
            if (firstAudioFrame)
            {
                firstAudioFrame = false;
                const auto c = packet.pts * audioDecodec->time_base.num * audioDecodec->sample_rate / audioDecodec->time_base.den;
                for (int i = 0; i < c; ++i)
                    audio.push_back(0);
            }
            int gotFrame = 0;
            AVFrame *decodedFrame = avcodec_alloc_frame();
            int len = avcodec_decode_audio4(audioDecodec, decodedFrame, &gotFrame, &packet);
            if (len >= 0)
            {
                if (gotFrame)
                {
                    int dataSize = av_samples_get_buffer_size(nullptr, channels,
                                                              decodedFrame->nb_samples,
                                                              audioDecodec->sample_fmt, 1);
                    if (channels == 1)
                        audio.insert(end(audio), (int16_t *)decodedFrame->data[0], (int16_t *)decodedFrame->data[0] + dataSize / sizeof(int16_t));
                    else
                    {
                        for (size_t i = 0; i < dataSize / sizeof(int16_t) / channels; ++i)
                        {
                            int sum = 0;
                            for (int c = 0; c < channels; ++c)
                                sum += ((int16_t *)decodedFrame->data[0])[i * channels + c];
                            audio.push_back(sum / channels);
                        }
                    }
                }
            }
            av_free(decodedFrame);
        }
        else if (packet.stream_index == videoStreamIndex && !isThumbCached)
        {
            if (packet.pts % (Fps * 10) == 0)
                clog << setfill('0') << setw(2) << packet.pts / Fps / 60 << ":"  << setw(2) << packet.pts / Fps % 60 << "."  << setw(2) << packet.pts % Fps << endl;
            AVFrame *decodedFrame = avcodec_alloc_frame();
            int result;
            avcodec_decode_video2(videoDecodec, decodedFrame, &result, &packet);
            if (result)
            {
                sws_scale(swsContext, decodedFrame->data, decodedFrame->linesize, 0, decodedFrame->height, 
                          rgbFrame->data, rgbFrame->linesize);
                int level = 0;
                while (level >= static_cast<int>(levels.size()))
                {
                    shared_ptr<Frame> f = make_shared<Frame>(fileName + ".thum" + to_string(levels.size()));
                    levels.push_back(f);
                    f->data.resize(rgbFrame->linesize[0] * rgbFrame->height);
                    for (auto &i: f->data)
                        i = 0;
                    f->count = 0;
                }
                auto f = levels[level];
                vector<int> d;
                for (auto i = rgbFrame->data[0]; i < rgbFrame->data[0] + rgbFrame->linesize[0] * rgbFrame->height; ++i)
                    d.push_back(*i);
                f->file.write((const char *)&d[0], d.size() * sizeof(d[0]));
                for (size_t i = 0; i < f->data.size(); ++i)
                    f->data[i] += rgbFrame->data[0][i];
                ++f->count;
                while (f->count > 1)
                {
                    ++level;
                    while (level >= static_cast<int>(levels.size()))
                    {
                        shared_ptr<Frame> f = make_shared<Frame>(fileName + ".thum" + to_string(levels.size()));
                        levels.push_back(f);
                        f->data.resize(rgbFrame->linesize[0] * rgbFrame->height);
                        for (auto &i: f->data)
                            i = 0;
                        f->count = 0;
                    }
                    auto f2 = levels[level];
                    f2->file.write((const char *)&f->data[0], f->data.size() * sizeof(f->data[0]));
                    for (size_t i = 0; i < f2->data.size(); ++i)
                    {
                        f2->data[i] += f->data[i];
                        f->data[i] = 0;
                    }
                    ++f2->count;
                    f->count = 0;
                    f = f2;
                }
            }
            av_free(decodedFrame);
        }
        av_free_packet(&packet);
    }
    av_free(buffer);
    av_free(rgbFrame);
    sws_freeContext(swsContext);
    avcodec_close(audioDecodec);
    av_free(audioDecodec);
    avformat_free_context(formatContext);

    if (!fileExists(fileName + ".pcs"))
    {
        int n = audio.size() / 2;
        const auto mm = make_pair(numeric_limits<int16_t>::max(), numeric_limits<int16_t>::min());
        while (n > 0)
        {
            minMax.push_back(vector<pair<int16_t, int16_t> >(n + 1));
            for (auto &i: minMax.back())
                i = mm;
            n /= 2;
        }
        size_t per = 0;
        for (size_t i = 0; i < audio.size(); ++i)
        {
            auto v = audio[i];
            for (size_t j = 0; j < minMax.size(); ++j)
            {
                assert(i / (1 << (j + 1)) < minMax[j].size());
                auto &z = minMax[j][i / (1 << (j + 1))];
                z = make_pair(min(z.first, v), max(z.second, v));
            }
            if (i >= per)
            {
                cout << i * 100 / audio.size() << endl;
                per += audio.size() / 100;
            }
        }
        ofstream f(fileName + ".pcs");
        for (const auto &i: minMax)
            f.write((const char *)&i[0], i.size() * sizeof(i[0]));
    }
    else
    {
        ifstream f(fileName + ".pcs");
        int n = audio.size() / 2;
        while (n > 0)
        {
            minMax.push_back(vector<pair<int16_t, int16_t> >(n + 1));
            f.read((char *)&minMax.back()[0], minMax.back().size() * sizeof(minMax.back()[0]));
            n /= 2;
        }
    }
}

int lastX = -1;
int lastMouseX = -1;

void mouse(int button, int state, int x, int y)
{
    if (button == GLUT_RIGHT_BUTTON)
    {
        follow = false;
        if (state == GLUT_DOWN)
        {
            lastX = ::x;
            lastMouseX = x;
        }
        else if (state == GLUT_UP)
            lastX = -1;
    }
    else if (button == GLUT_LEFT_BUTTON)
    {
        if (state == GLUT_DOWN)
            lastXX = x;
        if (state == GLUT_UP)
        {
            if (abs(lastXX - x) < 3)
            {
                auto p = x * zoom + ::x;
                if (p < 0)
                    p = 0;
                if (p >= audio.size())
                    p = audio.size() - 1;
                pos = p;
            }
            else
            {
                auto p1 = lastXX * zoom + ::x;
                auto p2 = x * zoom + ::x;
                bool isDel = false;
                if (p2 < p1)
                {
                    swap(p1, p2);
                    isDel = true;
                }
                auto r = make_pair(p1, p2);
                if (isDel)
                {
                    vector<int> rs;
                    decltype(rmList.find(Range(r.first, r.second))) i;
                    int speedUp = 6;
                    while ((i = rmList.find(Range(r.first, r.second))) != end(rmList))
                    {
                        rs.push_back(i->start);
                        rs.push_back(i->end);
                        rmList.erase(i);
                        speedUp = i->speedUp;
                    }
                    if (!rs.empty())
                    {
                        auto mm = minmax_element(begin(rs), end(rs));
                        auto nmm = make_pair(*mm.first, *mm.second);
                        if (nmm.first < r.second && nmm.first > r.first)
                            nmm.first = r.second;
                        if (nmm.second > r.first && nmm.second < r.second)
                            nmm.second = r.first;
                        if (nmm.first < nmm.second)
                            rmList.insert(Range(nmm.first, nmm.second, speedUp));
                    }
                }
                else
                {
                    vector<int> rs;
                    rs.push_back(r.first);
                    rs.push_back(r.second);
                    decltype(rmList.find(Range(r.first, r.second))) i;
                    while ((i = rmList.find(Range(r.first, r.second))) != end(rmList))
                    {
                        rs.push_back(i->start);
                        rs.push_back(i->end);
                        rmList.erase(i);
                    }
                    int speedUp = ((glutGetModifiers() & GLUT_ACTIVE_CTRL) != 0) ? 600 : 6;
                        
                    if (!rs.empty())
                    {
                        auto mm = minmax_element(begin(rs), end(rs));
                        rmList.insert(Range(*mm.first, *mm.second, speedUp));
                    }
                }
            }
            lastXX = -1;
            lastXX2 = -1;
        }
    }
    if (button == 3 || button == 4)
        if (state == GLUT_DOWN)
        {
            auto zoom0 = zoom;
            zoom *= pow(1.3, 3 * (button - 3.5f));
            ::x = x * zoom0 + ::x - x * zoom;
            glutPostRedisplay();
        }
}

void motion(int x, int y)
{
    if (lastX != -1)
        ::x = lastX + (lastMouseX - x) * zoom;
    if (lastXX != -1)
        lastXX2 = x;
    glutPostRedisplay();
}

volatile bool done = false;

void playerThread()
{
    static const pa_sample_spec ss = 
        {
            PA_SAMPLE_S16LE, /**< The sample format */
            sampleRate, /**< The sample rate. (e.g. 44100) */
            1 /**< Audio channels. (1 for mono, 2 for stereo, ...) */
        };
    int error;
    pa_simple *paSimple = pa_simple_new(NULL, "video_edit", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error);
    if (!paSimple)
    {
        cerr << __FILE__": pa_simple_new() failed: " << pa_strerror(error) << endl;
        return;
    }
    while (!done)
    {
        if (state == Playing)
        {
            if ((latency = pa_simple_get_latency(paSimple, &error)) == (pa_usec_t) -1) 
            {
                cerr <<  __FILE__": pa_simple_get_latency() failed: " << pa_strerror(error) << endl;
                pa_simple_free(paSimple);
                return;
            }

            /* ... and play it */
            if (pa_simple_write(paSimple, &audio[pos], 1024, &error) < 0) 
            {
                cerr << __FILE__": pa_simple_write() failed: " << pa_strerror(error) << endl;
                pa_simple_free(paSimple);
                return;
            }
            pos += 1024 / sizeof(int16_t);
            while (pos > static_cast<int>(audio.size()))
                pos -= audio.size();
            auto a = rmList.find(Range(pos, pos));
            if (a != end(rmList))
                pos = a->end;
        }
        else
            usleep(100);
    }
    pa_simple_free(paSimple);
}

void saveAudio()
{
    int skip = 0;
    size_t currentSample = 0;

    int skipCount = 0;

    auto removeRange = rmList.begin();
    vector<int16_t> result;
    vector<int> sum;
    int speedUp = 6;
    while (currentSample < audio.size())
    {
        vector<int16_t> buff;
        auto v = audio[currentSample];
        size_t frameSize;
        frameSize = 1024;;
        while (buff.size() < frameSize || ((v >= 0 || audio[currentSample] <= 0) && (abs(v) >= 10 || abs(audio[currentSample]) >= 10)))
        {
            v = audio[currentSample];
            buff.push_back(v);
            ++currentSample;
            if (currentSample >= audio.size())
                break;
        }
        if (removeRange != end(rmList) && static_cast<int>(currentSample) > removeRange->start)
        {
            speedUp = removeRange->speedUp;
            skip += 1LL * (removeRange->end - removeRange->start) * (speedUp - 1) / speedUp;
            cout << currentSample << "\t" << 1.0 * currentSample / sampleRate << "\tskip + " 
                 << 1.0 * 1LL * (removeRange->end - removeRange->start) * (speedUp - 1) / speedUp / sampleRate<< " =\t" 
                 << 1.0 * skip / sampleRate << "\t" 
                 << speedUp << endl;
            ++removeRange;
        }
        cout << currentSample << " " << skip << " " << currentSample + skip << endl;
        if (skip > 0 && skipCount < speedUp)
        {
            skip -= buff.size();
            if (sum.size() < buff.size())
                sum.resize(buff.size());
            
            for (size_t i = 0; i < std::min(sum.size(), buff.size()); ++i)
                sum[i] += buff[i];
                
            ++skipCount;
        }
        else
        {
            cout << "." << endl;
            if (sum.size() < buff.size())
                sum.resize(buff.size());
            ++skipCount;
            if (skipCount < 10)
            {
                for (size_t i = 0; i < sum.size(); ++i)
                {
                    int r = (sum[i] + (i < buff.size() ? buff[i] : 0)) * 2 / (skipCount + 1);
                    if (r > 0x7800)
                        r = 0x7800;
                    if (r < -0x7800)
                        r = -0x7800;
                    result.push_back(r);
                }
                skip += sum.size() - buff.size();
            }
            else
                for (size_t i = 0; i < buff.size(); ++i)
                    result.push_back(buff[i]);
            skipCount = 0;
            sum.resize(0);
        }
    }
    ofstream f(fileName + ".s16l");
    f.write((const char *)&result[0], result.size() * sizeof(result[0]));
}

thread *t;

void bye()
{
    clog << "bye" << endl;
    done = true;
    t->join();
    ofstream f(fileName + "_rm.txt");
    for (auto i: rmList)
        f << i.start << " " << i.end << " " << i.speedUp << endl;
    saveAudio();
    fftw_destroy_plan(plan);
    fftw_free(fftIn);
    fftw_free(fftOut);
}


void timer(int = 0)
{
    auto p = pos;
    auto l = latency;
    if (state == Playing)
    {
        auto sx = (p - x - l * sampleRate / 1000000) / zoom;
        if (sx < 0 || sx > width || follow)
        {
            follow = true;
            double tmp = p - l * sampleRate / 1000000 - (width / 10) * zoom;
            if (abs(0.00001 * (tmp - x)) > 10)
                x = x + 0.00001 * (tmp - x);
            else
                x = tmp;
        }
    }
    glutTimerFunc(0, timer, 0);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y)
{
    if (key == ' ')
    {
        if (state == Stop)
            state = Playing;
        else
        {
            state = Stop;
            follow = false;
        }
    }
}

void special(int key, int x, int y)
{
    if (state == Stop)
    {
        auto p = pos;
        auto l = latency;
        const auto Offset = sampleRate / Fps;
        if (key == GLUT_KEY_LEFT)
        {
            p = (p - Offset - l * sampleRate / 1000000) / Offset * Offset +  l * sampleRate / 1000000;
            if (p < 0)
                p = 0;
            pos = p;
            glutPostRedisplay();
        }
        else if (key == GLUT_KEY_RIGHT)
        {
            p = (p + Offset - l * sampleRate / 1000000) / Offset * Offset + l * sampleRate / 1000000;
            if (p >= static_cast<int>(audio.size()))
                p = audio.size() - 1;
            pos = p;
            glutPostRedisplay();
        }
        auto sx = (p - ::x - l * sampleRate / 1000000) / zoom;
        if (sx < 0 || sx > width)
            ::x = p - l * sampleRate / 1000000 - (width / 10) * zoom;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        cerr << "Usage: video_edit file_name" << endl;
        return -1;
    }
    fileName = argv[1];

    fftIn = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * SpecSize);
    fftOut = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * SpecSize);
    plan = fftw_plan_dft_1d(SpecSize, fftIn, fftOut, FFTW_FORWARD, FFTW_MEASURE);
    
    readVideoFile(fileName);
    if (!fileExists(fileName + "_rm.txt"))
        rmList = silenceDetector(audio);
    else
    {
        ifstream f(fileName + "_rm.txt");

        while (!f.eof())
        {
            int b, e, s;
            f >> b >> e >> s;
            if (!f.eof())
                rmList.insert(Range(b, e, s));
        }
    }
    width = 1280;
    height = 720;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(width, height);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Video Editor");
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0x8000, -0x8000, -1, 1);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    timer();
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    t = new thread(playerThread);
    atexit(bye);
    glutMainLoop();
}
