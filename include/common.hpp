#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <fstream>
#include <sstream>
#include <iostream>
#include <limits>
#include <cmath>

using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

constexpr int BOARD_SIZE = 64;

enum Piece : int8_t {
	EMPTY = 0,
	WP = 1, WN = 2, WB = 3, WR = 4, WQ = 5, WK = 6,
	BP = -1, BN = -2, BB = -3, BR = -4, BQ = -5, BK = -6
};

inline bool is_white(Piece p) { return p > 0; }
inline bool is_black(Piece p) { return p < 0; }
inline bool is_empty(Piece p) { return p == EMPTY; }
inline int abs_piece(Piece p) { return p < 0 ? -p : p; }

struct Move {
	uint8_t from;
	uint8_t to;
	int8_t promotion; // 0 if none, otherwise Piece code for promoted piece (positive for white, negative for black)
	uint8_t flags; // bit flags: 1=castle,2=enpassant,4=promotion
};

struct GameResult { // reward from white's perspective
	float reward; // +1 win, 0 draw, -1 loss
	bool terminal;
};

struct RNG {
	std::mt19937_64 gen;
	RNG() {
		std::random_device rd;
		std::seed_seq seq{rd(), (uint32_t)time(nullptr)};
		gen.seed(seq);
	}
	inline double uniform01() { return std::generate_canonical<double, 64>(gen); }
	inline uint64_t u64() { return gen(); }
};

extern RNG GLOBAL_RNG;

