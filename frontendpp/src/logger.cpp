#include "logger.h"

namespace FrontendPP {
    Logger* Logger::instance_ = nullptr;
    std::mutex Logger::mutex_;
}
