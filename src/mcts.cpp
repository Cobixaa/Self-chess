#include "mcts.hpp"
#include <chrono>

MCTS::MCTS() : time_budget_ms(0), persistent_q(true) {}

void MCTS::set_time_budget_ms(int64_t ms) { time_budget_ms = ms; }
void MCTS::enable_persistent_q(bool enabled) { persistent_q = enabled; }

void MCTS::load_qtable(const std::string &path) {
	qtable.clear();
	std::ifstream in(path);
	if (!in) return;
	uint64_t h; float s; uint32_t v;
	while (in >> h >> s >> v) qtable[h] = {s,v};
}

void MCTS::save_qtable(const std::string &path) {
	std::ofstream out(path);
	for (auto &kv : qtable) out << kv.first << ' ' << kv.second.first << ' ' << kv.second.second << '\n';
}

static inline float ucb_score(uint32_t parent_visits, uint32_t child_visits, float child_value, float c_puct) {
	if (child_visits == 0) return 1e9f; // avoid inf under -ffast-math
	float q = child_value / (float)child_visits;
	float u = c_puct * std::sqrt(std::max(1u, parent_visits)) / (1.0f + child_visits);
	return q + u;
}

Move MCTS::search_best_move(Board &root, int simulations, float c_puct) {
	auto deadline = time_budget_ms > 0 ? std::chrono::steady_clock::now() + std::chrono::milliseconds(time_budget_ms) : std::chrono::steady_clock::time_point::max();
	for (int i=0; i<simulations || std::chrono::steady_clock::now() < deadline; ++i) {
		Board b = root;
		float v = simulate(b);
		(void)v;
	}
	// pick move with max visits
	MCTSNodeKey k{root.hash};
	auto it = table.find(k);
	if (it == table.end() || it->second.moves.empty()) {
		auto legal = root.generate_legal_moves();
		if (legal.empty()) return Move{0,0,0,0};
		return legal[(size_t)(GLOBAL_RNG.uniform01()*legal.size())];
	}
	MCTSNodeData &node = it->second;
	uint32_t best_v = 0; size_t best_i = 0;
	for (size_t i=0;i<node.moves.size();++i) {
		if (node.child_visits[i] > best_v) { best_v = node.child_visits[i]; best_i = i; }
	}
	return node.moves[best_i];
}

float MCTS::simulate(Board &b) {
	std::vector<std::pair<MCTSNodeKey,size_t>> path;
	while (true) {
		MCTSNodeKey k{b.hash};
		auto tit = table.find(k);
		if (tit == table.end()) {
			// expand
			MCTSNodeData nd{};
			nd.visits = 0;
			nd.value_sum = 0.0f;
			nd.moves = b.generate_legal_moves();
			nd.child_visits.assign(nd.moves.size(), 0);
			nd.child_values.assign(nd.moves.size(), 0.0f);
			// seed from persistent q if enabled
			if (persistent_q) {
				auto itq = qtable.find(k.hash);
				if (itq != qtable.end()) {
					nd.value_sum = itq->second.first;
					nd.visits = itq->second.second;
				}
				// intentionally skip per-child seeding for speed
			}
			table.emplace(k, nd);
			float v = playout(b);
			// backprop
			for (auto it = path.rbegin(); it != path.rend(); ++it) {
				MCTSNodeData &n = table[it->first];
				n.visits++;
				n.value_sum += v;
				if (it->second != (size_t)-1) {
					n.child_visits[it->second]++;
					n.child_values[it->second] += v;
				}
				if (persistent_q) {
					auto &q = qtable[it->first.hash];
					q.first += v; // sum
					q.second += 1; // visits
				}
				v = -v; // switch perspective
			}
			return v;
		}
		MCTSNodeData &node = tit->second;
		if (node.moves.empty()) {
			// terminal
			GameResult g = b.evaluate_terminal();
			float v = g.reward;
			for (auto it = path.rbegin(); it != path.rend(); ++it) {
				MCTSNodeData &n = table[it->first];
				n.visits++;
				n.value_sum += v;
				if (it->second != (size_t)-1) {
					n.child_visits[it->second]++;
					n.child_values[it->second] += v;
				}
				if (persistent_q) {
					auto &q = qtable[it->first.hash];
					q.first += v;
					q.second += 1;
				}
				v = -v;
			}
			return v;
		}
		// select
		uint32_t parent_vis = std::max(1u, node.visits);
		float best = -1e9f; size_t best_i = 0;
		for (size_t i=0;i<node.moves.size();++i) {
			float sv = ucb_score(parent_vis, node.child_visits[i], node.child_values[i], 1.2f);
			if (node.child_visits[i] == 0) {
				sv += 0.001f * (float)GLOBAL_RNG.uniform01();
			}
			if (sv > best) { best = sv; best_i = i; }
		}
		// step
		Piece cap; uint8_t oc; int8_t oe; uint16_t oh; b.make_move(node.moves[best_i], cap, oc, oe, oh);
		path.emplace_back(k, best_i);
	}
}

float MCTS::playout(Board &b) {
	// Light playout with epsilon-greedy on material and random noise
	for (int depth=0; depth<192; ++depth) {
		GameResult g = b.evaluate_terminal();
		if (g.terminal) return g.reward;
		auto moves = b.generate_legal_moves();
		if (moves.empty()) return 0.0f;
		// choose move
		int best_score = std::numeric_limits<int>::min();
		int best_idx = -1;
		for (size_t i=0;i<moves.size();++i) {
			Board nb = b; Piece cap; uint8_t oc; int8_t oe; uint16_t oh; nb.make_move(moves[i], cap, oc, oe, oh);
			int s = nb.material_eval() * (nb.white_to_move ? -1 : 1);
			// little randomization to encourage exploration
			s += (int)((GLOBAL_RNG.uniform01()-0.5)*10);
			nb.unmake_move(moves[i], cap, oc, oe, oh);
			if (s > best_score) { best_score = s; best_idx = (int)i; }
		}
		if (best_idx < 0) best_idx = (int)(GLOBAL_RNG.uniform01()*moves.size());
		Piece cap; uint8_t oc; int8_t oe; uint16_t oh; b.make_move(moves[best_idx], cap, oc, oe, oh);
	}
	return 0.0f;
}