#pragma once

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "envoy/thread/thread.h"
#include "common/common/macros.h"

#include "spdlog/spdlog.h"

#ifdef GLOG_ON

#include "glog/logging.h"

#else

// When glog is not used, these values replace the #defines from glog.
namespace Envoy {

// SPD_ONLY because TRACE and DEBUG are not used in glog.  These are defined for use in macros below
// to convert the internal definition of VLOG(x) to types of logging that spdlog understands.
constexpr int SPD_ONLY_TRACE = 1;
constexpr int SPD_ONLY_DEBUG = 2;

// Declare the usual glog #defines with internally defined values to leave space for the two constants defined above.
constexpr int INFO = 3;
constexpr int WARNING = 4;
constexpr int ERROR = 5;
constexpr int FATAL = 6;
} // Envoy

#endif

namespace Envoy {
namespace Logger {

// clang-format off
#define ALL_LOGGER_IDS(FUNCTION) \
  FUNCTION(admin)                \
  FUNCTION(assert)               \
  FUNCTION(backtrace)            \
  FUNCTION(client)               \
  FUNCTION(config)               \
  FUNCTION(connection)           \
  FUNCTION(file)                 \
  FUNCTION(filter)               \
  FUNCTION(hc)                   \
  FUNCTION(http)                 \
  FUNCTION(http2)                \
  FUNCTION(main)                 \
  FUNCTION(mongo)                \
  FUNCTION(pool)                 \
  FUNCTION(redis)                \
  FUNCTION(router)               \
  FUNCTION(runtime)              \
  FUNCTION(testing)              \
  FUNCTION(upstream)

enum class Id {
  ALL_LOGGER_IDS(GENERATE_ENUM)
};
// clang-format on

#ifdef GLOG_ON

#define LOG_TO_OBJECT(LOG_OBJECT, LEVEL) LOG(LEVEL)

#define VLOG_TO_OBJECT(LOG, LEVEL) VLOG(LEVEL)

#define SINK_LOG(SINK, LEVEL) LOG_TO_SINK_BUT_NOT_TO_LOGFILE(SINK, LEVEL)

#define SINK_VLOG(SINK, LEVEL) (!VLOG_IS_ON(LEVEL)) ? (void) 0 : google::LogMessageVoidify() & LOG_TO_SINK_BUT_NOT_TO_LOGFILE(SINK, INFO)


class Logger { 
  public:
  std::string levelString() const { return spdlog::level::level_names[logger_->level()]; }
  std::string name() const { return logger_->name(); }
  void setLevel(spdlog::level::level_enum level) const { logger_->set_level(level); }
  LogSink &sink();

private:
  Logger(const std::string& name);

  friend class Registry;
};

class LockingStderrSink : public google::LogSink {
public:
  void setLock(Thread::BasicLockable& lock) { lock_ = &lock; }
  void send(google::LogSeverity severity, const char* full_filename,
                    const char* base_filename, int line,
                    const struct ::tm* tm_time,
                    const char* message, size_t message_len) override {
    UNREFERENCED_PARAMETER(full_filename);
    Thread::OptionalLockGuard<Thread::BasicLockable> guard(lock_);
    std::cerr << ToString(severity, base_filename, line, tm_time, message, message_len) << std::endl;
  }

private:
  Thread::BasicLockable* lock_{};
};


#else

class SpdLogStream {
public:
  SpdLogStream(spdlog::logger& logger, int log_severity)
      : logger_(logger), log_severity_(log_severity) {}
  virtual ~SpdLogStream() {
    switch (log_severity_) {
    case SPD_ONLY_TRACE:
      logger_.trace(stream_.str());
      break;
    case SPD_ONLY_DEBUG:
      logger_.debug(stream_.str());
      break;
    case INFO:
      logger_.info(stream_.str());
      break;
    case WARNING:
      logger_.warn(stream_.str());
      break;
    case ERROR:
      logger_.critical(stream_.str());
      break;
    default:
      logger_.trace(stream_.str());
      break;
    }
  }

  template <typename T> SpdLogStream& operator<<(T&& t) {
    stream_ << std::forward<T>(t);
    return *this;
  }

private:
  std::ostringstream stream_;
  spdlog::logger& logger_;
  const int log_severity_;
};

#define LOG_TO_OBJECT(LOG_OBJECT, LEVEL) Logger::SpdLogStream(LOG_OBJECT, LEVEL)

#define VLOG_TO_OBJECT(LOG_OBJECT, LEVEL)                                                          \
  Logger::SpdLogStream(LOG_OBJECT, (LEVEL == 1) ? SPD_ONLY_DEBUG : SPD_ONLY_TRACE)

#define LOG(LEVEL) LOG_TO_OBJECT(log(), LEVEL)

#define VLOG(LEVEL) VLOG_TO_OBJECT(log(), LEVEL)

#define SINK_LOG(SINK, LEVEL) LOG_TO_OBJECT(SINK, LEVEL)

#define SINK_VLOG(SINK, LEVEL) VLOG_TO_OBJECT(SINK, LEVEL)



  /**
 * Logger wrapper for a spdlog logger.
 */
class Logger {
public:
  std::string levelString() const { return spdlog::level::level_names[logger_->level()]; }
  std::string name() const { return logger_->name(); }
  void setLevel(spdlog::level::level_enum level) const { logger_->set_level(level); }
  spdlog::logger &spdlog() const { return *logger_; }

private:
  Logger(const std::string& name);

  std::shared_ptr<spdlog::logger> logger_; // Use shared_ptr here to allow static construction
                                           // of constant vector below.
  friend class Registry;
};


/**
 * An optionally locking stderr logging sink.
 */
class LockingStderrSink : public spdlog::sinks::sink {
public:
  void setLock(Thread::BasicLockable& lock) { lock_ = &lock; }

  // spdlog::sinks::sink
  void log(const spdlog::details::log_msg& msg) override;
  void flush() override;

private:
  Thread::BasicLockable* lock_{};
};

#endif

/**
 * A registry of all named loggers in envoy. Usable for adjusting levels of each logger
 * individually.
 */
class Registry {
public:
  /**
   * @param id supplies the fixed ID of the logger to create.
   * @return spdlog::logger& a logger with system specified sinks for a given ID.
   */
  static Logger& getLog(Id id);

  /**
   * @return the singleton sink to use for all loggers.
   */
  static std::shared_ptr<LockingStderrSink> getSink() {
    static std::shared_ptr<LockingStderrSink> sink(new LockingStderrSink());
    return sink;
  }

  /**
   * Initialize the logging system from server options.
   */
  static void initialize(uint64_t log_level, Thread::BasicLockable& lock);

  /**
   * @return const std::vector<Logger>& the installed loggers.
   */
  static const std::vector<Logger>& loggers() { return all_loggers_; }

private:
  static std::vector<Logger> all_loggers_;
};

/**
 * Mixin class that allows any class to peform logging with a logger of a particular ID.
 */
template <Id id> class Loggable {
protected:
  /**
   * @return spdlog::logger& the static log instance to use for class local logging.
   */
  static Logger& log() {
    static Logger& instance = Registry::getLog(id);
    return instance;
  }
};

} // Logger

#ifdef NDEBUG
#define log_trace(...)
#define log_debug(...)
#else
#define log_trace(...) VLOG(2) << fmt::format(__VA_ARGS__)
#define log_debug(...) VLOG(1) << fmt::format(__VA_ARGS__)
#endif

/**
 * Convenience macros for logging with connection ID.
 */
#define conn_log(LOG_TYPE, LOG_OBJECT, LEVEL, FORMAT, CONNECTION, ...)                             \
  LOG_TYPE##_TO_OBJECT(LOG_OBJECT, LEVEL)                                                          \
      << fmt::format("[C{}] " FORMAT, (CONNECTION).id(), ##__VA_ARGS__)

#ifdef NDEBUG
#define conn_log_trace(...)
#define conn_log_debug(...)
#else
#define conn_log_trace(FORMAT, CONNECTION, ...)                                                    \
  conn_log(VLOG, log(), 2, FORMAT, CONNECTION, ##__VA_ARGS__)
#define conn_log_debug(FORMAT, CONNECTION, ...)                                                    \
  conn_log(VLOG, log(), 1, FORMAT, CONNECTION, ##__VA_ARGS__)
#endif

#define conn_log_info(FORMAT, CONNECTION, ...)                                                     \
  conn_log(LOG, log(), INFO, FORMAT, CONNECTION, ##__VA_ARGS__)

/**
 * Convenience macros for logging with a stream ID and a connection ID.
 */
#define stream_log(LOG_TYPE, LOG_OBJECT, LEVEL, FORMAT, STREAM, ...)                               \
  LOG_TYPE##_TO_OBJECT(LOG_OBJECT, LEVEL) << fmt::format(                                          \
      "[C{}][S{}] " FORMAT, (STREAM).connectionId(), (STREAM).streamId(), ##__VA_ARGS__)

#ifdef NDEBUG
#define stream_log_trace(...)
#define stream_log_debug(...)
#else
#define stream_log_trace(FORMAT, STREAM, ...)                                                      \
  stream_log(VLOG, log(), 2, FORMAT, STREAM, ##__VA_ARGS__)
#define stream_log_debug(FORMAT, STREAM, ...)                                                      \
  stream_log(VLOG, log(), 1, FORMAT, STREAM, ##__VA_ARGS__)
#endif

#define stream_log_info(FORMAT, STREAM, ...)                                                       \
  stream_log(LOG, log(), INFO, FORMAT, STREAM, ##__VA_ARGS__)

} // Envoy
