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
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unistd.h>

namespace esphome {
namespace host {

static const char *const TAG = "host.shell";
extern char **environ;

static bool create_pipe(int fds[2]) {
#ifdef O_CLOEXEC
  if (pipe2(fds, O_CLOEXEC) == 0) {
    return true;
  }
#endif
  if (pipe(fds) != 0) {
    return false;
  }
  // Ensure the descriptors are close-on-exec even if pipe2 is unavailable.
  fcntl(fds[0], F_SETFD, FD_CLOEXEC);
  fcntl(fds[1], F_SETFD, FD_CLOEXEC);
  return true;
}

static std::string shell_quote(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted.push_back(c);
    }
  }
  quoted += "'";
  return quoted;
}

static std::string prepend_export_commands(const ShellCommandOptions &options, const std::string &command) {
  if (options.environment.empty()) {
    return command;
  }

  std::string export_prefix;
  export_prefix.reserve(command.size() + options.environment.size() * 16);
  for (const auto &kv : options.environment) {
    export_prefix += "export ";
    export_prefix += kv.first;
    export_prefix += "=";
    export_prefix += shell_quote(kv.second);
    export_prefix += ";\n";
  }
  export_prefix += command;
  return export_prefix;
}

ShellCommandResult execute_shell_command(const std::string &command, const ShellCommandOptions &options) {
  ShellCommandResult result{};

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
    std::string shell = options.shell.empty() ? "/bin/sh" : options.shell;
    for (const auto &kv : options.environment) {
      if (setenv(kv.first.c_str(), kv.second.c_str(), 1) != 0) {
        ESP_LOGW(TAG, "Failed to set environment variable %s: errno=%d", kv.first.c_str(), errno);
      }
    }

    ESP_LOGD(TAG, "Executing command with shell '%s' and %zu custom env vars: %s", shell.c_str(),
             options.environment.size(), command.c_str());

    if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1 || dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
      _exit(127);
    }
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    std::string command_with_exports = prepend_export_commands(options, command);
    const char *argv[] = {shell.c_str(), "-c", command_with_exports.c_str(), nullptr};
    execve(shell.c_str(), const_cast<char *const *>(argv), ::environ);
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
  ESP_LOGD(TAG, "Command stderr:\n%s", result.stderr_output.c_str());
  ESP_LOGD(TAG, "Command finished with exit code %d", result.exit_code);

  return result;
}

}  // namespace host
}  // namespace esphome

#endif  // USE_HOST
