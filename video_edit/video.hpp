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
  void read();
  void minMaxCalc();
  std::string fileName_;
  mutable std::vector<unsigned char> image_;
  std::vector<int16_t> audio_;
  vector<vector<pair<int16_t, int16_t> > > minMax_;
  vector<pair<void *, size_t> > thumbs_;
  int thumbLinesize_;
  int thumbHeight_;
  int thumbWidth_;
  size_t width_;
  size_t height_;
  int sampleRate_;
};
