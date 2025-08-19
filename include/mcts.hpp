#pragma once

#include "board.hpp"

struct MCTSNodeKey {
	uint64_t hash;
	bool operator==(const MCTSNodeKey &o) const { return hash==o.hash; }
};

struct MCTSNodeData {
	uint32_t visits;
	float value_sum; // from current player perspective at node creation
	std::vector<Move> moves;
	std::vector<uint32_t> child_visits;
	std::vector<float> child_values;
};

struct KeyHasher { size_t operator()(const MCTSNodeKey &k) const { return (size_t)k.hash; } };

class MCTS {
public:
	MCTS();
	Move search_best_move(Board &root, int simulations, float c_puct=1.4f);
	void set_time_budget_ms(int64_t ms);
	void enable_persistent_q(bool enabled);
	void load_qtable(const std::string &path);
	void save_qtable(const std::string &path);

private:
	std::unordered_map<MCTSNodeKey, MCTSNodeData, KeyHasher> table;
	std::unordered_map<uint64_t, std::pair<float,uint32_t>> qtable; // hash->(value_sum,visits)
	int64_t time_budget_ms;
	bool persistent_q;

	float simulate(Board &b);
	float playout(Board &b);
};

