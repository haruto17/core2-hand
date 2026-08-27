#include <M5Unified.h>
#include <M5Utility.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>

#include <vector>

#include "BasicInterpreter.h"
#include "KeyboardUtils.h"

namespace {

constexpr uint8_t kSdChipSelect = 4;
constexpr int kTextSize = 2;
constexpr int kLineHeight = 16;
constexpr int kVisibleProgramLines = 11;
constexpr int kVisibleSavedSlots = 12;
constexpr uint32_t kCursorBlinkIntervalMs = 500;
constexpr size_t kNoLine = static_cast<size_t>(-1);
constexpr char kStorageNamespace[] = "hand-basic";
constexpr char kProgramKey[] = "program";

struct SavedSlot {
  int number;
  String first_line;
};

std::vector<String> program;
std::vector<SavedSlot> saved_slots;
String input;
size_t input_cursor = 0;
String input_before_edit;
size_t cursor_before_edit = 0;
size_t selected_line = kNoLine;
size_t editing_line = kNoLine;
String status_message;
BasicInterpreter interpreter;
Preferences preferences;
bool output_mode = false;
bool files_mode = false;
bool sd_ready = false;
bool cursor_visible = true;
uint32_t last_cursor_blink_ms = 0;
size_t saved_slots_scroll = 0;

String serialize_program() {
  String serialized;
  for (size_t i = 0; i < program.size(); ++i) {
    if (i != 0) {
      serialized += '\n';
    }
    serialized += program[i];
  }
  return serialized;
}

std::vector<String> parse_program(const String& serialized) {
  std::vector<String> lines;
  size_t start = 0;
  while (start < serialized.length()) {
    const int newline = serialized.indexOf('\n', start);
    String line = newline < 0 ? serialized.substring(start)
                              : serialized.substring(start, newline);
    if (line.endsWith("\r")) {
      line.remove(line.length() - 1);
    }
    if (!line.isEmpty()) {
      lines.push_back(line);
    }
    if (newline < 0) {
      break;
    }
    start = newline + 1;
  }
  return lines;
}

void persist_current_program() {
  preferences.putString(kProgramKey, serialize_program());
}

void restore_current_program() {
  program = parse_program(preferences.getString(kProgramKey, ""));
}

void draw_input_line() {
  const int input_y = M5.Display.height() - kLineHeight * 2;
  char prefix[16];
  const size_t displayed_line =
      editing_line == kNoLine ? program.size() : editing_line;
  snprintf(prefix, sizeof(prefix), "%03u%c ",
           static_cast<unsigned>(displayed_line + 1),
           editing_line == kNoLine ? '>' : '*');

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

  const size_t focused_line =
      editing_line == kNoLine ? selected_line : editing_line;
  size_t first = program.size() > kVisibleProgramLines
                     ? program.size() - kVisibleProgramLines
                     : 0;
  if (focused_line != kNoLine) {
    first = focused_line >= static_cast<size_t>(kVisibleProgramLines)
                ? focused_line - kVisibleProgramLines + 1
                : 0;
  }
  const size_t end =
      min(program.size(), first + static_cast<size_t>(kVisibleProgramLines));
  for (size_t i = first; i < end; ++i) {
    const int line_y = M5.Display.getCursorY();
    const bool focused = i == focused_line;
    if (focused) {
      M5.Display.fillRect(0, line_y, M5.Display.width(), kLineHeight, TFT_NAVY);
      M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
    } else {
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }
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

bool parse_slot(const String& command, const char* name, int& slot) {
  String upper = command;
  upper.toUpperCase();
  const size_t name_length = strlen(name);
  if (!(upper == name ||
        (upper.startsWith(name) && upper.length() > name_length &&
         isSpace(upper[name_length])))) {
    return false;
  }
  String argument = command.substring(name_length);
  argument.trim();
  if (argument.isEmpty()) {
    slot = 0;
    return true;
  }
  char* end = nullptr;
  const long parsed = strtol(argument.c_str(), &end, 10);
  slot = (*end == '\0' && parsed >= 1 && parsed <= 100)
             ? static_cast<int>(parsed)
             : 0;
  return true;
}

String slot_path(int slot) { return "/hand-basic/" + String(slot) + ".txt"; }

void save_program(int slot) {
  if (slot == 0) {
    return;
  }
  if (!sd_ready) {
    status_message = "SD NOT READY";
    return;
  }
  if (!SD.exists("/hand-basic") && !SD.mkdir("/hand-basic")) {
    status_message = "SAVE ERROR";
    return;
  }
  const String path = slot_path(slot);
  if (SD.exists(path)) {
    SD.remove(path);
  }
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    status_message = "SAVE ERROR";
    return;
  }
  for (const auto& line : program) {
    file.println(line);
  }
  const bool success = file.getWriteError() == 0;
  file.close();
  status_message = success ? "SAVED " + String(slot) : "SAVE ERROR";
}

void load_program(int slot) {
  if (slot == 0 || !sd_ready) {
    return;
  }
  File file = SD.open(slot_path(slot), FILE_READ);
  if (!file) {
    return;
  }
  String serialized;
  while (file.available()) {
    serialized += static_cast<char>(file.read());
  }
  file.close();
  program = parse_program(serialized);
  persist_current_program();
  status_message = "LOADED " + String(slot);
}

void draw_saved_slots() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(kTextSize);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.printf("FILES (%u)\n", static_cast<unsigned>(saved_slots.size()));

  if (!sd_ready) {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.println("SD NOT READY");
  } else if (saved_slots.empty()) {
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.println("NO SAVED PROGRAMS");
  } else {
    const size_t end = min(
        saved_slots.size(),
        saved_slots_scroll + static_cast<size_t>(kVisibleSavedSlots));
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    for (size_t i = saved_slots_scroll; i < end; ++i) {
      char prefix[8];
      snprintf(prefix, sizeof(prefix), "%03d ", saved_slots[i].number);
      String visible_line = String(prefix) + saved_slots[i].first_line;
      while (!visible_line.isEmpty() &&
             M5.Display.textWidth(visible_line) > M5.Display.width()) {
        visible_line.remove(visible_line.length() - 1);
      }
      M5.Display.println(visible_line);
    }
  }

  M5.Display.fillRect(0, M5.Display.height() - 9, M5.Display.width(), 9,
                      TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(0, M5.Display.height() - 8);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.print("Fn+D/X SCROLL  ENTER/ESC EXIT");
}

void open_saved_slots() {
  saved_slots.clear();
  saved_slots_scroll = 0;
  if (sd_ready) {
    for (int slot = 1; slot <= 100; ++slot) {
      const String path = slot_path(slot);
      if (!SD.exists(path)) {
        continue;
      }
      File file = SD.open(path, FILE_READ);
      if (!file) {
        continue;
      }
      String first_line;
      while (file.available() && first_line.length() < 128) {
        const char character = static_cast<char>(file.read());
        if (character == '\r' || character == '\n') {
          break;
        }
        first_line += character;
      }
      file.close();
      first_line.trim();
      if (first_line.isEmpty()) {
        first_line = "(EMPTY)";
      }
      saved_slots.push_back({slot, first_line});
    }
  }
  files_mode = true;
  draw_saved_slots();
}

void submit_input() {
  String command = input;
  command.trim();

  if (editing_line != kNoLine) {
    if (command.isEmpty()) {
      status_message = "LINE CANNOT BE EMPTY";
      draw_editor();
      return;
    }
    const size_t updated_line = editing_line;
    program[updated_line] = command;
    persist_current_program();
    editing_line = kNoLine;
    selected_line = kNoLine;
    input = input_before_edit;
    input_cursor = min(cursor_before_edit, input.length());
    input_before_edit = "";
    status_message = "UPDATED " + String(updated_line + 1);
    draw_editor();
    return;
  }

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
    persist_current_program();
    status_message = "NEW PROGRAM";
    draw_editor();
    return;
  }
  if (upper == "FILES") {
    open_saved_slots();
    return;
  }

  int slot{};
  if (parse_slot(command, "SAVE", slot)) {
    save_program(slot);
    draw_editor();
    return;
  }
  if (parse_slot(command, "LOAD", slot)) {
    load_program(slot);
    draw_editor();
    return;
  }

  program.push_back(command);
  persist_current_program();
  draw_editor();
}

void select_previous_line() {
  if (editing_line != kNoLine || program.empty()) {
    return;
  }
  if (selected_line == kNoLine) {
    selected_line = program.size() - 1;
  } else if (selected_line > 0) {
    --selected_line;
  }
  status_message = "ENTER TO EDIT " + String(selected_line + 1);
  draw_editor();
}

void select_next_line() {
  if (editing_line != kNoLine || program.empty()) {
    return;
  }
  if (selected_line == kNoLine) {
    selected_line = 0;
  } else if (selected_line + 1 < program.size()) {
    ++selected_line;
  }
  status_message = "ENTER TO EDIT " + String(selected_line + 1);
  draw_editor();
}

void begin_selected_line_edit() {
  if (selected_line == kNoLine) {
    return;
  }
  input_before_edit = input;
  cursor_before_edit = input_cursor;
  editing_line = selected_line;
  selected_line = kNoLine;
  input = program[editing_line];
  input_cursor = input.length();
  status_message = "ESC TO CANCEL";
  draw_editor();
}

void cancel_line_navigation() {
  if (editing_line != kNoLine) {
    editing_line = kNoLine;
    input = input_before_edit;
    input_cursor = min(cursor_before_edit, input.length());
    input_before_edit = "";
    status_message = "EDIT CANCELLED";
  } else if (selected_line != kNoLine) {
    selected_line = kNoLine;
    status_message = "";
  } else {
    return;
  }
  draw_editor();
}

void leave_line_selection() {
  if (selected_line != kNoLine) {
    selected_line = kNoLine;
    status_message = "";
  }
}

void handle_key(char key) {
  if (files_mode) {
    if (is_cursor_up_key(key)) {
      if (saved_slots_scroll > 0) {
        --saved_slots_scroll;
        draw_saved_slots();
      }
      return;
    }
    if (is_cursor_down_key(key)) {
      if (saved_slots_scroll + kVisibleSavedSlots < saved_slots.size()) {
        ++saved_slots_scroll;
        draw_saved_slots();
      }
      return;
    }
    if (is_escape_key(key) || key == '\r' || key == '\n') {
      files_mode = false;
      status_message = "FILES CLOSED";
      draw_editor();
    }
    return;
  }
  if (output_mode) {
    output_mode = false;
    draw_editor();
    return;
  }
  if (is_cursor_up_key(key)) {
    select_previous_line();
    return;
  }
  if (is_cursor_down_key(key)) {
    select_next_line();
    return;
  }
  if (is_escape_key(key)) {
    cancel_line_navigation();
    return;
  }
  if (is_cursor_left_key(key)) {
    const bool had_selection = selected_line != kNoLine;
    leave_line_selection();
    if (input_cursor > 0) {
      --input_cursor;
    }
    reset_cursor_blink();
    if (had_selection) {
      draw_editor();
    } else {
      draw_input_line();
    }
    return;
  }
  if (is_cursor_right_key(key)) {
    const bool had_selection = selected_line != kNoLine;
    leave_line_selection();
    if (input_cursor < input.length()) {
      ++input_cursor;
    }
    reset_cursor_blink();
    if (had_selection) {
      draw_editor();
    } else {
      draw_input_line();
    }
    return;
  }
  if (key == '\r' || key == '\n') {
    if (selected_line != kNoLine) {
      begin_selected_line_edit();
      return;
    }
    submit_input();
    return;
  }
  if (key == '\b' || key == 0x7F) {
    leave_line_selection();
    if (input_cursor > 0) {
      input.remove(input_cursor - 1, 1);
      --input_cursor;
    }
    draw_editor();
    return;
  }
  if (key >= 0x20 && key <= 0x7E) {
    leave_line_selection();
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

  preferences.begin(kStorageNamespace, false);
  restore_current_program();
  sd_ready = SD.begin(kSdChipSelect, SPI, 25000000);

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
  status_message = sd_ready ? "READY" : "READY (NO SD)";
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
  if (!output_mode && !files_mode &&
      now - last_cursor_blink_ms >= kCursorBlinkIntervalMs) {
    cursor_visible = !cursor_visible;
    last_cursor_blink_ms = now;
    draw_input_line();
  }
}
