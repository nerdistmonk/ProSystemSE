# ProsystemSE Emulator for Linux
Based on ProSystem 1.3 written by Greg Stanton

Ported to Linux using SDL3 by R.Shaklfrd and Gemini Flavorous Maximus.

Version 2.0 Completed in October 2026.

# Future Plans
*Make further improvements to gamepad support.
*Clean up remnants of old Windows era codebase.
*Attempt to improve compatibility with newer Atari 7800 homebrew.
*Add in xbrz/hqx filter support.

# Controls for gamepads

D-Pad / Left Analog Stick: Joystick Direction

A Button (South): Button 1 (Fire)

B / X Buttons: Button 2

Start Button: Console Reset

Back Button: Console Select

# Controls for Keyboard

F11 / Alt+Enter: Toggle Borderless Fullscreen

O / F2: Open Native File Chooser

F1: Quick Save State

F5: Quick Load State

F9: Cycle Save Slot (1–9)

Z / X / Arrows: P1 Action / Joystick

R / S: Console Reset / Select

# Build Instructions:

mkdir build && cd build

cmake .. && make
