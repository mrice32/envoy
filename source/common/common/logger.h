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
#endif

namespace Envoy {

#ifndef GLOG_ON
constexpr int SPD_ONLY_TRACE = 1;
constexpr int SPD_ONLY_DEBUG = 2;
constexpr int INFO = 3;
constexpr int WARNING = 4;
constexpr int ERROR = 5;
constexpr int FATAL = 6;
#endif

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
#define DLOG_TO_OBJECT(LOG_OBJECT, LEVEL) DLOG(LEVEL)

#define VLOG_TO_OBJECT(LOG, LEVEL) VLOG(LEVEL)
#define DVLOG_TO_OBJECT(LOG_OBJECT, LEVEL) DVLOG(LEVEL)

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

class NullStream {
public:
  template <typename T> NullStream& operator<<(T&& t) {
    UNREFERENCED_PARAMETER(t);
    return *this;
  }
};

#define LOG_TO_OBJECT(LOG_OBJECT, LEVEL) Logger::SpdLogStream(LOG_OBJECT, LEVEL)

#define VLOG_TO_OBJECT(LOG_OBJECT, LEVEL)                                                          \
  Logger::SpdLogStream(LOG_OBJECT, (LEVEL == 1) ? SPD_ONLY_DEBUG : SPD_ONLY_TRACE)

#define LOG(LEVEL) LOG_TO_OBJECT(log(), LEVEL)
#define VLOG(LEVEL) VLOG_TO_OBJECT(log(), LEVEL)

#ifdef NDEBUG

// Send debug LOG calls to the NullStream when not compiled in debug mode.
#define DLOG_TO_OBJECT(LOG_OBJECT, LEVEL) Logger::NullStream()
#define DVLOG_TO_OBJECT(LOG_OBJECT, LEVEL) Logger::NullStream()

#else

// Send debug LOG calls to the non-debug LOG calls when compiled in debug mode.
#define DLOG_TO_OBJECT(LOG_OBJECT, LEVEL) LOG_TO_OBJECT(LOG_OBJECT, LEVEL)
#define DVLOG_TO_OBJECT(LOG_OBJECT, LEVEL) VLOG_TO_OBJECT(LOG_OBJECT, LEVEL)

#endif

#define DLOG(LEVEL) DLOG_TO_OBJECT(log(), LEVEL)
#define DVLOG(LEVEL) DVLOG_TO_OBJECT(log(), LEVEL)

#endif

/**
 * Logger wrapper for a spdlog logger.
 */
class Logger {
public:
  std::string levelString() const { return spdlog::level::level_names[logger_->level()]; }
  std::string name() const { return logger_->name(); }
  void setLevel(spdlog::level::level_enum level) const { logger_->set_level(level); }

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
  static spdlog::logger& getLog(Id id);

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
  static spdlog::logger& log() {
    static spdlog::logger& instance = Registry::getLog(id);
    return instance;
  }
};

} // Logger

// Function for formatting a connection log.
template <typename T, typename... Args>
std::string format_connection_log(const std::string& format, T& connection, Args&&... args) {
  return fmt::format("[C{}] " + format, connection.id(), std::forward<Args>(args)...);
}

// Function for formatting a stream log.
template <typename T, typename... Args>
std::string format_stream_log(const std::string& format, T& stream, Args&&... args) {
  return fmt::format("[C{}][S{}] " + format, stream.connectionId(), stream.streamId(),
                     std::forward<Args>(args)...);
}
} // Envoy
