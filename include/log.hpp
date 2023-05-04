#ifndef LOG_HPP
#define LOG_HPP

#include <spdlog/async.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace std;
using namespace spdlog;

const string LOGGER_NAME = "DSP";
const string LOGS_FILE = "logs/daily.txt";
const unsigned int FLUSHING_INTERVAL = 3;

void registerLoggers();

template<typename FormatString, typename... Args>
void logInfo(const FormatString &fmt, Args&&...args)
{
  get(LOGGER_NAME)->info(fmt, std::forward<Args>(args)...);
}

#endif
