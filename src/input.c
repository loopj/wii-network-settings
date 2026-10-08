#include "input.h"

#include <gccore.h>
#include <wiiuse/wpad.h>

// Frames a direction is held before it starts repeating, and between repeats
#define REPEAT_DELAY 20
#define REPEAT_RATE 4

// GameCube stick deflection that counts as a direction
#define STICK_THRESHOLD 40

#define DIRECTIONS (INPUT_UP | INPUT_DOWN | INPUT_LEFT | INPUT_RIGHT)

static u32 prev_held;
static int held_frames;

void input_init(void)
{
  PAD_Init();
  WPAD_Init();
}

static u32 map_wpad_buttons(u32 b)
{
  u32 r = 0;
  if (b & WPAD_BUTTON_UP)
    r |= INPUT_UP;
  if (b & WPAD_BUTTON_DOWN)
    r |= INPUT_DOWN;
  if (b & WPAD_BUTTON_LEFT)
    r |= INPUT_LEFT;
  if (b & WPAD_BUTTON_RIGHT)
    r |= INPUT_RIGHT;
  if (b & WPAD_BUTTON_A)
    r |= INPUT_A;
  if (b & WPAD_BUTTON_B)
    r |= INPUT_B;
  if (b & WPAD_BUTTON_PLUS)
    r |= INPUT_START;
  if (b & WPAD_BUTTON_MINUS)
    r |= INPUT_SHIFT;
  if (b & WPAD_BUTTON_HOME)
    r |= INPUT_HOME;

  return r;
}

static u32 map_pad_buttons(u32 b, s8 x, s8 y)
{
  // The main stick doubles as a D-pad
  u32 r = 0;
  if ((b & PAD_BUTTON_UP) || y > STICK_THRESHOLD)
    r |= INPUT_UP;
  if ((b & PAD_BUTTON_DOWN) || y < -STICK_THRESHOLD)
    r |= INPUT_DOWN;
  if ((b & PAD_BUTTON_LEFT) || x < -STICK_THRESHOLD)
    r |= INPUT_LEFT;
  if ((b & PAD_BUTTON_RIGHT) || x > STICK_THRESHOLD)
    r |= INPUT_RIGHT;
  if (b & PAD_BUTTON_A)
    r |= INPUT_A;
  if (b & PAD_BUTTON_B)
    r |= INPUT_B;
  if (b & PAD_BUTTON_START)
    r |= INPUT_START;
  if (b & PAD_BUTTON_Y)
    r |= INPUT_SHIFT;
  if (b & PAD_TRIGGER_Z)
    r |= INPUT_HOME;

  return r;
}

static u32 poll(void)
{
  // Merge every controller into one held set once per frame
  VIDEO_WaitVSync();
  WPAD_ScanPads();
  PAD_ScanPads();
  u32 held = 0;
  for (int i = 0; i < 4; i++) {
    held |= map_wpad_buttons(WPAD_ButtonsHeld(i));
    held |= map_pad_buttons(PAD_ButtonsHeld(i), PAD_StickX(i), PAD_StickY(i));
  }

  // Count edges as presses
  u32 pressed = held & ~prev_held;

  // Repeat a direction that has been held long enough
  if ((held & DIRECTIONS) != 0 && (held & DIRECTIONS) == (prev_held & DIRECTIONS)) {
    held_frames++;
    if (held_frames >= REPEAT_DELAY && (held_frames - REPEAT_DELAY) % REPEAT_RATE == 0)
      pressed |= held & DIRECTIONS;
  } else {
    held_frames = 0;
  }

  prev_held = held;
  return pressed;
}

u32 input_wait_press(void)
{
  u32 b;
  do {
    b = poll();
  } while (b == 0);

  return b;
}
