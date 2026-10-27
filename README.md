```text
// ----------------------------------------------------------------------------
//   ___  ___  ___  ___       ___  ____  ___  _  _
//  /__/ /__/ /  / /__  /__/ /__    /   /_   / |/ /
// /    / \  /__/ ___/ ___/ ___/   /   /__  /    /  emulator
//
// ----------------------------------------------------------------------------
// Originally designed by Greg Stanton, ported to GNU/Linux by R.Shaklfrd.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
```

# Version 2.5b - October 26th, 2026

# Default Controls for gamepads

D-Pad: Joystick Direction

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
