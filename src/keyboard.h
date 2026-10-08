#pragma once

#include <stdbool.h>

// Character sets the keyboard offers
enum keyboard_mode {
  KEYBOARD_MODE_TEXT,    // letters, digits and symbols
  KEYBOARD_MODE_NUMERIC, // digits and a dot
};

// Lets the user edit text in place and returns true when they accepted it
bool keyboard_edit(const char *title, char *text, int capacity, enum keyboard_mode mode);
