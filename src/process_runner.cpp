// *****************************************************
//    Copyright 2022 Videonetics Technology Pvt Ltd
// *****************************************************
#include "process_runner.h"
#include <Poco/Exception.h>
#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/Process.h>
#include <Poco/StreamCopier.h>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

constexpr int RETRY_TIMEOUT = 300;
ProcessRunner::ProcessRunner(std::string command) : ProcessRunner(std::move(command), std::vector<std::string>(), "") {}
ProcessRunner::ProcessRunner(std::string command, std::vector<std::string> args, std::string prologue_command,
                             std::vector<std::string> prologue_args, std::string epilogue_command,
                             std::vector<std::string> epilogue_args, std::string unique_id, std::string app_install_dir)
    : _command(std::move(command)), _args(std::move(args)), _prologue_command(std::move(prologue_command)),
      _prologue_args(std::move(prologue_args)), _epilogue_command(std::move(epilogue_command)),
      _epilogue_args(std::move(epilogue_args)), _app_install_dir(std::move(app_install_dir)),
      _unique_id(std::move(unique_id)) {
  _composite_command = _compose_composite_command(_command, _args, _unique_id, _app_install_dir);
  // _thread = std::make_unique<std::thread>(&ProcessRunner::run, this);
  _thread = std::make_unique<std::future<void>>(std::async(std::launch::async, &ProcessRunner::run, this));
}

ProcessRunner::ProcessRunner(std::string command, std::vector<std::string> args, std::string prologue_command,
                             std::vector<std::string> prologue_args, std::string unique_id, std::string app_install_dir)
    : ProcessRunner(std::move(command), std::move(args), std::move(prologue_command), std::move(prologue_args),
                    std::string(), std::vector<std::string>(), std::move(unique_id), std::move(app_install_dir))

{}
ProcessRunner::ProcessRunner(std::string command, std::vector<std::string> args, std::string unique_id)
    : ProcessRunner(std::move(command), std::move(args), std::string(), std::vector<std::string>(),
                    std::move(unique_id), std::string(".")) {}

ProcessRunner::~ProcessRunner() {
  _signal_to_stop();
  // if (_thread->joinable()) {
  //   _thread->join()
  // }
  if (_thread->wait_for(std::chrono::seconds(2)) == std::future_status::timeout) {
    spdlog::warn("Thread not yet terminated, forcing, for {}", _composite_command);
    if (_process_handle) {
      Poco::Process::kill(*_process_handle);
    }
  }
  _thread->wait();
}

void ProcessRunner::signal_to_stop() { _signal_to_stop(); }

void ProcessRunner::_signal_to_stop() {
  if (_is_already_shutting_down) {
    return;
  }
  _is_already_shutting_down = true;
  _do_shutdown              = true;
  const int id              = _get_id();
  if (id > 0) {
    Poco::Process::requestTermination(id);
  }
}

bool ProcessRunner::is_running() { return _get_id() > 0; }

int ProcessRunner::get_last_exit_code() {
  if (_get_id() > 0) {
    return _last_exit_code;
  }
  return -1;
}

int ProcessRunner::get_id() { return _get_id(); }

int ProcessRunner::_get_id() {
  std::unique_lock<std::mutex> lock_thread_running(_thread_running_mutex);
  _thread_running_cv.wait(lock_thread_running, [this] { return _is_thread_running; });

  if (_process_handle) {
    return _process_handle->id();
  }
  return -1;
}

std::string ProcessRunner::get_composite_command() { return _composite_command; }

void PrintLastExitStatus(int last_exit_code) {
  if (last_exit_code == 0) {
    spdlog::info("Process returned with:: {}", last_exit_code);
  } else {
    spdlog::error("Process returned with:: {}", last_exit_code);
  }
}

void ProcessRunner::run() {
  {
    const std::lock_guard<std::mutex> lock_thread_running(_thread_running_mutex);
    _is_thread_running = true;
  }
  _thread_running_cv.notify_all();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!_prologue_command.empty()) {
    spdlog::info("Executing prelogue in thread");
    run_once(_prologue_command, _prologue_args, _unique_id, _app_install_dir);
  }
  spdlog::info("Thread Started for {}", _composite_command);
  while (!_do_shutdown) {
    try {
      _process_handle = std::make_unique<Poco::ProcessHandle>(Poco::Process::launch(_command, _args, _app_install_dir));
    } catch (Poco::Exception& e) {
      spdlog::error("ProcessHandle Poco Exception: {}", e.what());
    } catch (const std::exception& e) {
      spdlog::error("ProcessHandle Exception:: {}", e.what());
    }
    if (_process_handle) {
      if (_process_handle->id() > 0) {
        _last_exit_code = 0;
        _last_exit_code = _process_handle->wait();
        PrintLastExitStatus((int)_last_exit_code);
      }
      _process_handle = nullptr;
    } else {
      // break; keep on trying
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_TIMEOUT));
  }
  if (!_epilogue_command.empty()) {
    spdlog::info("Executing epilogue in thread");
    run_once(_epilogue_command, _epilogue_args, _unique_id, _app_install_dir);
  }

  spdlog::info("Thread Stopped for {}", _composite_command);
}

int ProcessRunner::run_once(const std::string& command) { return run_once(command, std::vector<std::string>()); }
int ProcessRunner::run_once(const std::string& command, const std::vector<std::string>& args) {
  return run_once(command, args, "", "");
}
int ProcessRunner::run_once(const std::string& command, const std::vector<std::string>& args,
                            const std::string& unique_id, const std::string& app_install_dir) {
  std::unique_ptr<Poco::ProcessHandle> process_handle;
  int                                  last_exit_code = -1;
  spdlog::info("Starting process {}", _compose_composite_command(command, args, unique_id, app_install_dir));
  try {

    process_handle = std::make_unique<Poco::ProcessHandle>(Poco::Process::launch(command, args, app_install_dir));
  } catch (Poco::Exception& e) {
    spdlog::error("Poco::Exception {}", e.what());
  } catch (const std::exception& e) {
    spdlog::error("Std exception {}", e.what());
  }
  if (process_handle) {
    if (process_handle->id() > 0) {
      // last_exit_code = 0;
      last_exit_code = process_handle->wait();
      PrintLastExitStatus(last_exit_code);
    }
    process_handle = nullptr;
  } else {
    // break; keep on trying
  }
  return last_exit_code;
}

int ProcessRunner::run_once(const std::string& command, std::stringstream& ss) {
  return run_once(command, std::vector<std::string>(), ss);
}
int ProcessRunner::run_once(const std::string& command, const std::vector<std::string>& args, std::stringstream& ss) {
  return run_once(command, args, "", "", ss);
}
int ProcessRunner::run_once(const std::string& command, const std::vector<std::string>& args,
                            const std::string& unique_id, const std::string& app_install_dir, std::stringstream& ss) {
  std::unique_ptr<Poco::ProcessHandle> process_handle;
  int                                  last_exit_code = -1;
  spdlog::info("Starting process {}", _compose_composite_command(command, args, unique_id, app_install_dir));
  try {
    Poco::Pipe out_pipe;
    process_handle = std::make_unique<Poco::ProcessHandle>(
        Poco::Process::launch(command, args, app_install_dir, nullptr, &out_pipe, nullptr));
    Poco::PipeInputStream istr(out_pipe);
    Poco::StreamCopier::copyStream(istr, ss);
  } catch (Poco::Exception& e) {
    spdlog::error("Poco::Exception {}", e.what());
  } catch (const std::exception& e) {
    spdlog::error("Std exception {}", e.what());
  }
  if (process_handle) {
    if (process_handle->id() > 0) {
      // last_exit_code = 0;
      last_exit_code = process_handle->wait();
      PrintLastExitStatus(last_exit_code);
    }
    process_handle = nullptr;
  } else {
    // break; keep on trying
  }
  return last_exit_code;
}

std::string ProcessRunner::_compose_composite_command(const std::string& command, const std::vector<std::string>& args,
                                                      const std::string& unique_id,
                                                      const std::string& app_install_dir) {
  std::stringstream ss;
  ss << "[";
  ss << command;
  for (const auto& piece : args) {
    ss << " ";
    ss << piece;
  }
  ss << "] unique_id: " << unique_id;
  ss << " from: " << app_install_dir;
  return ss.str();
}
