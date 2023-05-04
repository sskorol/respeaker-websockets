# include "log.hpp"

void registerLoggers()
{
  init_thread_pool(8192, 1);
  auto stdout_sink = make_shared<sinks::stdout_color_sink_mt >();
  auto rotating_sink = make_shared<sinks::daily_file_sink_mt>(LOGS_FILE, 0, 0);
  vector<sink_ptr> sinks {stdout_sink, rotating_sink};
  auto logger = make_shared<async_logger>(LOGGER_NAME, sinks.begin(), sinks.end(), thread_pool(), async_overflow_policy::block);
  register_logger(logger);
  flush_every(chrono::seconds(FLUSHING_INTERVAL));
}
