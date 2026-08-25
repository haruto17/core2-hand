#pragma once

const char* special_key_name(char ch);

bool setup_keyboard_i2c();

void update_unit();

bool is_keyboard_updated();

bool is_keyboard_available();

bool read_key(char& key);