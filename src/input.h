#pragma once

#include <gctypes.h>

// Logical buttons shared by the Wii Remote and the GameCube controller
#define INPUT_UP 0x01
#define INPUT_DOWN 0x02
#define INPUT_LEFT 0x04
#define INPUT_RIGHT 0x08
#define INPUT_A 0x10
#define INPUT_B 0x20
#define INPUT_START 0x40
#define INPUT_SHIFT 0x80
#define INPUT_HOME 0x100

// Starts the Wii Remote and GameCube controller drivers
void input_init(void);

// Waits until at least one button is pressed and returns those buttons
u32 input_wait_press(void);
