// *****************************************************
//  Copyright 2024 Videonetics Technology Pvt Ltd
// *****************************************************

#include "process_runner.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
// #include "sinks/rotating_sqllite_sink.h"
// #include <spdlog/sinks/rotating_file_sink.h>

int main(/*int argc, char const* argv[]*/) {

  {
    std::stringstream ss;
    ProcessRunner     ps("ls");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    ProcessRunner::run_once("clinfo", ss);
    if (ss.str().find("CMake") != std::string::npos) {
      std::cout << "Received: " << ss.str() << '\n';
    }
  }
  return 0;
}
