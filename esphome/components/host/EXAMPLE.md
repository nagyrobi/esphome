# Host shell command example

This configuration demonstrates running host shell commands from lambdas, publishing outputs directly to template entities, and automatically updating a template sensor with the 1-minute load average from `/proc/loadavg`.

```yaml
esphome:
  name: host-shell-example
  platform: host

text_sensor:
  - platform: template
    id: last_stdout
    name: "Last Command Stdout"
  - platform: template
    id: last_stderr
    name: "Last Command Stderr"
sensor:
  - platform: template
    id: host_load_1m
    name: "Host Load (1m)"
    update_interval: 30s
    lambda: |-
      auto result = esphome::host::execute_shell_command("awk '{print $1}' /proc/loadavg");
      // Publish any stderr for visibility
      if (!result.stderr_output.empty()) {
        id(last_stderr).publish_state(result.stderr_output);
      }
      // Strip whitespace/newlines so parsing succeeds
      auto load_str = result.stdout_output;
      load_str.erase(std::remove_if(load_str.begin(), load_str.end(), ::isspace), load_str.end());
      return parse_number<float>(load_str);
  - platform: template
    id: last_exit_code
    name: "Last Command Exit Code"

button:
  - platform: template
    name: "Turn Off Screen (bash, custom env)"
    on_press:
      - lambda: |-
          esphome::host::ShellCommandOptions opts;
          opts.shell = "/bin/bash";
          opts.environment = {
            {"DISPLAY", ":0.0"},
            {"XAUTHORITY", "/home/user/.Xauthority"},
          };
          auto result = esphome::host::execute_shell_command("xset dpms force off", opts);
          id(last_exit_code).publish_state(result.exit_code);
          id(last_stdout).publish_state(result.stdout_output);
          id(last_stderr).publish_state(result.stderr_output);

  - platform: template
    name: "Restart Host (default sh)"
    on_press:
      - lambda: |-
          auto result = esphome::host::execute_shell_command("shutdown -r now");
          id(last_exit_code).publish_state(result.exit_code);
          id(last_stdout).publish_state(result.stdout_output);
          id(last_stderr).publish_state(result.stderr_output);

  - platform: template
    name: "Run Arbitrary Command"
    on_press:
      - lambda: |-
          auto result = esphome::host::execute_shell_command(id(arbitrary_command).state.c_str());
          id(last_exit_code).publish_state(result.exit_code);
          id(last_stdout).publish_state(result.stdout_output);
          id(last_stderr).publish_state(result.stderr_output);

text:
  - platform: template
    name: "Command"
    id: arbitrary_command
    optimistic: true
    min_length: 0
    max_length: 100
    mode: text
```
