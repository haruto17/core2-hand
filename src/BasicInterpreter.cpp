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

  if (starts_with_command(upper, "CURSOR")) {
    const String arguments = trimmed(statement.substring(6));
    const int comma = find_outside_quotes(arguments, ',');
    int32_t x{};
    int32_t y{};
    if (comma < 0 ||
        !resolve_integer(trimmed(arguments.substring(0, comma)), x) ||
        !resolve_integer(trimmed(arguments.substring(comma + 1)), y)) {
      return fail("BAD CURSOR");
    }
    M5.Display.setCursor(x, y);
    return true;
  }

  if (starts_with_command(upper, "WAIT")) {
    int32_t seconds{};
    if (!resolve_integer(trimmed(statement.substring(4)), seconds) ||
        seconds < 0) {
      return fail("BAD WAIT");
    }
    for (int32_t i = 0; i < seconds; ++i) {
      m5::utility::delay(1000);
    }
    return true;
  }

  if (starts_with_command(upper, "BEEP")) {
    const String arguments = trimmed(statement.substring(4));
    const int comma = find_outside_quotes(arguments, ',');
    int32_t frequency{};
    int32_t seconds{};
    if (comma < 0 || find_outside_quotes(arguments, ',', comma + 1) >= 0 ||
        !resolve_integer(trimmed(arguments.substring(0, comma)), frequency) ||
        !resolve_integer(trimmed(arguments.substring(comma + 1)), seconds) ||
        frequency <= 0 || seconds < 0 ||
        static_cast<uint32_t>(seconds) > UINT32_MAX / 1000U) {
      return fail("BAD BEEP");
    }
    if (seconds == 0) {
      M5.Speaker.stop();
      return true;
    }
    if (!M5.Speaker.tone(static_cast<float>(frequency),
                         static_cast<uint32_t>(seconds) * 1000U)) {
      return fail("BEEP ERROR");
    }
    return true;
  }

  if (upper == "MUTE") {
    M5.Speaker.stop();
    return true;
  }

  if (starts_with_command(upper, "COLOR")) {
    const String arguments = trimmed(statement.substring(5));
    const int first_comma = find_outside_quotes(arguments, ',');
    const int second_comma =
        first_comma < 0
            ? -1
            : find_outside_quotes(arguments, ',', first_comma + 1);
    int32_t red{};
    int32_t green{};
    int32_t blue{};
    if (first_comma < 0 || second_comma < 0 ||
        find_outside_quotes(arguments, ',', second_comma + 1) >= 0 ||
        !resolve_integer(trimmed(arguments.substring(0, first_comma)), red) ||
        !resolve_integer(
            trimmed(arguments.substring(first_comma + 1, second_comma)),
            green) ||
        !resolve_integer(trimmed(arguments.substring(second_comma + 1)), blue) ||
        red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 ||
        blue > 255) {
      return fail("BAD COLOR");
    }
    text_color_ = M5.Display.color565(static_cast<uint8_t>(red),
                                      static_cast<uint8_t>(green),
                                      static_cast<uint8_t>(blue));
    return true;
  }

  if (starts_with_command(upper, "IF")) {
    if (!allow_control_flow) {
      return fail("NESTED COMMAND");
    }
    return execute_if(statement);
  }

  if (starts_with_command(upper, "LOOP")) {
    if (!allow_control_flow) {
      return fail("NESTED COMMAND");
    }
    return execute_loop(statement);
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

bool BasicInterpreter::execute_if(const String& statement) {
  const int then_position = find_keyword(statement, "THEN", 2);
  if (then_position < 0) {
    return fail("THEN EXPECTED");
  }

  bool condition{};
  if (!evaluate_condition(trimmed(statement.substring(2, then_position)),
                          condition)) {
    return false;
  }

  const String actions = trimmed(statement.substring(then_position + 4));
  const int else_position = find_keyword(actions, "ELSE");
  const String when_true = trimmed(
      else_position < 0 ? actions : actions.substring(0, else_position));
  const String when_false =
      else_position < 0 ? "" : trimmed(actions.substring(else_position + 4));
  if (when_true.isEmpty()) {
    return fail("COMMAND EXPECTED");
  }
  if (condition) {
    return execute(when_true, false);
  }
  return when_false.isEmpty() ? true : execute(when_false, false);
}

bool BasicInterpreter::execute_loop(const String& statement) {
  const int equals = find_outside_quotes(statement, '=', 4);
  if (equals < 0) {
    return fail("BAD LOOP");
  }
  const int to_position = find_keyword(statement, "TO", equals + 1);
  if (to_position < 0) {
    return fail("BAD LOOP");
  }
  const int then_position = find_keyword(statement, "THEN", to_position + 2);
  if (then_position < 0) {
    return fail("BAD LOOP");
  }

  String name = trimmed(statement.substring(4, equals));
  name.toUpperCase();
  if (!is_identifier(name)) {
    return fail("BAD VARIABLE");
  }

  int32_t first{};
  int32_t last{};
  if (!resolve_integer(trimmed(statement.substring(equals + 1, to_position)),
                       first) ||
      !resolve_integer(
          trimmed(statement.substring(to_position + 2, then_position)), last)) {
    return fail("INTEGER EXPECTED");
  }
  const String body = trimmed(statement.substring(then_position + 4));
  if (body.isEmpty()) {
    return fail("COMMAND EXPECTED");
  }

  if (first > last) {
    return true;
  }
  Value counter;
  counter.type = ValueType::Integer;
  for (int32_t current = first;; ++current) {
    counter.integer = current;
    set_variable(name, counter);
    if (!execute(body, false)) {
      return false;
    }
    if (current == last) {
      break;
    }
  }
  return true;
}

bool BasicInterpreter::evaluate_condition(const String& expression,
                                          bool& result) {
  int operator_position = -1;
  size_t operator_length = 1;
  bool less_or_equal = false;
  bool less_than = false;
  bool quoted = false;

  for (size_t i = 0; i < expression.length(); ++i) {
    if (expression[i] == '"') {
      quoted = !quoted;
      continue;
    }
    if (quoted) {
      continue;
    }
    if (expression[i] == '<' && i + 1 < expression.length() &&
        expression[i + 1] == '=') {
      operator_position = static_cast<int>(i);
      operator_length = 2;
      less_or_equal = true;
      break;
    }
    if (expression[i] == '<') {
      operator_position = static_cast<int>(i);
      less_than = true;
      break;
    }
    if (expression[i] == '=') {
      operator_position = static_cast<int>(i);
      break;
    }
  }
  if (operator_position < 0) {
    return fail("BAD CONDITION");
  }

  Value left;
  Value right;
  if (!resolve_value(trimmed(expression.substring(0, operator_position)),
                     left) ||
      !resolve_value(
          trimmed(expression.substring(operator_position + operator_length)),
          right)) {
    return false;
  }
  if (left.type != right.type) {
    return fail("TYPE MISMATCH");
  }
  if (left.type == ValueType::Integer) {
    result = less_or_equal ? left.integer <= right.integer
             : less_than   ? left.integer < right.integer
                           : left.integer == right.integer;
  } else {
    const int comparison = left.text.compareTo(right.text);
    result = less_or_equal ? comparison <= 0
             : less_than   ? comparison < 0
                           : comparison == 0;
  }
  return true;
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
