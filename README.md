# sfml-apps

A collection of small C++/SFML applications. Each app lives under `apps/<name>/`
and is built from the top-level CMake project. SFML 3 is fetched and built
automatically via CMake `FetchContent` — no system install required.

## Apps

- **maze** (`apps/maze/`) — a tile-based maze rendered from BMP images. C++/SFML 3
  port of [`maze.c`](maze.c), the original SDL3 reference kept at the repo root.
  Move the player with the arrow keys or by clicking an adjacent tile; walls block
  movement; stepping on the exit advances to the next map (wrapping after the last).
  `apps/maze/reference_procedural.cpp` is an alternative, more procedural port kept
  for reference (it is not built).

## Build

Linux needs X11/OpenGL dev packages to build SFML from source:

```bash
sudo apt-get install -y libx11-dev libxrandr-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev libudev-dev libfreetype-dev
```

Then configure and build:

```bash
cmake -S . -B build
cmake --build build -j
```

The first build downloads and compiles SFML 3 (a few minutes); later builds are fast.

## Run

```bash
./build/apps/maze/maze
```

Assets are copied next to each executable at build time, so the app finds them
regardless of the directory it is launched from.
