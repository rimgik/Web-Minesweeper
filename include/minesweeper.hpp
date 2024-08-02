#pragma once

#include <mutex>
#include <math.h>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <functional>

class minesweeper final {
public:
    enum class GAME_STATUS { WIN, LOSE, NEUTRAL };

    enum class HINT_TYPE { SAFE, HIGH_PROBABILITY, MINE };

    struct Hint {
        const int x;
        const int y;
        HINT_TYPE hint;
    };


private:
    struct pair_hash {
        std::size_t operator() (const std::pair<int, int>& p) const {
            // Most significant bit up to 7th bit
            return std::hash<int>()((p.first << 7) | p.second);
        }
    };

    static std::uniform_real_distribution<double> distribution;
    static std::mt19937 generator;
    static constexpr int MAX_RECURSION_DEPTH = 25;
    static constexpr int MAX_DIMENSION = 255;
    static constexpr int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

    const int rows;
    const int cols;
    const double mine_density;
    std::vector<std::vector<bool>> has_bomb;
    std::vector<std::vector<bool>> revealed;
    std::vector<std::vector<bool>> flagged;
    std::vector<std::vector<int>>  adjacent_bomb_count;
    bool game_over{ false };
    int bomb_remaining{ 0 };
    int safe_remaining{ 0 };
    std::unordered_set<std::pair<int, int>, pair_hash> next_to_revealed;

    std::vector<std::pair<int, int>> get_revealed_neighbour(const std::pair<int, int>& cell) const;

    std::vector<std::pair<int, int>> get_unrevealed_neighbour(const std::pair<int, int>& cell) const;

    int count_adjacent_flag(const std::pair<int, int>& cell) const;

    int count_adjacent_unrevealed(const std::pair<int, int>& cell) const;

    int count_adjacent_revealed(const std::pair<int, int>& cell) const;

    int count_adjacent_bomb(const std::pair<int, int>& cell) const;

    void generate_mines();

    std::vector<std::pair<int, int>> reveal_around(const std::pair<int, int>& cell);

public:

    minesweeper(const int& _rows, const int& _cols, const double& _density);

    const bool is_valid(std::pair<int, int> cell) const;

    std::vector<std::pair<int, int>> reveal_all(const std::pair<int, int>& cell);

    std::vector<Hint> get_hint() const;

    bool toggle_flag(const std::pair<int, int>& cell);

    const GAME_STATUS get_game_status() const;

    bool is_flagged(const std::pair<int, int>& cell) const;

    const bool is_bomb(const std::pair<int, int>& cell) const;

    const int& get_adjacent_bomb_count(const std::pair<int, int>& cell) const;

    const int& get_bomb_remaining() const;

    bool is_revealed(const std::pair<int, int>& cell) const;

};

