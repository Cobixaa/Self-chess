# Chess RL + MCTS (C++17, no external libs)

This is a self-contained C++ chess program with:
- Legal move generation (including castling and en-passant)
- Zobrist hashing
- Monte Carlo Tree Search with lightweight playouts
- Simple CLI to play or self-train

No dependencies outside of the C++ standard library.

## Build (Termux on aarch64)

Use clang available in Termux and compile directly:

```bash
# Install clang if needed
pkg install -y clang

# Clone or cd to the project, then build:
cd /path/to/project
clang++ -std=c++17 -O3 -ffast-math -flto -pipe -fno-exceptions -fno-rtti -DNDEBUG \
  -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter \
  -Iinclude src/*.cpp -o chess_rl
```

Notes:
- On some devices, you can add CPU tuning flags. If you know your big cores (e.g. Cortex-A76/A78), try: `-mcpu=cortex-a76`.
- If `-flto` increases link time too much on your device, you can drop it.

## Build (generic Linux)

```bash
# Using clang++
clang++ -std=c++17 -O3 -ffast-math -pipe -fno-exceptions -fno-rtti -DNDEBUG \
  -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter \
  -Iinclude src/*.cpp -o chess_rl

# Or g++
g++ -std=c++17 -O3 -ffast-math -pipe -fno-exceptions -fno-rtti -DNDEBUG \
  -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter \
  -Iinclude src/*.cpp -o chess_rl
```

## Run

```bash
./chess_rl
```

You’ll see a minimal REPL:
- Type `play` to play as White against the engine (engine moves as Black). Enter moves like `e2e4`.
- Type `train` to run a batch of self-play games quickly. Default 1000; you can pass a number as the first program argument as well.
- Type `quit` to exit.

Example session:

```text
Type: play, train, or quit
play
...board prints...
Enter move like e2e4:
```

## Performance tuning tips
- Reduce/Increase engine thinking per move by changing simulations in `src/main.cpp` for `search_best_move`.
- For fastest self-play, use `train` mode which uses lower simulations per move.
- Disable I/O when benchmarking; console printing dominates runtime.
- Compile with `-Ofast` if your toolchain allows it: for clang++ recent versions, prefer `-O3 -ffast-math`.
- Try `-mcpu=native` on GCC or `-mcpu=<your-core>` on clang for extra speed.

## Design overview
- `include/board.hpp`, `src/board.cpp`: board state, legal move generation, hashing.
- `include/mcts.hpp`, `src/mcts.cpp`: MCTS with a lightweight playout policy biased by material.
- `src/common.cpp`, `include/common.hpp`: shared utilities and RNG.
- `src/main.cpp`: CLI entrypoint.

## Persistence
A simple persistent Q-table hook exists in MCTS (save/load), but it’s not wired by default in the CLI. You can enable file I/O in `src/main.cpp` if desired.

## Limitations
- This is a compact engine focused on learning via Monte Carlo and speed. It omits sophisticated evaluation and pruning used by top engines.
- Strength will improve with self-play and better playout heuristics; feel free to tweak `material_eval()` and rollout selection in `mcts.cpp`.

## License
MIT