#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <set>
#include <algorithm>
using namespace std;

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        cerr << "Usage: avconv -i video.mp4 -f yuv4mpegpipe - | ./frame_remover rm_list.txt sample_rate" << endl;
        return -1;
    }
    std::string fileName = argv[1];
    int sampleRate = atoi(argv[2]);
    ifstream f(fileName);
    if (f.bad())
    {
        cerr << "file rmlist does not exist" << endl;
        return -1;
    }

    set<pair<int64_t, int64_t> > frames;

    while (!f.eof())
    {
        int64_t b, e;
        f >> b >> e;
        if (!f.eof())
            frames.insert(make_pair(b, e));
    }

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

    auto yuv = new unsigned char[w * h * 3 / 2];
    auto yuvAve = new int[w * h * 3 / 2];
    for (auto ave = yuvAve; ave != yuvAve + w * h * 3 / 2; ++ave)
        *ave = 0;

    int skip = 0;
    int currentFrame = 0;

    int skipCount = 0;
    const int SpeedUp = 6;

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
        ++currentFrame;
        if (frames.size() > 0 && 1LL * currentFrame * sampleRate / fps > frames.begin()->first)
        {
            skip += (frames.begin()->second - frames.begin()->first) * (SpeedUp - 1) / SpeedUp;
            clog << frames.begin()->second << " " <<  frames.begin()->first << endl;
            frames.erase(frames.begin());
        }
        if (skip > 0 && skipCount < SpeedUp)
        {
            skip -= sampleRate / fps;
            {
                auto cur = yuv;
                auto ave = yuvAve;
                for (; ave < yuvAve + w * h; ++ave, ++cur)
                    *ave += *cur;
                for (; ave < yuvAve + w * h * 3 / 2; ++ave, ++cur)
                    *ave += *cur - 128;
            }
            ++skipCount;
        }
        else
        {
            cout << "FRAME\n";
            ++skipCount;
            {
                auto cur = yuv;
                auto ave = yuvAve;
                for (; ave != yuvAve + w * h; ++ave, ++cur)
                {
                    *cur = (*ave + *cur) / skipCount;
                    *ave = 0;
                }
                for (; ave != yuvAve + w * h * 3 / 2; ++ave, ++cur)
                {
                    *cur = (*ave + *cur - 128) / skipCount + 128;
                    *ave = 0;
                }
            }
            cout.write((char *)yuv, w * h * 3 / 2);
            skipCount = 0;
        }
    }

    delete [] yuv;
    delete [] yuvAve;
}
