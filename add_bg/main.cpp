#include <fstream>
#include <iostream>
using namespace std;

int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        cerr << "Usage: add_bg file_name.s16l bg_music.s16l" << endl;
        return -1;
    }
    ifstream f(argv[1]);
    ifstream *bg = nullptr;
    float k = 0;
    while (!f.eof())
    {
        int16_t v1;
        f.read((char *)&v1, sizeof(v1));
        int16_t v2;
        if (!bg)
            bg = new ifstream(argv[2]);
        bg->read((char *)&v2, sizeof(v2));
        v1 += v2 * k;
        if (k < 0.15)
            k += 0.15 / 5 / 48000;
        cout.write((char *)&v1, sizeof(v1));
        if (bg->eof())
        {
            delete bg;
            bg = nullptr;
        }
    }
}
