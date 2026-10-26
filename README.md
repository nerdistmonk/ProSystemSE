# ProsystemSE Emulator for Linux
Originally based on Greg Stanton's ProSystem Emulator.

Ported to Linux using SDL3.

Version 2.5 - October 26th, 2026

# Default Controls for gamepads

D-Pad / Left Analog Stick: Joystick Direction

A Button (South): Button 1 (Fire)

B / X Buttons: Button 2

Start Button: Console Reset

Back Button: Console Select

# Default Controls for Keyboard

F11 / Alt+Enter: Toggle Borderless Fullscreen

O / F2: Open Native File Chooser

F1: Quick Save State

F5: Quick Load State

F9: Cycle Save Slot (1–9)

Q / E / WASD: P1 Action / Joystick

N / M / Arrow Keys: P2 Action / Joystick

Z / X: Console Reset / Select

# Build Instructions:
mkdir build && cd build

cmake .. && make

(Requires cmake/gcc/SDL3/zlib/minizip)
