#include "BasicInterpreter.h"

#include <M5Unified.h>
#include <M5Utility.h>
#include <esp_system.h>

#include <cerrno>
#include <climits>
#include <cstdlib>

namespace {

String trimmed(String value) {
  value.trim();
  return value;
}

bool is_identifier(const String& value) {
  if (value.isEmpty() || !(isAlpha(value[0]) || value[0] == '_')) {
    return false;
  }
  for (size_t i = 1; i < value.length(); ++i) {
    if (!(isAlphaNumeric(value[i]) || value[i] == '_')) {
      return false;
    }
  }
  return true;
}

bool starts_with_command(const String& upper, const char* command) {
  const size_t length = strlen(command);
  return upper == command ||
         (upper.length() > length && upper.startsWith(command) &&
          isSpace(upper[length]));
}

int find_outside_quotes(const String& value, char wanted, size_t from = 0) {
  bool quoted = false;
  for (size_t i = from; i < value.length(); ++i) {
    if (value[i] == '"') {
      quoted = !quoted;
    } else if (!quoted && value[i] == wanted) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int find_keyword(const String& value, const char* keyword, size_t from = 0) {
  String upper = value;
  upper.toUpperCase();
  const size_t length = strlen(keyword);
  bool quoted = false;
  for (size_t i = from; i + length <= upper.length(); ++i) {
    if (upper[i] == '"') {
      quoted = !quoted;
      continue;
    }
    if (quoted || upper.substring(i, i + length) != keyword) {
      continue;
    }
    const bool left_ok = i == 0 || isSpace(upper[i - 1]);
    const bool right_ok =
        i + length == upper.length() || isSpace(upper[i + length]);
    if (left_ok && right_ok) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool parse_integer(const String& token, int32_t& result) {
  if (token.isEmpty()) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const long parsed = strtol(token.c_str(), &end, 10);
  if (errno == ERANGE || end == token.c_str() || *end != '\0' ||
      parsed < INT32_MIN || parsed > INT32_MAX) {
    return false;
  }
  result = static_cast<int32_t>(parsed);
  return true;
}

}  // namespace

bool BasicInterpreter::run(const std::vector<String>& program) {
  variables_.clear();
  error_ = "";
  text_color_ = TFT_WHITE;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(0, 0);

  for (size_t i = 0; i < program.size(); ++i) {
    if (!execute(program[i])) {
      show_error(i + 1);
      return false;
    }
  }
  return true;
}

bool BasicInterpreter::execute(const String& raw_statement,
                               bool allow_control_flow) {
  const String statement = trimmed(raw_statement);
  if (statement.isEmpty()) {
    return true;
  }

  String upper = statement;
  upper.toUpperCase();

  if (starts_with_command(upper, "PRINT")) {
    Value value;
    if (!resolve_value(trimmed(statement.substring(5)), value)) {
      return false;
    }
    M5.Display.setTextColor(text_color_, TFT_BLACK);
    if (value.type == ValueType::Integer) {
      M5.Display.println(value.integer);
    } else {
      M5.Display.println(value.text);
    }
    return true;
  }

  if (upper == "CLEAR") {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    return true;
  }

  const int equals = find_outside_quotes(statement, '=');
  if (equals >= 0) {
    return execute_assignment(statement, equals);
  }
  return fail("UNKNOWN COMMAND");
}

bool BasicInterpreter::execute_assignment(const String& statement,
                                          int equals_position) {
  String name = trimmed(statement.substring(0, equals_position));
  name.toUpperCase();
  if (!is_identifier(name)) {
    return fail("BAD VARIABLE");
  }
  Value value;
  if (!resolve_value(trimmed(statement.substring(equals_position + 1)),
                     value)) {
    return false;
  }
  return set_variable(name, value);
}

bool BasicInterpreter::resolve_value(const String& raw_token, Value& value) {
  const String token = trimmed(raw_token);
  if (token.length() >= 2 && token[0] == '"' &&
      token[token.length() - 1] == '"') {
    value.type = ValueType::Text;
    value.text = token.substring(1, token.length() - 1);
    return true;
  }
  if (token.indexOf('"') >= 0) {
    return fail("BAD STRING");
  }
  String upper = token;
  upper.toUpperCase();
  if (starts_with_command(upper, "RND")) {
    int32_t maximum{};
    if (!resolve_integer(trimmed(token.substring(3)), maximum) || maximum < 0) {
      return fail("BAD RND");
    }
    const uint64_t range = static_cast<uint64_t>(maximum) + 1U;
    value.type = ValueType::Integer;
    value.integer = static_cast<int32_t>(esp_random() % range);
    value.text = "";
    return true;
  }
  int32_t integer{};
  if (parse_integer(token, integer)) {
    value.type = ValueType::Integer;
    value.integer = integer;
    value.text = "";
    return true;
  }
  String name = token;
  name.toUpperCase();
  if (!is_identifier(name)) {
    return fail("VALUE EXPECTED");
  }
  const Value* found = find_variable(name);
  if (found == nullptr) {
    return fail("UNDEFINED " + name);
  }
  value = *found;
  return true;
}

bool BasicInterpreter::resolve_integer(const String& token, int32_t& value) {
  Value resolved;
  if (!resolve_value(token, resolved)) {
    return false;
  }
  if (resolved.type != ValueType::Integer) {
    return fail("INTEGER EXPECTED");
  }
  value = resolved.integer;
  return true;
}

bool BasicInterpreter::set_variable(const String& name, const Value& value) {
  for (auto& variable : variables_) {
    if (variable.name == name) {
      variable.value = value;
      return true;
    }
  }
  variables_.push_back({name, value});
  return true;
}

const BasicInterpreter::Value* BasicInterpreter::find_variable(
    const String& name) const {
  for (const auto& variable : variables_) {
    if (variable.name == name) {
      return &variable.value;
    }
  }
  return nullptr;
}

void BasicInterpreter::show_error(size_t line_number) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.printf("ERROR LINE %u\n", static_cast<unsigned>(line_number));
  M5.Display.println(error_);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

bool BasicInterpreter::fail(const String& message) {
  error_ = message;
  return false;
}
