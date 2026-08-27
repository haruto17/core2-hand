#pragma once

#include <Arduino.h>

#include <vector>

class BasicInterpreter {
 public:
  bool run(const std::vector<String>& program);

 private:
  enum class ValueType { Integer, Text };

  struct Value {
    ValueType type{ValueType::Integer};
    int32_t integer{};
    String text;
  };

  struct Variable {
    String name;
    Value value;
  };

  std::vector<Variable> variables_;
  String error_;
  uint16_t text_color_{0xFFFF};

  bool execute(const String& statement, bool allow_control_flow = true);
  bool execute_assignment(const String& statement, int equals_position);
  bool execute_if(const String& statement);
  bool execute_loop(const String& statement);
  bool evaluate_condition(const String& expression, bool& result);
  bool resolve_value(const String& token, Value& value);
  bool resolve_integer(const String& token, int32_t& value);
  bool set_variable(const String& name, const Value& value);
  const Value* find_variable(const String& name) const;

  void show_error(size_t line_number);
  bool fail(const String& message);
};
