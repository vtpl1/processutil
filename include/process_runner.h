// *****************************************************
//  Copyright 2024 Videonetics Technology Pvt Ltd
// *****************************************************

#pragma once
#ifndef process_runner_h
#define process_runner_h
#include <Poco/Process.h>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

class ProcessRunner {
private:
  std::string              _command;
  std::vector<std::string> _args;
  std::string              _prologue_command;
  std::vector<std::string> _prologue_args;
  std::string              _epilogue_command;
  std::vector<std::string> _epilogue_args;

  std::atomic_int _last_exit_code{-1};
  std::string     _composite_command;
  std::string     _unique_id;
  std::string     _app_install_dir{"."};

  // std::unique_ptr<std::thread> _thread;
  std::unique_ptr<std::future<void>> _thread;
  std::atomic_bool                   _do_shutdown{false};

  std::unique_ptr<Poco::ProcessHandle> _process_handle;
  std::mutex                           _thread_running_mutex;
  std::condition_variable              _thread_running_cv;
  bool                                 _is_thread_running{false};
  bool                                 _is_already_shutting_down{false};

  int                _get_id();
  void               run();
  void               _signal_to_stop();
  static std::string _compose_composite_command(const std::string& command, const std::vector<std::string>& args,
                                                const std::string& unique_id, const std::string& app_install_dir);

public:
  ProcessRunner(std::string command, std::vector<std::string> args, std::string unique_id);
  ProcessRunner(std::string command, std::vector<std::string> args, std::string prologue_command,
                std::vector<std::string> prologue_args, std::string unique_id = "", std::string app_install_dir = ".");
  ProcessRunner(std::string command, std::vector<std::string> args, std::string prologue_command,
                std::vector<std::string> prologue_args, std::string epilogue_command,
                std::vector<std::string> epilogue_args, std::string unique_id = "", std::string app_install_dir = ".");
  static int run_once(const std::string& command, const std::vector<std::string>& args,
                      const std::string& unique_id = "", const std::string& app_install_dir = ".");
  virtual ~ProcessRunner();
  void        signal_to_stop();
  bool        is_running();
  int         get_last_exit_code();
  int         get_id();
  std::string get_composite_command();
};
#endif // process_runner_h
