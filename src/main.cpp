#include <M5Unified.h>
#include <M5Utility.h>
#include <vector>

#include "BasicInterpreter.h"
#include "KeyboardUtils.h"

namespace {

constexpr int kTextSize = 2;
constexpr int kLineHeight = 16;
constexpr int kVisibleProgramLines = 11;
constexpr uint32_t kCursorBlinkIntervalMs = 500;

std::vector<String> program;
String input;
size_t input_cursor = 0;
String status_message;
BasicInterpreter interpreter;
bool output_mode = false;
bool cursor_visible = true;
uint32_t last_cursor_blink_ms = 0;

void draw_input_line() {
  const int input_y = M5.Display.height() - kLineHeight * 2;
  char prefix[16];
  snprintf(prefix, sizeof(prefix), "%03u> ",
           static_cast<unsigned>(program.size() + 1));

  M5.Display.setTextSize(kTextSize);
  const int cursor_width = 3;
  const int available_width =
      M5.Display.width() - M5.Display.textWidth(prefix) - cursor_width;
  size_t visible_start = 0;
  while (visible_start < input_cursor &&
         M5.Display.textWidth(input.substring(visible_start, input_cursor)) >
             available_width) {
    ++visible_start;
  }
  size_t visible_end = input.length();
  while (visible_end > input_cursor &&
         M5.Display.textWidth(input.substring(visible_start, visible_end)) >
             available_width) {
    --visible_end;
  }
  const String visible_input = input.substring(visible_start, visible_end);
  const String before_cursor =
      input.substring(visible_start, input_cursor);

  M5.Display.fillRect(0, input_y, M5.Display.width(), kLineHeight, TFT_BLACK);
  M5.Display.setCursor(0, input_y);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.print(prefix);
  M5.Display.print(visible_input);
  if (cursor_visible) {
    const int cursor_x =
        M5.Display.textWidth(prefix) + M5.Display.textWidth(before_cursor);
    M5.Display.fillRect(cursor_x, input_y + 2, cursor_width, kLineHeight - 4,
                        TFT_CYAN);
  }
}

void reset_cursor_blink() {
  cursor_visible = true;
  last_cursor_blink_ms = millis();
}

void draw_editor() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextSize(kTextSize);
  M5.Display.setCursor(0, 0);
  M5.Display.println("HAND-BASIC v0.1");
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

  size_t first = program.size() > kVisibleProgramLines
                     ? program.size() - kVisibleProgramLines
                     : 0;
  const size_t end =
      min(program.size(), first + static_cast<size_t>(kVisibleProgramLines));
  for (size_t i = first; i < end; ++i) {
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    char line_prefix[16];
    snprintf(line_prefix, sizeof(line_prefix), "%03u ",
             static_cast<unsigned>(i + 1));
    String visible_line = String(line_prefix) + program[i];
    while (!visible_line.isEmpty() &&
           M5.Display.textWidth(visible_line) > M5.Display.width()) {
      visible_line.remove(visible_line.length() - 1);
    }
    M5.Display.println(visible_line);
  }

  const int input_y = M5.Display.height() - kLineHeight * 2;
  M5.Display.fillRect(0, input_y, M5.Display.width(), kLineHeight * 2,
                      TFT_BLACK);
  reset_cursor_blink();
  draw_input_line();
  M5.Display.setCursor(0, input_y + kLineHeight);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.print(status_message);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

void submit_input() {
  String command = input;
  command.trim();

  input = "";
  input_cursor = 0;
  status_message = "";
  if (command.isEmpty()) {
    draw_editor();
    return;
  }

  String upper = command;
  upper.toUpperCase();
  if (upper == "RUN") {
    output_mode = true;
    interpreter.run(program);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, M5.Display.height() - 8);
    M5.Display.print("PRESS A KEY TO EDIT");
    return;
  }
  if (upper == "NEW") {
    program.clear();
    status_message = "NEW PROGRAM";
    draw_editor();
    return;
  }
  program.push_back(command);
  draw_editor();
}

void handle_key(char key) {
  if (output_mode) {
    output_mode = false;
    draw_editor();
    return;
  }
  if (is_cursor_left_key(key)) {
    if (input_cursor > 0) {
      --input_cursor;
    }
    reset_cursor_blink();
    draw_input_line();
    return;
  }
  if (is_cursor_right_key(key)) {
    if (input_cursor < input.length()) {
      ++input_cursor;
    }
    reset_cursor_blink();
    draw_input_line();
    return;
  }
  if (key == '\r' || key == '\n') {
    submit_input();
    return;
  }
  if (key == '\b' || key == 0x7F) {
    if (input_cursor > 0) {
      input.remove(input_cursor - 1, 1);
      --input_cursor;
    }
    draw_editor();
    return;
  }
  if (key >= 0x20 && key <= 0x7E) {
    input = input.substring(0, input_cursor) + String(key) +
            input.substring(input_cursor);
    ++input_cursor;
    draw_editor();
  }
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);

  if (!setup_keyboard_uart()) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("KEYBOARD UART ERROR");
    M5.Display.println("Fn+Sym+2, RESET KB");
    while (true) {
      m5::utility::delay(10000);
    }
  }
  status_message = "READY";
  draw_editor();
}

void loop() {
  M5.update();
  update_unit();
  if (is_keyboard_updated()) {
    char key{};
    while (read_key(key)) {
      handle_key(key);
    }
  }

  const uint32_t now = millis();
  if (!output_mode &&
      now - last_cursor_blink_ms >= kCursorBlinkIntervalMs) {
    cursor_visible = !cursor_visible;
    last_cursor_blink_ms = now;
    draw_input_line();
  }
}
