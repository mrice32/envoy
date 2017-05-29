#include "glog/logging.h"

int main(int argc, char** argv) {

  google::nitGoogleLogging(argv[0]);
  LOG(ERROR) << "hey earth";
  return 0;
}
