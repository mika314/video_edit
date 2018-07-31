#include <coddle/config.hpp>

void configure(Config& cfg)
{
  cfg.common.libs.push_back("GL");
  cfg.multithread = true;
}
