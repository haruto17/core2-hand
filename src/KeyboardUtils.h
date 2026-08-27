#pragma once

const char* special_key_name(char ch);

bool setup_keyboard_uart();

void update_unit();

bool is_keyboard_updated();

bool is_keyboard_available();

bool read_key(char& key);

bool is_cursor_left_key(char key);

bool is_cursor_right_key(char key);

bool is_cursor_up_key(char key);

bool is_cursor_down_key(char key);

bool is_escape_key(char key);
