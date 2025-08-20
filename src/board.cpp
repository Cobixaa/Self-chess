#include "board.hpp"

Zobrist ZOBRIST;

Zobrist::Zobrist() {
	for (auto &arr : piece_square) {
		for (auto &v : arr) v = GLOBAL_RNG.u64();
	}
	black_to_move = GLOBAL_RNG.u64();
	for (auto &v : castling) v = GLOBAL_RNG.u64();
	for (auto &v : ep_file) v = GLOBAL_RNG.u64();
}

static const int PIECE_INDEX[13] = {0,1,2,3,4,5,6,0,7,8,9,10,11};

Board::Board() {
	squares.fill(EMPTY);
	white_to_move = true;
	castling_rights = 0;
	ep_square = -1;
	halfmove_clock = 0;
	fullmove_number = 1;
	hash = 0;
}

Board Board::startpos() {
	Board b;
	const Piece backrank_w[8] = {WR, WN, WB, WQ, WK, WB, WN, WR};
	const Piece backrank_b[8] = {BR, BN, BB, BQ, BK, BB, BN, BR};
	for (int f=0; f<8; ++f) {
		b.squares[idx(0,f)] = backrank_w[f];
		b.squares[idx(1,f)] = WP;
		b.squares[idx(6,f)] = BP;
		b.squares[idx(7,f)] = backrank_b[f];
	}
	b.white_to_move = true;
	b.castling_rights = 1|2|4|8;
	b.ep_square = -1;
	b.halfmove_clock = 0;
	b.fullmove_number = 1;
	b.update_hash();
	return b;
}

void Board::update_hash() {
	uint64_t h = 0;
	for (int sq=0; sq<64; ++sq) {
		Piece p = squares[sq];
		if (p != EMPTY) {
			int pi = PIECE_INDEX[p + 6];
			h ^= ZOBRIST.piece_square[pi][sq];
		}
	}
	if (!white_to_move) h ^= ZOBRIST.black_to_move;
	h ^= ZOBRIST.castling[castling_rights & 15];
	if (ep_square >= 0) h ^= ZOBRIST.ep_file[file_of(ep_square)];
	hash = h;
}

static inline bool slide(const Board &b, int sq, int dr, int df, bool by_white) {
	int r = rank_of(sq), f = file_of(sq);
	while (true) {
		r += dr; f += df;
		if (r < 0 || r >= 8 || f < 0 || f >= 8) return false;
		int to = idx(r,f);
		Piece p = b.squares[to];
		if (!is_empty(p)) {
			return (by_white && is_white(p)) || (!by_white && is_black(p));
		}
	}
}

bool Board::is_square_attacked(int sq, bool by_white) const {
	int r = rank_of(sq), f = file_of(sq);
	// Pawns
	int pr = by_white ? -1 : 1;
	for (int df=-1; df<=1; df+=2) {
		int rr = r + pr, ff = f + df;
		if (rr>=0 && rr<8 && ff>=0 && ff<8) {
			Piece p = squares[idx(rr,ff)];
			if (p == (by_white?WP:BP)) return true;
		}
	}
	// Knights
	static const int KOFF[8][2]={{2,1},{1,2},{-1,2},{-2,1},{-2,-1},{-1,-2},{1,-2},{2,-1}};
	for (auto &o:KOFF){int rr=r+o[0], ff=f+o[1]; if(rr>=0&&rr<8&&ff>=0&&ff<8){Piece p=squares[idx(rr,ff)]; if(p==(by_white?WN:BN)) return true;}}
	// Diagonals (bishops/queens)
	for (auto dir : std::array<std::pair<int,int>,4>{{std::make_pair(1,1),{1,-1},{-1,1},{-1,-1}}}) {
		int rr=r+dir.first, ff=f+dir.second;
		while (rr>=0&&rr<8&&ff>=0&&ff<8) {
			Piece p = squares[idx(rr,ff)];
			if (!is_empty(p)) {
				if (is_white(p)==by_white && (abs_piece(p)==3 || abs_piece(p)==5)) return true;
				break;
			}
			r+=dir.first; ff+=dir.second;
		}
	}
	// Orthogonals (rooks/queens)
	for (auto dir : std::array<std::pair<int,int>,4>{{std::make_pair(1,0),{-1,0},{0,1},{0,-1}}}) {
		int rr=r+dir.first, ff=f+dir.second;
		while (rr>=0&&rr<8&&ff>=0&&ff<8) {
			Piece p = squares[idx(rr,ff)];
			if (!is_empty(p)) {
				if (is_white(p)==by_white && (abs_piece(p)==4 || abs_piece(p)==5)) return true;
				break;
			}
			r+=dir.first; ff+=dir.second;
		}
	}
	// Kings
	for (int dr=-1; dr<=1; ++dr) for (int df=-1; df<=1; ++df) if (dr||df) {
		int rr=r+dr, ff=f+df; if(rr>=0&&rr<8&&ff>=0&&ff<8){Piece p=squares[idx(rr,ff)]; if(p==(by_white?WK:BK)) return true;}
	}
	return false;
}

bool Board::in_check(bool for_white) const {
	int king_sq = -1; Piece k = for_white?WK:BK;
	for (int i=0;i<64;++i) if (squares[i]==k){ king_sq=i; break; }
	if (king_sq<0) return false; // shouldn't happen
	return is_square_attacked(king_sq, !for_white);
}

static void add_if_legal(const Board &b, std::vector<Move> &moves, int from, int to, int promo=0, uint8_t flags=0) {
	Board c = b;
	Piece cap; uint8_t oc; int8_t oe; uint16_t oh;
	Move m{(uint8_t)from,(uint8_t)to,(int8_t)promo,flags};
	c.make_move(m, cap, oc, oe, oh);
	bool illegal = c.in_check(!c.white_to_move);
	c.unmake_move(m, cap, oc, oe, oh);
	if (!illegal) moves.push_back(m);
}

std::vector<Move> Board::generate_legal_moves() const {
	std::vector<Move> moves;
	moves.reserve(64);
	for (int sq=0; sq<64; ++sq) {
		Piece p = squares[sq]; if (p==EMPTY) continue; if (white_to_move != is_white(p)) continue;
		int r=rank_of(sq), f=file_of(sq);
		switch (abs_piece(p)) {
			case 1: { // Pawn
				int dir = is_white(p) ? 1 : -1;
				int start_rank = is_white(p) ? 1 : 6;
				int promo_rank = is_white(p) ? 6 : 1;
				int rr = r + dir;
				if (rr>=0 && rr<8) {
					int to = idx(rr,f);
					if (is_empty(squares[to])) {
						if (r==promo_rank) {
							for (int pr : {4,5,2,3}) { // R,Q,N,B in code index terms later converted
								int pp = is_white(p) ? pr : -pr;
								add_if_legal(*this,moves,sq,to,pp,4);
							}
						} else {
							add_if_legal(*this,moves,sq,to);
							if (r==start_rank) {
								int to2 = idx(r+2*dir, f);
								if (is_empty(squares[to2])) add_if_legal(*this,moves,sq,to2);
							}
						}
					}
					// captures
					for (int df=-1; df<=1; df+=2) {
						int ff=f+df; if(ff<0||ff>=8) continue; int to2=idx(rr,ff); if(rr<0||rr>=8) continue;
						if (!is_empty(squares[to2]) && (is_white(p)!=is_white(squares[to2]))) {
							if (r==promo_rank) {
								for (int pr : {4,5,2,3}) { int pp = is_white(p)?pr:-pr; add_if_legal(*this,moves,sq,to2,pp,4);} 
							} else add_if_legal(*this,moves,sq,to2);
						}
						// en-passant
						if (ep_square>=0 && to2==ep_square) {
							add_if_legal(*this,moves,sq,to2,0,2);
						}
					}
				}
				}
				break;
			case 2: { // Knight
				static const int KOFF[8][2]={{2,1},{1,2},{-1,2},{-2,1},{-2,-1},{-1,-2},{1,-2},{2,-1}};
				for (auto &o:KOFF){int rr=r+o[0], ff=f+o[1]; if(rr>=0&&rr<8&&ff>=0&&ff<8){int to=idx(rr,ff); Piece q=squares[to]; if(is_empty(q)||is_white(q)!=is_white(p)) add_if_legal(*this,moves,sq,to);}}
			}
			break;
			case 3: { // Bishop
				for (int dr : {1,-1}) for (int df : {1,-1}) {
					int rr=r+dr, ff=f+df; while(rr>=0&&rr<8&&ff>=0&&ff<8){int to=idx(rr,ff); Piece q=squares[to]; if(is_empty(q)) {add_if_legal(*this,moves,sq,to);} else { if(is_white(q)!=is_white(p)) add_if_legal(*this,moves,sq,to); break;} rr+=dr; ff+=df;}
				}
			}
			break;
			case 4: { // Rook
				for (auto [dr,df] : std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}) {
					int rr=r+dr, ff=f+df; while(rr>=0&&rr<8&&ff>=0&&ff<8){int to=idx(rr,ff); Piece q=squares[to]; if(is_empty(q)) {add_if_legal(*this,moves,sq,to);} else { if(is_white(q)!=is_white(p)) add_if_legal(*this,moves,sq,to); break;} rr+=dr; ff+=df;}
				}
			}
			break;
			case 5: { // Queen
				for (auto [dr,df] : std::array<std::pair<int,int>,8>{{{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}}}) {
					int rr=r+dr, ff=f+df; while(rr>=0&&rr<8&&ff>=0&&ff<8){int to=idx(rr,ff); Piece q=squares[to]; if(is_empty(q)) {add_if_legal(*this,moves,sq,to);} else { if(is_white(q)!=is_white(p)) add_if_legal(*this,moves,sq,to); break;} rr+=dr; ff+=df;}
				}
			}
			break;
			case 6: { // King
				for (int dr=-1; dr<=1; ++dr) for (int df=-1; df<=1; ++df) if (dr||df){int rr=r+dr, ff=f+df; if(rr>=0&&rr<8&&ff>=0&&ff<8){int to=idx(rr,ff); Piece q=squares[to]; if(is_empty(q)||is_white(q)!=is_white(p)) add_if_legal(*this,moves,sq,to);}}
				// Castling
				if (!in_check(is_white(p))) {
					if (is_white(p)) {
						// King side
						if ((castling_rights & 1) && is_empty(squares[idx(0,5)]) && is_empty(squares[idx(0,6)]) && !is_square_attacked(idx(0,5), false) && !is_square_attacked(idx(0,6), false)) {
							add_if_legal(*this,moves,sq,idx(0,6),0,1);
						}
						// Queen side
						if ((castling_rights & 2) && is_empty(squares[idx(0,1)]) && is_empty(squares[idx(0,2)]) && is_empty(squares[idx(0,3)]) && !is_square_attacked(idx(0,2), false) && !is_square_attacked(idx(0,3), false)) {
							add_if_legal(*this,moves,sq,idx(0,2),0,1);
						}
					} else {
						if ((castling_rights & 4) && is_empty(squares[idx(7,5)]) && is_empty(squares[idx(7,6)]) && !is_square_attacked(idx(7,5), true) && !is_square_attacked(idx(7,6), true)) {
							add_if_legal(*this,moves,sq,idx(7,6),0,1);
						}
						if ((castling_rights & 8) && is_empty(squares[idx(7,1)]) && is_empty(squares[idx(7,2)]) && is_empty(squares[idx(7,3)]) && !is_square_attacked(idx(7,2), true) && !is_square_attacked(idx(7,3), true)) {
							add_if_legal(*this,moves,sq,idx(7,2),0,1);
						}
					}
				}
			}
			break;
		}
	}
	return moves;
}

void Board::make_move(const Move &m, Piece &captured_out, uint8_t &old_castle, int8_t &old_ep, uint16_t &old_half) {
	old_castle = castling_rights;
	old_ep = ep_square;
	old_half = halfmove_clock;
	Piece moving = squares[m.from];
	Piece captured = squares[m.to];
	captured_out = captured;
	// hash out
	if (moving != EMPTY) hash ^= ZOBRIST.piece_square[PIECE_INDEX[moving+6]][m.from];
	if (captured != EMPTY) hash ^= ZOBRIST.piece_square[PIECE_INDEX[captured+6]][m.to];
	// side-to-move toggled once at end
	hash ^= ZOBRIST.castling[castling_rights & 15];
	if (ep_square >= 0) hash ^= ZOBRIST.ep_file[file_of(ep_square)];

	ep_square = -1;
	// move piece
	squares[m.to] = squares[m.from];
	squares[m.from] = EMPTY;
	// special moves
	if (m.flags & 2) { // en-passant capture
		int dir = is_white(moving) ? -1 : 1;
		int cap_sq = idx(rank_of(m.to)+dir, file_of(m.to));
		captured_out = squares[cap_sq];
		if (captured_out != EMPTY) {
			hash ^= ZOBRIST.piece_square[PIECE_INDEX[captured_out+6]][cap_sq];
			squares[cap_sq] = EMPTY;
		}
	}
	if (abs_piece(moving)==6 && (m.flags & 1)) { // castle
		if (file_of(m.to)==6) { // king side
			int r = is_white(moving)?0:7;
			int rook_from = idx(r,7), rook_to = idx(r,5);
			Piece rook = squares[rook_from];
			squares[rook_to] = rook; squares[rook_from] = EMPTY;
			hash ^= ZOBRIST.piece_square[PIECE_INDEX[rook+6]][rook_from];
			hash ^= ZOBRIST.piece_square[PIECE_INDEX[rook+6]][rook_to];
		} else if (file_of(m.to)==2) {
			int r = is_white(moving)?0:7;
			int rook_from = idx(r,0), rook_to = idx(r,3);
			Piece rook = squares[rook_from];
			squares[rook_to] = rook; squares[rook_from] = EMPTY;
			hash ^= ZOBRIST.piece_square[PIECE_INDEX[rook+6]][rook_from];
			hash ^= ZOBRIST.piece_square[PIECE_INDEX[rook+6]][rook_to];
		}
	}
	// promotions
	if (m.flags & 4) {
		Piece promoted = (Piece)m.promotion;
		squares[m.to] = promoted;
	}
	// update castling rights if king/rook moved or rook captured
	if (moving==WK) castling_rights &= ~(1|2);
	if (moving==BK) castling_rights &= ~(4|8);
	if (m.from==idx(0,0) || m.to==idx(0,0)) castling_rights &= ~2;
	if (m.from==idx(0,7) || m.to==idx(0,7)) castling_rights &= ~1;
	if (m.from==idx(7,0) || m.to==idx(7,0)) castling_rights &= ~8;
	if (m.from==idx(7,7) || m.to==idx(7,7)) castling_rights &= ~4;
	// double pawn push sets ep
	if (abs_piece(moving)==1 && std::abs(rank_of(m.to)-rank_of(m.from))==2) {
		int mid = idx((rank_of(m.to)+rank_of(m.from))/2, file_of(m.to));
		ep_square = mid;
	}
	// halfmove clock
	if (abs_piece(moving)==1 || captured_out!=EMPTY) halfmove_clock = 0; else ++halfmove_clock;
	if (!white_to_move) ++fullmove_number;
	white_to_move = !white_to_move;

	// hash in
	if (squares[m.to] != EMPTY) hash ^= ZOBRIST.piece_square[PIECE_INDEX[squares[m.to]+6]][m.to];
	hash ^= ZOBRIST.castling[castling_rights & 15];
	if (ep_square >= 0) hash ^= ZOBRIST.ep_file[file_of(ep_square)];
	// toggle side-to-move exactly once
	hash ^= ZOBRIST.black_to_move;
}

void Board::unmake_move(const Move &m, Piece captured, uint8_t old_castle, int8_t old_ep, uint16_t old_half) {
	// Recompute fully for safety and simplicity
	// (This is less efficient but still fine for MCTS here.)
	// We'll snapshot the board before move via make_move caller data.
	// To simplify, we revert naively by reconstructing from scratch is expensive.
	// Instead, perform inverse of make with stored data.
	white_to_move = !white_to_move;
	Piece moving = squares[m.to];
	// handle promotion
	if (m.flags & 4) {
		moving = (is_white(moving)?WP:BP);
	}
	// move back
	squares[m.from] = moving;
	squares[m.to] = captured;
	// en-passant restoration
	if (m.flags & 2) {
		int dir = white_to_move ? 1 : -1; // because we toggled
		int cap_sq = idx(rank_of(m.to)+dir, file_of(m.to));
		squares[cap_sq] = white_to_move ? BP : WP;
		squares[m.to] = EMPTY;
	}
	// castle undo
	if ((abs_piece(moving)==6) && (m.flags & 1)) {
		if (file_of(m.to)==6) {
			int r = white_to_move?0:7; int rook_from = idx(r,7), rook_to = idx(r,5);
			Piece rook = squares[rook_to]; squares[rook_from]=rook; squares[rook_to]=EMPTY;
		} else if (file_of(m.to)==2) {
			int r = white_to_move?0:7; int rook_from = idx(r,0), rook_to = idx(r,3);
			Piece rook = squares[rook_to]; squares[rook_from]=rook; squares[rook_to]=EMPTY;
		}
	}
	castling_rights = old_castle;
	ep_square = old_ep;
	halfmove_clock = old_half;
	if (white_to_move) --fullmove_number;
	update_hash();
}

GameResult Board::evaluate_terminal() const {
	auto moves = generate_legal_moves();
	if (!moves.empty()) return {0.0f,false};
	if (in_check(white_to_move)) {
		return {white_to_move ? -1.0f : 1.0f, true};
	}
	if (halfmove_clock >= 100) return {0.0f,true};
	return {0.0f,true}; // stalemate
}

int Board::material_eval() const {
	static const int val[7] = {0,100,320,330,500,900,0};
	int s = 0;
	for (auto p : squares) {
		int a = abs_piece(p);
		if (a==6) continue;
		s += (is_white(p)?1:-1) * val[a];
	}
	return s;
}