// *****************************************************
//  Copyright 2024 Videonetics Technology Pvt Ltd
// *****************************************************

#include "process_runner.h"
#include <iostream>
// #include "sinks/rotating_sqllite_sink.h"
// #include <spdlog/sinks/rotating_file_sink.h>

int main(/*int argc, char const* argv[]*/) {
  std::vector<std::string> args;
  ProcessRunner            ps("", args, "22");
  return 0;
}
