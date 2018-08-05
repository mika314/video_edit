#include "silence_detector.h"
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
#include <vector>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fftw3.h>
#include <unordered_map>

using namespace std;

vector<int16_t> audio;

set<Range, Cmp > rmList;
vector<vector<pair<int16_t, int16_t> > > minMax;
size_t width;
size_t height;
double x = 0;
float zoom = 1.0f;

string fileName;

int lastXX = -1;
int lastXX2 = -1;
vector<unsigned char> rgb;
bool follow = false;

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
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if (audioStreamIndex == -1)
                audioStreamIndex = i;
        }
        else if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
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
    const auto channels = audioDecodec->channels;
    std::cout << "channels: " << channels << std::endl;
    switch (audioDecodec->sample_fmt)
    {
    case AV_SAMPLE_FMT_NONE:
      std::cout << "sample_fmt: AV_SAMPLE_FMT_NONE" << std::endl;
      break;
    case AV_SAMPLE_FMT_U8:
      std::cout << "sample_fmt: U8" << std::endl;
      break;
    case AV_SAMPLE_FMT_S16:
      std::cout << "sample_fmt: S16" << std::endl;
      break;
    case AV_SAMPLE_FMT_S32:
      std::cout << "sample_fmt: S32" << std::endl;
      break;
    case AV_SAMPLE_FMT_FLT:
      std::cout << "sample_fmt: FLT" << std::endl;
      break;
    case AV_SAMPLE_FMT_DBL:
      std::cout << "sample_fmt: DBL" << std::endl;
      break;
    case AV_SAMPLE_FMT_U8P:
      std::cout << "sample_fmt: U8P" << std::endl;
      break;
    case AV_SAMPLE_FMT_S16P:
      std::cout << "sample_fmt: S16P" << std::endl;
      break;
    case AV_SAMPLE_FMT_S32P:
      std::cout << "sample_fmt: S32P" << std::endl;
      break;
    case AV_SAMPLE_FMT_FLTP:
      std::cout << "sample_fmt: FLTP" << std::endl;
      break;
    case AV_SAMPLE_FMT_DBLP:
      std::cout << "sample_fmt: DBLP" << std::endl;
      break;
    case AV_SAMPLE_FMT_NB:
      std::cout << "sample_fmt: NB" << std::endl;
      break;
    case AV_SAMPLE_FMT_S64:
      std::cout << "sample_fmt: S64" << std::endl;
      break;
    case AV_SAMPLE_FMT_S64P:
      std::cout << "sample_fmt: S64P" << std::endl;
      break;
    }                    
    AVPacket packet;
    bool firstAudioFrame = true;
    while (av_read_frame(formatContext, &packet) == 0)
    {
        if (packet.stream_index == audioStreamIndex)
        {
            const auto c = packet.pts * audioDecodec->pkt_timebase.num * audioDecodec->sample_rate / audioDecodec->pkt_timebase.den;
            if (firstAudioFrame)
            {
                firstAudioFrame = false;
                for (int i = 0; i < c; ++i)
                    audio.push_back(0);
            }
            else
            {
              if (std::abs(static_cast<long long>(c) - static_cast<long long>(audio.size())) > 44100 / 30)
              {
                  std::clog << "Time correction: " << audio.size() << " to " << c << std::endl;
                  audio.resize(c);
              }
            }
            int gotFrame = 0;
            AVFrame *decodedFrame = av_frame_alloc();
            int len = avcodec_decode_audio4(audioDecodec, decodedFrame, &gotFrame, &packet);
            if (len >= 0)
            {
                if (gotFrame)
                {
                    int dataSize = av_samples_get_buffer_size(nullptr, channels,
                                                              decodedFrame->nb_samples,
                                                              audioDecodec->sample_fmt, 1);
                    if (audioDecodec->sample_fmt == AV_SAMPLE_FMT_FLT)
                    {
                        for (size_t i = 0; i < dataSize / sizeof(float) / channels; ++i)
                        {
                            int sum = 0;
                            for (int c = 0; c < channels; ++c)
                                sum += ((float *)decodedFrame->data[0])[i * channels + c] * 0x8000;
                            audio.push_back(sum / channels);
                        }
                    }
                    else if (audioDecodec->sample_fmt == AV_SAMPLE_FMT_FLTP)
                    {
                        for (size_t i = 0; i < dataSize / sizeof(float) / channels; ++i)
                        {
                            int sum = 0;
                            for (int c = 0; c < channels; ++c)
                                sum += ((float *)decodedFrame->data[0])[i + c * dataSize / sizeof(float) / channels] * 0x8000;
                            audio.push_back(sum / channels);
                        }
                    }
                    else if (audioDecodec->sample_fmt == AV_SAMPLE_FMT_S16)
                    {
                        for (size_t i = 0; i < dataSize / sizeof(int16_t) / channels; ++i)
                        {
                            int sum = 0;
                            for (int c = 0; c < channels; ++c)
                                sum += ((int16_t *)decodedFrame->data[0])[i * channels + c];
                            audio.push_back(sum / channels);
                        }
                    }
                    else if (audioDecodec->sample_fmt == AV_SAMPLE_FMT_S16P)
                    {
                        for (size_t i = 0; i < dataSize / sizeof(int16_t) / channels; ++i)
                        {
                            int sum = 0;
                            for (int c = 0; c < channels; ++c)
                                sum += ((int16_t *)decodedFrame->data[0])[i + c * dataSize / sizeof(int16_t) / channels];
                            audio.push_back(sum / channels);
                        }
                    }
                }
            }
            av_free(decodedFrame);
        }
        av_packet_unref(&packet);
    }
}

void saveAudio()
{
    ssize_t inputSample = 0;
    ssize_t outputSample = 0;

    int skipCount = 0;

    auto removeRange = rmList.begin();
    vector<int16_t> result;
    vector<int> sum;
    int speedUp = 6;
    while (inputSample < static_cast<ssize_t>(audio.size()))
    {
        vector<int16_t> buff;
        auto v = audio[inputSample];
        const size_t frameSize = 1024;
        while (buff.size() < frameSize || ((v >= 0 || audio[inputSample] <= 0) && (abs(v) >= 10 || abs(audio[inputSample]) >= 10)))
        {
            v = audio[inputSample];
            buff.push_back(v);
            ++inputSample;
            ++outputSample;
            if (inputSample >= static_cast<ssize_t>(audio.size()))
                break;
        }
        if (removeRange != end(rmList) && inputSample > removeRange->start)
        {
            speedUp = removeRange->speedUp;
            outputSample -= removeRange->end - removeRange->start - (removeRange->end - removeRange->start) / speedUp;
            cout << inputSample << "\t" << inputSample << "\tskip + "
                 << (removeRange->end - removeRange->start) * (speedUp - 1) / speedUp << " =\t" 
                 << speedUp << endl;
            ++removeRange;
        }
        if (static_cast<ssize_t>(result.size()) > outputSample && skipCount < speedUp)
        {
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

void bye()
{
    clog << "bye" << endl;
    ofstream f(fileName + "_rm.txt");
    for (auto i: rmList)
        f << i.start << " " << i.end << " " << i.speedUp << endl;
    saveAudio();
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        cerr << "Usage: video_edit file_name" << endl;
        return -1;
    }
    fileName = argv[1];

    readVideoFile(fileName);

    std::unordered_map<int16_t, int> hist;
    for (auto y: audio)
      hist[y]++;
    auto sum = 0u;
    auto max = 0x7fff;
    while (sum < audio.size() / 10000)
      sum += hist[max--];
    std::cout << "Average: " << 1.0f * max / 0x7fff << std::endl;
      for (auto &y: audio)
      {
        int64_t tmp = y * 0x7000 / max;
        if (tmp > 0x7fff)
          tmp = 0x7fff;
        if (tmp < -0x7fff)
          tmp = -0x7fff;
        y = tmp;
      }

    rmList = silenceDetector(audio);
    bye();
}
