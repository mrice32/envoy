#include "glog/logging.h"

int main(int argc, char** argv) {
  (void) argc;
  google::InitGoogleLogging(argv[0]);
  LOG(ERROR) << "hey earth";
  return 0;
}
