#ifdef USE_HOST

#if !(defined(__linux__) || defined(__APPLE__))
#error This Host shell command implementation is not supported on this host OS
#endif

#include "shell_command.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <map>
#include <string>
#include <cstring>
#include <string_view>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unistd.h>

#ifndef USE_ESPHOME_HOST_ALLOW_SHELL_COMMANDS
#define USE_ESPHOME_HOST_ALLOW_SHELL_COMMANDS 0
#endif

extern char **environ;  // Declare the global variable

namespace esphome::host {

static const char *const TAG = "host.shell";

static bool create_pipe(int fds[2]) {
  if (pipe(fds) != 0) {
    return false;
  }
  // Make the descriptors close-on-exec
  fcntl(fds[0], F_SETFD, FD_CLOEXEC);
  fcntl(fds[1], F_SETFD, FD_CLOEXEC);
  return true;
}

static std::map<std::string, std::string> current_environment() {
  std::map<std::string, std::string> env_map;
  for (char **env = environ; env != nullptr && *env != nullptr; ++env) {
    const std::string entry(*env);
    auto separator = entry.find('=');
    if (separator == std::string::npos) {
      continue;
    }
    env_map[entry.substr(0, separator)] = entry.substr(separator + 1);
  }
  return env_map;
}

static std::string describe_command(const std::vector<std::string> &command) {
  std::string description;
  for (const auto &arg : command) {
    if (!description.empty()) {
      description.append(" ");
    }
    description.append(arg);
  }
  return description;
}

static std::vector<char *> build_envp(const std::map<std::string, std::string> &env_map) {
  std::vector<char *> envp;
  envp.reserve(env_map.size() + 1);
  for (const auto &[k, v] : env_map) {
    envp.push_back(strdup((k + "=" + v).c_str()));
  }
  envp.push_back(nullptr);
  return envp;
}

static void execve_with_path_search(const std::vector<std::string> &command, const std::vector<char *> &envp,
                                    const std::string &path_env) {
  std::vector<char *> argv;
  argv.reserve(command.size() + 1);
  for (const auto &arg : command) {
    argv.push_back(const_cast<char *>(arg.c_str()));
  }
  argv.push_back(nullptr);

  const std::string &binary = command.front();
  if (binary.find('/') != std::string::npos || path_env.empty()) {
    execve(binary.c_str(), argv.data(), envp.data());
    return;
  }

  int last_errno = ENOENT;
  size_t start = 0;
  while (true) {
    size_t end = path_env.find(':', start);
    std::string_view segment(path_env.c_str() + start,
                             (end == std::string::npos ? path_env.size() : end) - start);
    std::string candidate;
    if (segment.empty()) {
      candidate = binary;
    } else {
      candidate.reserve(segment.size() + 1 + binary.size());
      candidate.append(segment);
      candidate.push_back('/');
      candidate.append(binary);
    }
    execve(candidate.c_str(), argv.data(), envp.data());
    if (errno != ENOENT) {
      last_errno = errno;
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }

  errno = last_errno;
  execve(binary.c_str(), argv.data(), envp.data());
}

static ShellCommandResult execute_command_with_arguments(const std::vector<std::string> &command,
                                                         const ShellCommandOptions &options,
                                                         const std::string &log_command) {
  ShellCommandResult result{};
  if (command.empty()) {
    ESP_LOGE(TAG, "Cannot execute an empty command");
    return result;
  }

  int stdout_pipe[2];
  int stderr_pipe[2];
  if (!create_pipe(stdout_pipe) || !create_pipe(stderr_pipe)) {
    ESP_LOGE(TAG, "Creating pipes failed: errno=%d", errno);
    return result;
  }

  pid_t pid = fork();
  if (pid == -1) {
    ESP_LOGE(TAG, "Fork failed: errno=%d", errno);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    return result;
  }

  if (pid == 0) {
    auto env_map = current_environment();
    for (const auto &kv : options.environment) {
      env_map[kv.first] = kv.second;
    }

    std::string env_log;
    for (const auto &kv : options.environment) {
      if (!env_log.empty()) {
        env_log.append(", ");
      }
      env_log.append(kv.first);
      env_log.append("=");
      env_log.append(kv.second);
    }

    auto envp = build_envp(env_map);

    ESP_LOGD(TAG, "Executing command '%s' with %zu custom env vars", log_command.c_str(),
             options.environment.size());
    if (!env_log.empty()) {
      ESP_LOGD(TAG, "Custom environment variables from YAML: %s", env_log.c_str());
    }

    if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1 || dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
      _exit(127);
    }
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);

    std::string path_env;
    auto path_it = env_map.find("PATH");
    if (path_it != env_map.end()) {
      path_env = path_it->second;
    }
    execve_with_path_search(command, envp, path_env);
    _exit(127);
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  bool stdout_open = true;
  bool stderr_open = true;
  std::array<char, 4096> buffer{};

  while (stdout_open || stderr_open) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = -1;
    if (stdout_open) {
      FD_SET(stdout_pipe[0], &read_fds);
      max_fd = std::max(max_fd, stdout_pipe[0]);
    }
    if (stderr_open) {
      FD_SET(stderr_pipe[0], &read_fds);
      max_fd = std::max(max_fd, stderr_pipe[0]);
    }

    int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
    if (ready == -1) {
      if (errno == EINTR) {
        continue;
      }
      ESP_LOGW(TAG, "select() failed: errno=%d", errno);
      break;
    }

    if (stdout_open && FD_ISSET(stdout_pipe[0], &read_fds)) {
      ssize_t count = ::read(stdout_pipe[0], buffer.data(), buffer.size());
      if (count > 0) {
        result.stdout_output.append(buffer.data(), static_cast<size_t>(count));
      } else if (count == 0) {
        stdout_open = false;
      } else if (errno != EINTR) {
        ESP_LOGW(TAG, "Reading stdout failed: errno=%d", errno);
        stdout_open = false;
      }
    }

    if (stderr_open && FD_ISSET(stderr_pipe[0], &read_fds)) {
      ssize_t count = ::read(stderr_pipe[0], buffer.data(), buffer.size());
      if (count > 0) {
        result.stderr_output.append(buffer.data(), static_cast<size_t>(count));
      } else if (count == 0) {
        stderr_open = false;
      } else if (errno != EINTR) {
        ESP_LOGW(TAG, "Reading stderr failed: errno=%d", errno);
        stderr_open = false;
      }
    }
  }

  close(stdout_pipe[0]);
  close(stderr_pipe[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) == -1) {
    ESP_LOGE(TAG, "waitpid failed: errno=%d", errno);
    result.exit_code = -1;
    return result;
  }

  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  } else {
    result.exit_code = -1;
  }

  ESP_LOGD(TAG, "Command stdout:\n%s", result.stdout_output.c_str());
  if (!result.stderr_output.empty()) {
    ESP_LOGE(TAG, "Command stderr:\n%s", result.stderr_output.c_str());
  }
  ESP_LOGD(TAG, "Command finished with exit code %d", result.exit_code);

  return result;
}

ShellCommandResult execute_command(const std::vector<std::string> &command, const ShellCommandOptions &options) {
  return execute_command_with_arguments(command, options, describe_command(command));
}

ShellCommandResult execute_shell_command(const std::string &command, const ShellCommandOptions &options) {
#if !USE_ESPHOME_HOST_ALLOW_SHELL_COMMANDS
  ESP_LOGE(TAG,
           "Shell command execution is disabled. Enable allow_shell_commands in the host "
           "configuration to use execute_shell_command.");
  return {};
#else
  std::string shell = options.shell.empty() ? "/bin/sh" : options.shell;
  std::vector<std::string> args{shell, "-c", command};
  return execute_command_with_arguments(args, options, shell + " -c " + command);
#endif
}

}  // namespace esphome::host

#endif  // USE_HOST
