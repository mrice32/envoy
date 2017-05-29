#include "glog/logging.h"

#include <iostream>

class Logger : google::LogSink {
  void send(LogSeverity severity, const char* full_filename,
                    const char* base_filename, int line,
                    const struct ::tm* tm_time,
                    const char* message, size_t message_len) override {
    std::cout << "sink call" << std::endl;
    std::cout << ToString(severity, file, line, tm_time, message, message_len) << std::endl;
  }
}


int main(int argc, char** argv) {
  (void) argc;
  google::InitGoogleLogging(argv[0]);
  Logger logger;
    LOG_TO_SINK_BUT_NOT_TO_LOGFILE(logger, ERROR) << "hello world";
  LOG(ERROR) << "hey earth";
  return 0;
}
