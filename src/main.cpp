#include "mcts.hpp"

static void print_board(const Board &b) {
	for (int r=7;r>=0;--r) {
		for (int f=0; f<8; ++f) {
			Piece p = b.squares[idx(r,f)];
			char c='.';
			switch(p){
				case WP:c='P';break; case WN:c='N';break; case WB:c='B';break; case WR:c='R';break; case WQ:c='Q';break; case WK:c='K';break;
				case BP:c='p';break; case BN:c='n';break; case BB:c='b';break; case BR:c='r';break; case BQ:c='q';break; case BK:c='k';break;
				default:c='.';
			}
			std::cout<<c<<' ';
		}
		std::cout<<"\n";
	}
	std::cout<< (b.white_to_move?"White":"Black") <<" to move\n";
}

static int sq_from_algebraic(const std::string &s) {
	if (s.size()<2) return -1;
	int f = s[0]-'a'; int r = s[1]-'1';
	if (f<0||f>7||r<0||r>7) return -1; return idx(r,f);
}

int main(int argc, char** argv) {
	Board b = Board::startpos();
	MCTS mcts;
	mcts.enable_persistent_q(true);
	const std::string qfile = "data/qtable.txt";
	mcts.load_qtable(qfile);
	std::cout << "Type: play, train, or quit\n";
	std::string cmd;
	while (std::cin>>cmd) {
		if (cmd=="quit") break;
		if (cmd=="play") {
			b = Board::startpos();
			while (true) {
				print_board(b);
				GameResult g = b.evaluate_terminal();
				if (g.terminal) { std::cout<<"Game over score="<<g.reward<<"\n"; break; }
				if (b.white_to_move) {
					std::cout<<"Enter move like e2e4: "; std::string mv; if(!(std::cin>>mv)) return 0; if(mv.size()<4) continue; int from=sq_from_algebraic(mv.substr(0,2)); int to=sq_from_algebraic(mv.substr(2,2)); if(from<0||to<0) continue; auto ms=b.generate_legal_moves(); bool done=false; for(auto &m:ms){ if(m.from==from && m.to==to){ Piece cap; uint8_t oc; int8_t oe; uint16_t oh; b.make_move(m,cap,oc,oe,oh); done=true; break; } } if(!done) std::cout<<"Illegal\n";
				} else {
					Move best = mcts.search_best_move(b, 2000, 1.2f);
					Piece cap; uint8_t oc; int8_t oe; uint16_t oh; b.make_move(best,cap,oc,oe,oh);
				}
			}
		}
		if (cmd=="train") {
			int games = 1000; if (argc>1) games = std::atoi(argv[1]);
			int white_wins=0, black_wins=0, draws=0;
			for (int g=0; g<games; ++g) {
				b = Board::startpos();
				for (int ply=0; ply<1000; ++ply) {
					GameResult gr = b.evaluate_terminal(); if (gr.terminal) { if (gr.reward>0) ++white_wins; else if (gr.reward<0) ++black_wins; else ++draws; break; }
					Move mv = mcts.search_best_move(b, 64, 1.2f);
					Piece cap; uint8_t oc; int8_t oe; uint16_t oh; b.make_move(mv,cap,oc,oe,oh);
				}
			}
			std::cout<<"W:"<<white_wins<<" B:"<<black_wins<<" D:"<<draws<<"\n";
		}
		std::cout<<"Type: play, train, or quit\n";
	}
	mcts.save_qtable(qfile);
	return 0;
}