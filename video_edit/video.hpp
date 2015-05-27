#pragma once
#include <string>
#include <vector>

class Video
{
public:
  Video(const std::string &fileName);
  std::pair<int, int> audioMinMax(int sample1, int sample2) const;
  int sampleRate() const;
  unsigned char *thumb(int frame1, int frame2) const;
  int thumbHeight() const;
  int thumbWidth() const;
  unsigned char *frame(int frame);
  int height() const;
  int width() const;
  std::pair<int, int> fps() const;
  const int16_t *audio() const;
private:
  mutable std::vector<unsigned char> image_;
  std::vector<int16_t> audio_;
};
