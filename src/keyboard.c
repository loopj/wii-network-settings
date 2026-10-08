#include "keyboard.h"

#include <stdio.h>
#include <string.h>

#include "input.h"

#define MAX_ROWS 4

enum special_key {
  KEY_SHIFT,
  KEY_SPACE,
  KEY_BACKSPACE,
  KEY_DONE,
  KEY_CANCEL,
};

// Character rows for the text layout, unshifted and shifted
static const char *text_rows[2][MAX_ROWS] = {
  {"1234567890-=", "qwertyuiop[]", "asdfghjkl;'\\", "zxcvbnm,./`"},
  {"!@#$%^&*()_+", "QWERTYUIOP{}", "ASDFGHJKL:\"|", "ZXCVBNM<>?~"},
};

// Character row for the numeric layout
static const char *numeric_rows[1] = {"0123456789."};

// Bottom row of each layout
static const char *special_names[]              = {"Shift", "Space", "Backspace", "Done", "Cancel"};
static const enum special_key text_special[]    = {KEY_SHIFT, KEY_SPACE, KEY_BACKSPACE, KEY_DONE, KEY_CANCEL};
static const enum special_key numeric_special[] = {KEY_BACKSPACE, KEY_DONE, KEY_CANCEL};

struct layout {
  const char *const *rows;         // character rows for the current shift state
  int nrows;                       // number of character rows
  const enum special_key *special; // keys on the bottom row
  int nspecial;                    // number of keys on the bottom row
};

static void get_layout(enum keyboard_mode mode, bool shift, struct layout *l)
{
  if (mode == KEYBOARD_MODE_NUMERIC) {
    l->rows     = numeric_rows;
    l->nrows    = 1;
    l->special  = numeric_special;
    l->nspecial = sizeof(numeric_special) / sizeof(numeric_special[0]);
  } else {
    l->rows     = text_rows[shift ? 1 : 0];
    l->nrows    = MAX_ROWS;
    l->special  = text_special;
    l->nspecial = sizeof(text_special) / sizeof(text_special[0]);
  }
}

static int row_length(const struct layout *l, int row)
{
  return row < l->nrows ? (int)strlen(l->rows[row]) : l->nspecial;
}

static void draw_keyboard(const char *title, const char *text, const struct layout *l, int row, int col)
{
  // Show the text being edited with a cursor after it
  printf("\x1b[2J%s\n\n  %s_\n\n", title, text);

  // Draw the character rows with the current key in brackets
  for (int r = 0; r < l->nrows; r++) {
    printf(" ");
    for (int c = 0; l->rows[r][c] != 0; c++)
      printf(r == row && c == col ? "[%c]" : " %c ", l->rows[r][c]);
    printf("\n");
  }

  // Draw the bottom row the same way
  printf("\n ");
  for (int c = 0; c < l->nspecial; c++)
    printf(row == l->nrows && c == col ? "[%s]" : " %s ", special_names[l->special[c]]);
  printf("\n\nD-pad move, A type, B delete, + or START done, - or Y shift, HOME or Z cancel\n");
}

static void append_char(char *text, int capacity, char ch)
{
  int n = strlen(text);
  if (n + 1 < capacity) {
    text[n]     = ch;
    text[n + 1] = 0;
  }
}

static void delete_last_char(char *text)
{
  int n = strlen(text);
  if (n > 0)
    text[n - 1] = 0;
}

bool keyboard_edit(const char *title, char *text, int capacity, enum keyboard_mode mode)
{
  bool shift = false;
  int row    = 0;
  int col    = 0;
  struct layout l;
  for (;;) {
    get_layout(mode, shift, &l);
    draw_keyboard(title, text, &l, row, col);
    u32 b = input_wait_press();

    // Move the cursor, wrapping at the edges and keeping it on a key after a vertical move
    if (b & INPUT_UP)
      row = row > 0 ? row - 1 : l.nrows;
    if (b & INPUT_DOWN)
      row = row < l.nrows ? row + 1 : 0;
    if (b & INPUT_LEFT)
      col = col > 0 ? col - 1 : row_length(&l, row) - 1;
    if (b & INPUT_RIGHT)
      col = col < row_length(&l, row) - 1 ? col + 1 : 0;
    if (col >= row_length(&l, row))
      col = row_length(&l, row) - 1;

    // Handle the buttons that act without selecting a key
    if (b & INPUT_B)
      delete_last_char(text);
    if (b & INPUT_SHIFT)
      shift = !shift;
    if (b & INPUT_START)
      return true;
    if (b & INPUT_HOME)
      return false;
    if (!(b & INPUT_A))
      continue;

    // Type the selected character
    if (row < l.nrows) {
      append_char(text, capacity, l.rows[row][col]);
      continue;
    }

    // Or act on the selected bottom row key
    switch (l.special[col]) {
      case KEY_SHIFT:
        shift = !shift;
        break;
      case KEY_SPACE:
        append_char(text, capacity, ' ');
        break;
      case KEY_BACKSPACE:
        delete_last_char(text);
        break;
      case KEY_DONE:
        return true;
      case KEY_CANCEL:
        return false;
    }
  }
}
