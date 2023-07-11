#pragma once
#include "silence_detector.h"
#include <string>

std::set<Range, Cmp> extractSilencesFromCaptions(std::string fileName);
