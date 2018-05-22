#include <coddle/config.hpp>

void configure(Config& cfg)
{
  cfg.multithread = true;
  cfg.common.libs.push_back("pulse-simple");
  cfg.common.libs.push_back("pulse");
}
