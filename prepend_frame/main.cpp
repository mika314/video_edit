#include <iostream>
#include <fstream>
#include <vector>
extern "C" 
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswscale/swscale.h>
}
using namespace std;

vector<unsigned char> readImage(string fileName, int outputWidth, int outputHeight)
{
    AVFormatContext *formatContext = nullptr;
    int len = avformat_open_input(&formatContext, fileName.c_str(), nullptr, nullptr);
    if (len != 0) 
    {
        cerr << "Could not open input " << fileName << endl;;
        exit(-0x10);
    }
    if (avformat_find_stream_info(formatContext, NULL) < 0) 
    {
        cerr << "Could not read stream information from " <<  fileName << endl;
        exit(-0x11);
    }
    av_dump_format(formatContext, 0, fileName.c_str(), 0);
    int videoStreamIndex = -1;
    for (unsigned i = 0; i < formatContext->nb_streams; ++i)
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
            break;
        }
    if (videoStreamIndex == -1)
    {
        cerr << "File does not have video stream" << endl;
        exit(-0x34);
    }

    auto codec = formatContext->streams[videoStreamIndex]->codec;
    AVCodecContext *videoDecodec;
    {
        if(codec->codec_id == 0)
        {
            cerr << "-0x30" << endl;
            exit(-0x30);
        }
        AVCodec* c = avcodec_find_decoder(codec->codec_id);
        if (c == NULL)
        {
            cerr << "Could not find decoder ID " << codec->codec_id << endl;
            exit(-0x31);
        }
        videoDecodec = avcodec_alloc_context3(c);
        if (videoDecodec == NULL)
        {
            cerr << "Could not alloc context for decoder " << c->name << endl;
            exit(-0x32);
        }
        avcodec_copy_context(videoDecodec, codec);
        int ret = avcodec_open2(videoDecodec, c, NULL);
        if (ret < 0)
        {
            cerr << "Could not open stream decoder " << c->name;
            exit(-0x33);
        }
    }
    vector<unsigned char> result;
    AVPacket packet;
    while (av_read_frame(formatContext, &packet) == 0)
    {
        if (packet.stream_index == videoStreamIndex)
        {
            AVFrame *decodedFrame = avcodec_alloc_frame();
            int r;
            avcodec_decode_video2(videoDecodec, decodedFrame, &r, &packet);
            if (r)
            {
                // uint8_t *data[AV_NUM_DATA_POINTERS];
                clog << "linesize: " << endl;
                for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i)
                    clog << decodedFrame->linesize[i] << endl;
#define _(x) clog << #x << ": " << decodedFrame->x << endl
                _(key_frame);
                _(pts);
                _(coded_picture_number);
                _(display_picture_number);
                _(quality);
                _(reference);
                _(pkt_pts);
                _(pkt_dts);
                _(sample_aspect_ratio.num);
                _(sample_aspect_ratio.den);
                _(width);
                _(height);
#define __(x) case x: clog << "format: "#x << endl; break
                switch (decodedFrame->format)
                {
                    __(PIX_FMT_NONE); __(PIX_FMT_YUV420P); __(PIX_FMT_YUYV422); __(PIX_FMT_RGB24); __(PIX_FMT_BGR24); __(PIX_FMT_YUV422P);
                    __(PIX_FMT_YUV444P); __(PIX_FMT_YUV410P); __(PIX_FMT_YUV411P); __(PIX_FMT_GRAY8); __(PIX_FMT_MONOWHITE); __(PIX_FMT_MONOBLACK);
                    __(PIX_FMT_PAL8); __(PIX_FMT_YUVJ420P); __(PIX_FMT_YUVJ422P); __(PIX_FMT_YUVJ444P); __(PIX_FMT_XVMC_MPEG2_MC); __(PIX_FMT_XVMC_MPEG2_IDCT);
                    __(PIX_FMT_UYVY422); __(PIX_FMT_UYYVYY411); __(PIX_FMT_BGR8); __(PIX_FMT_BGR4); __(PIX_FMT_BGR4_BYTE); __(PIX_FMT_RGB8);
                    __(PIX_FMT_RGB4); __(PIX_FMT_RGB4_BYTE); __(PIX_FMT_NV12); __(PIX_FMT_NV21);

                    __(PIX_FMT_ARGB); __(PIX_FMT_RGBA); __(PIX_FMT_ABGR); __(PIX_FMT_BGRA);

                    __(PIX_FMT_GRAY16BE); __(PIX_FMT_GRAY16LE); __(PIX_FMT_YUV440P); __(PIX_FMT_YUVJ440P); __(PIX_FMT_YUVA420P);
                    __(PIX_FMT_VDPAU_H264); __(PIX_FMT_VDPAU_MPEG1); __(PIX_FMT_VDPAU_MPEG2); __(PIX_FMT_VDPAU_WMV3); __(PIX_FMT_VDPAU_VC1);
                    __(PIX_FMT_RGB48BE); __(PIX_FMT_RGB48LE);

                    __(PIX_FMT_RGB565BE); __(PIX_FMT_RGB565LE); __(PIX_FMT_RGB555BE);  __(PIX_FMT_RGB555LE);

                    __(PIX_FMT_BGR565BE); __(PIX_FMT_BGR565LE); __(PIX_FMT_BGR555BE); __(PIX_FMT_BGR555LE);

                    __(PIX_FMT_VAAPI_MOCO); __(PIX_FMT_VAAPI_IDCT); __(PIX_FMT_VAAPI_VLD);

                    __(PIX_FMT_YUV420P16LE); __(PIX_FMT_YUV420P16BE); __(PIX_FMT_YUV422P16LE); __(PIX_FMT_YUV422P16BE);
                    __(PIX_FMT_YUV444P16LE); __(PIX_FMT_YUV444P16BE); __(PIX_FMT_VDPAU_MPEG4); __(PIX_FMT_DXVA2_VLD);

                    __(PIX_FMT_RGB444LE); __(PIX_FMT_RGB444BE); __(PIX_FMT_BGR444LE); __(PIX_FMT_BGR444BE);
                    __(PIX_FMT_Y400A); __(PIX_FMT_BGR48BE); __(PIX_FMT_BGR48LE); __(PIX_FMT_YUV420P9BE);
                    __(PIX_FMT_YUV420P9LE); __(PIX_FMT_YUV420P10BE); __(PIX_FMT_YUV420P10LE); __(PIX_FMT_YUV422P10BE);
                    __(PIX_FMT_YUV422P10LE); __(PIX_FMT_YUV444P9BE); __(PIX_FMT_YUV444P9LE); __(PIX_FMT_YUV444P10BE);
                    __(PIX_FMT_YUV444P10LE); __(PIX_FMT_YUV422P9BE); __(PIX_FMT_YUV422P9LE); __(PIX_FMT_VDA_VLD);
                    __(PIX_FMT_GBRP); __(PIX_FMT_GBRP9BE); __(PIX_FMT_GBRP9LE); __(PIX_FMT_GBRP10BE); __(PIX_FMT_GBRP10LE);
                    __(PIX_FMT_GBRP16BE); __(PIX_FMT_GBRP16LE); __(PIX_FMT_NB);
                }
#undef __
#undef _
                AVFrame *yuvFrame = avcodec_alloc_frame();
                if (!yuvFrame)
                {
                    cerr << "Couldnot allocate memory for YUV frame" << endl;
                    exit(-1);
                }
                yuvFrame->width = outputWidth;
                yuvFrame->height = outputHeight;
                yuvFrame->format = PIX_FMT_YUV420P;
                const auto NumBytes = avpicture_get_size((PixelFormat)yuvFrame->format, yuvFrame->width, yuvFrame->height);
                uint8_t *yuvBuffer = (uint8_t *)(av_malloc(NumBytes));
                avpicture_fill((AVPicture *)yuvFrame, yuvBuffer, (PixelFormat)yuvFrame->format, yuvFrame->width, yuvFrame->height);
                auto swsContext = sws_getContext(decodedFrame->width, decodedFrame->height, (PixelFormat)decodedFrame->format,
                                                 yuvFrame->width, yuvFrame->height, PIX_FMT_YUV420P,
                                                 SWS_BICUBIC, nullptr, nullptr, nullptr);
                sws_scale(swsContext, decodedFrame->data, decodedFrame->linesize, 0, decodedFrame->height,
                          yuvFrame->data, yuvFrame->linesize);
                sws_freeContext(swsContext);
                
                for (int y = 0; y < yuvFrame->height; ++y)
                    result.insert(end(result), yuvFrame->data[0] + yuvFrame->linesize[0] * y, yuvFrame->data[0] + yuvFrame->linesize[0] * y + yuvFrame->width);

                for (int y = 0; y < yuvFrame->height / 2; ++y)
                    result.insert(end(result), yuvFrame->data[1] + yuvFrame->linesize[1] * y, yuvFrame->data[1] + yuvFrame->linesize[1] * y + yuvFrame->width / 2);

                for (int y = 0; y < yuvFrame->height / 2; ++y)
                    result.insert(end(result), yuvFrame->data[2] + yuvFrame->linesize[2] * y, yuvFrame->data[2] + yuvFrame->linesize[2] * y + yuvFrame->width / 2);
                av_free(yuvBuffer);
                av_free(yuvFrame);
            }
            av_free(decodedFrame);
        }
        av_free_packet(&packet);
    }
    avcodec_close(videoDecodec);
    av_free(videoDecodec);
    avformat_free_context(formatContext);
    return result;
}

int main(int argc, const char *argv[])
{
    av_register_all();
    if (argc != 3)
    {
        cerr << "Usage: ffmpeg -i vidoe.mp4 -f yuv4mpegpipe - | ./prepend_frame frame.png durationInMs" << endl;
        return -1;
    }
    const std::string fileName = argv[1];
    const int Duration = atoi(argv[2]);
    
    
    char line[300];
    cin.getline(line, sizeof(line));
    cout << line << "\n";
    int w = 0;
    int h = 0;
    int fps = 0;
    int isw = false;
    int ish = false;
    int isfps = false;
    for (char *i = line; *i != 0; ++i)
    {
        switch (*i)
        {
        case 'W':
            isw = true;
            break;
        case 'H':
            ish = true;
            break;
        case 'F':
            isfps = true;
            break;
        default:
            if (isdigit(*i))
            { 
                if (isw)
                    w = w * 10 + *i - '0';
                else if (ish)
                    h = h * 10 + *i - '0';
                else if (isfps)
                    fps = fps * 10 + *i - '0';
            }
            else
            {
                isw = false;
                ish = false;
                isfps = false;
            }
        }
    }
    clog << w << "x" << h << " fps: " << fps << endl;

    auto image = readImage(fileName, w, h);
    for (int i = 0; i < 24 * Duration / 1000; ++i)
    {
        cout << "FRAME\n";
        cout.write((const char *)&image[0], image.size());
    }

    auto yuv = new unsigned char[w * h * 3 / 2];
    while (!cin.eof())
    {
        char line[300];
        do 
        {
            cin.getline(line, sizeof(line));
            if (strlen(line) == 0)
                return false;
        } while (strcmp(line, "FRAME") != 0);

        cin.read((char *)yuv, w * h * 3 / 2);
        cout << "FRAME\n";
        cout.write((char *)yuv, w * h * 3 / 2);
    }

    delete [] yuv;
}
