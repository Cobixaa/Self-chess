#pragma once

#include "common.hpp"

struct Zobrist {
	std::array<std::array<uint64_t, 64>, 13> piece_square; // index 0 unused
	uint64_t black_to_move;
	std::array<uint64_t, 16> castling; // castle rights mask 0..15
	std::array<uint64_t, 8> ep_file; // en-passant file (a..h) when applicable

	Zobrist();
};

extern Zobrist ZOBRIST;

struct Board {
	std::array<Piece, 64> squares;
	bool white_to_move;
	uint8_t castling_rights; // bits: 1=WK,2=WQ,4=BK,8=BQ
	int8_t ep_square; // -1 if none else 0..63
	uint16_t halfmove_clock;
	uint16_t fullmove_number;
	uint64_t hash;

	Board();
	static Board startpos();

	bool is_square_attacked(int sq, bool by_white) const;
	bool in_check(bool for_white) const;
	void update_hash();

	std::vector<Move> generate_legal_moves() const;
	void make_move(const Move &m, Piece &captured_out, uint8_t &old_castle, int8_t &old_ep, uint16_t &old_half);
	void unmake_move(const Move &m, Piece captured, uint8_t old_castle, int8_t old_ep, uint16_t old_half);

	GameResult evaluate_terminal() const; // simple: checkmate/stalemate/50-move
	int material_eval() const; // for playout bias
};

inline int file_of(int sq) { return sq & 7; }
inline int rank_of(int sq) { return sq >> 3; }
inline bool on_board(int sq) { return sq >= 0 && sq < 64; }
inline int idx(int r, int f) { return (r << 3) | f; }

