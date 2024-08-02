#include "minesweeper.hpp"


std::mt19937 minesweeper::generator(std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_real_distribution<double> minesweeper::distribution(0.0, 1.0);

std::vector<std::pair<int, int>> minesweeper::get_revealed_neighbour(const std::pair<int, int>& cell) const {
    auto& [x, y] = cell;
    std::vector<std::pair<int, int>> ans;
    for (int i = 0; i < 8; ++i) {
        if (is_valid({x + dx[i], y + dy[i]}) && revealed[x + dx[i]][y + dy[i]])
            ans.emplace_back(std::make_pair(x + dx[i], y + dy[i]));
    }
    return ans;
}

std::vector<std::pair<int, int>> minesweeper::get_unrevealed_neighbour(const std::pair<int, int>& cell) const {
    auto& [x, y] = cell;
    std::vector<std::pair<int, int>> ans;
    for (int i = 0; i < 8; ++i) {
        if (is_valid({x + dx[i], y + dy[i]}) && !revealed[x + dx[i]][y + dy[i]] && !flagged[x + dx[i]][y + dy[i]])
            ans.emplace_back(std::make_pair(x + dx[i], y + dy[i]));
    }
    return ans;
}

int minesweeper::count_adjacent_flag(const std::pair<int, int>& cell) const {
    auto& [x, y] = cell;
    int ans = 0;
    for (int i = 0; i < 8; ++i) {
        if (is_valid({x + dx[i], y + dy[i]}) && flagged[x + dx[i]][y + dy[i]])
            ans++;
    }
    return ans;
}

int minesweeper::count_adjacent_unrevealed(const std::pair<int, int>& cell) const {
    auto& [x, y] = cell;
    int ans = 0;
    for (int i = 0; i < 8; ++i) {
        if (is_valid({x + dx[i], y + dy[i]}) && !revealed[x + dx[i]][y + dy[i]] && !flagged[x + dx[i]][y + dy[i]])
            ans++;
    }
    return ans;
}

int minesweeper::count_adjacent_revealed(const std::pair<int, int>& cell) const {
    auto& [x, y] = cell;
    int ans = 0;
    for (int i = 0; i < 8; ++i) {
        if (is_valid({x + dx[i], y + dy[i]}) && revealed[x + dx[i]][y + dy[i]] )
            ans++;
    }
    return ans;
}

int minesweeper::count_adjacent_bomb(const std::pair<int, int>& cell) const {
    auto& [x, y] = cell;
    int ans = 0;
    for (int i = 0; i < 8; ++i) {
        if (is_valid({x + dx[i], y + dy[i]}) && has_bomb[x + dx[i]][y + dy[i]] )
            ans++;
    }
    return ans;
}

void minesweeper::generate_mines() {
    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            has_bomb[i][j] = distribution(generator) < mine_density ? 1 : 0;
            bomb_remaining += has_bomb[i][j];
            safe_remaining += 1 - has_bomb[i][j];
        }
    }

    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            adjacent_bomb_count[i][j] = count_adjacent_bomb({i, j});
        }
    }

}

std::vector<std::pair<int, int>> minesweeper::reveal_around(const std::pair<int, int>& cell) {
    auto& [x, y] = cell;
    std::vector<std::pair<int, int>> cell_revealed;
    for (int i = std::max(0, x - 1); i <= std::min(x + 1, cols - 1); ++i) {
        for (int j = std::max(0, y - 1); j <= std::min(y + 1, rows - 1); ++j) {
            if ( (i == x && j == y) || revealed[i][j]) 
                continue;
            auto other = reveal_all({i, j});
            cell_revealed.insert(
                cell_revealed.end(),
                std::make_move_iterator(other.begin()),
                std::make_move_iterator(other.end())
            );
        }
    } 
    return cell_revealed;
}


minesweeper::minesweeper(const int& _rows, const int& _cols, const double& _density) 
: rows{_rows}, cols{_cols}, mine_density{_density},
    flagged{std::vector<std::vector<bool>>(cols, std::vector<bool>(rows, 0))}, 
    has_bomb{std::vector<std::vector<bool>>(cols, std::vector<bool>(rows, 0))},
    revealed{std::vector<std::vector<bool>>(cols, std::vector<bool>(rows, 0))},
    adjacent_bomb_count{std::vector<std::vector<int>>(cols, std::vector<int>(rows, 0))} {

    generate_mines();
}

const bool minesweeper::is_valid(std::pair<int, int> cell) const {
    auto& [x, y] = cell;
    return x >= 0 && x < cols && y >= 0 && y < rows;
}

std::vector<std::pair<int, int>> minesweeper::reveal_all(const std::pair<int, int>& cell) {
    auto& [x, y] = cell;
    std::vector<std::pair<int, int>> res{};

    if (flagged[x][y] || !is_valid(cell)) {
        return {};
    } else if (revealed[x][y] && count_adjacent_flag({x, y}) == get_adjacent_bomb_count({x, y}) && count_adjacent_unrevealed({x, y}) > 0) {
        res = std::move(reveal_around({x, y}));
        return res;
    } else if (revealed[x][y]) {
        return {};
    }

    revealed[x][y] = true;
    safe_remaining--;
    if (auto it = next_to_revealed.find(cell); it != next_to_revealed.end()) 
        next_to_revealed.erase(it);

    for (int i = 0; i < 8; ++i) {
        if (is_valid({x + dx[i], y + dy[i]}) && !revealed[x + dx[i]][y + dy[i]] && !flagged[x + dx[i]][y + dy[i]])
            next_to_revealed.insert({x + dx[i], y + dy[i]});
    }
    res.emplace_back(std::move(cell));

    if (has_bomb[x][y]) {
        game_over = true;
        return res;
    }

    if (get_adjacent_bomb_count({x, y}) == 0) {
        for (int i = 0; i < 8; ++i) {
            if (!is_valid({x + dx[i], y + dy[i]}) || revealed[x + dx[i]][y + dy[i]])
                continue;
            auto other = reveal_all({x + dx[i], y + dy[i]});
            res.insert(
                res.end(),
                std::make_move_iterator(other.begin()),
                std::make_move_iterator(other.end())
            );
        }
    }

    return res;
}

std::vector<minesweeper::Hint> minesweeper::get_hint() const {
    std::unordered_map<std::pair<int, int>, int, pair_hash> total_count, count, delta, remaining_space;
    std::unordered_map<std::pair<int, int>, bool, pair_hash> vis, vis_local, curr;
    std::vector<minesweeper::Hint> ans;

    // Return all cell that share a same revealed cell along the current path
    auto get_connected_cell = [&]() {
        std::vector<std::pair<int, int>> ans;
        std::unordered_map<std::pair<int, int>, int, pair_hash> m;
        for (auto& k : curr) {
            if (!k.second)
                continue;
            for (auto& i : get_revealed_neighbour(k.first)) {
                for (auto& j : get_unrevealed_neighbour(i)) {
                    if (curr[j])
                        continue;
                    if (m.find(j) == m.end())
                        m[j] = abs(k.first.first - j.first) + abs(k.first.second - j.second);
                    else 
                        m[j] = std::min(m[j], abs(k.first.first - j.first) + abs(k.first.second - j.second));
                }
            }
        }
        std::vector<std::pair<std::pair<int, int>, int>> tmp { m.begin(), m.end() };

        std::sort(tmp.begin(), tmp.end(), [](const auto& lhs, const auto& rhs){
            return lhs.second < rhs.second;
        });

        for (auto& i : tmp) {
            ans.emplace_back(i.first);
        }

        return ans;
    };

    std::function<int(const std::pair<int, int>&, int)> dfs;

    dfs = [&](const std::pair<int, int>& node, int level=0) -> bool {
        if (level > MAX_RECURSION_DEPTH) 
            return true;

        bool ans = false;
        curr[node] = true;
        vis[node] = true;
        auto nbrs = get_revealed_neighbour(node); 
        if (count.find(node) == count.end())
            count[node] = 0;
        std::vector<std::pair<int, int>> connected_cell = get_connected_cell();

        bool can_be_bomb = true;
        bool can_be_safe = true;
        bool at_tail = true;
        bool trivial_safe = false;
        bool trivial_mine = false;

        for (auto& i : nbrs) {
            if (remaining_space.find(i) == remaining_space.end())
                remaining_space[i] = count_adjacent_unrevealed(i);
            trivial_safe |= (get_adjacent_bomb_count(i) + delta[i] - count_adjacent_flag(i) == 0);
            trivial_mine |= (get_adjacent_bomb_count(i) + delta[i] - count_adjacent_flag(i) == remaining_space[i] && remaining_space[i] == 1);

            remaining_space[i]--;

            can_be_bomb &= (get_adjacent_bomb_count(i) + delta[i] - count_adjacent_flag(i) >= 1);
            can_be_safe &= (get_adjacent_bomb_count(i) + delta[i] - count_adjacent_flag(i) <= remaining_space[i]);
        }


        // Assume current cell contains a bomb
        if (can_be_bomb) {
            for (auto& i : nbrs)
                delta[i]--;

            for (auto& i : connected_cell) {
                if (curr[i]) 
                    continue;
                if (dfs(i, level + 1)) {
                    ans |= true;
                    total_count[node]++;
                }
                at_tail = false;
                break;
            }

            for (auto& i : nbrs)
                delta[i]++;
        } 
        
        // Assume current cell contains no bomb
        if (can_be_safe) {
            for (auto& i : connected_cell) {
                if (curr[i]) 
                    continue;
                if (dfs(i, level + 1)) {
                    ans |= true;
                    count[node]++;
                    total_count[node]++;
                }
                at_tail = false;
                break;
            }
        }

        for (auto& i : nbrs) {
            remaining_space[i]++;
        }

        curr[node] = false;

        if (at_tail && (trivial_mine ^ trivial_safe)) {
            ans |= true;
            count[node] += trivial_safe;
            total_count[node]++;
        }

        return ans;
    };

    std::pair<int, int> highest_probability{0, 1};
    for (auto& i : next_to_revealed) {
        bool trivial_safe = false, trivial_mine = false;
        for (auto& j : get_revealed_neighbour(i)) {
            trivial_mine |= (get_adjacent_bomb_count(j) - count_adjacent_flag(j) == count_adjacent_unrevealed(j));
            trivial_safe |= (get_adjacent_bomb_count(j) - count_adjacent_flag(j) == 0);
        }
        if (trivial_mine || trivial_safe) {
            return {{i.first, i.second, trivial_safe ? HINT_TYPE::SAFE : HINT_TYPE::MINE}};
        }

    }
    for (auto& i : next_to_revealed) {
        if (!vis[i]) {
            dfs(i, 0);
            for (auto& [key, value] : total_count) {
                if (value == count[key]) {
                    return {{key.first, key.second, HINT_TYPE::SAFE}};
                } else if (count[key] == 0) {
                    return {{key.first, key.second, HINT_TYPE::MINE}};
                } else if (count[key] * highest_probability.second > highest_probability.first * value) {
                    highest_probability = std::make_pair(count[key], value);
                    ans = std::vector<minesweeper::Hint>{minesweeper::Hint{key.first, key.second, HINT_TYPE::HIGH_PROBABILITY}};
                } else if (count[key] * highest_probability.second == highest_probability.first * value) {
                    ans.emplace_back(minesweeper::Hint{key.first, key.second, HINT_TYPE::HIGH_PROBABILITY});
                }
            } 
            count.clear();
            total_count.clear();
            curr.clear();
        }
    }
    return ans;
} 

bool minesweeper::toggle_flag(const std::pair<int, int>& cell) {
    if (!revealed[cell.first][cell.second]) {
        flagged[cell.first][cell.second] = 1 - flagged[cell.first][cell.second];
        bomb_remaining += is_flagged(cell) ? -1 : 1;
        if (auto it = next_to_revealed.find(cell); it != next_to_revealed.end()) {
            next_to_revealed.erase(it);
        } else if (count_adjacent_revealed(cell) > 0){
            next_to_revealed.insert(cell);
        }
    }
    return flagged[cell.first][cell.second];
}

const minesweeper::GAME_STATUS minesweeper::get_game_status() const {
    if (game_over) 
        return GAME_STATUS::LOSE;
    else if (safe_remaining == 0)
        return GAME_STATUS::WIN;
    return GAME_STATUS::NEUTRAL;
}

bool minesweeper::is_flagged(const std::pair<int, int>& cell) const {
    return this->flagged[cell.first][cell.second];
}

const bool minesweeper::is_bomb(const std::pair<int, int>& cell) const {
    return has_bomb[cell.first][cell.second];
}

const int& minesweeper::get_adjacent_bomb_count(const std::pair<int, int>& cell) const {
    return adjacent_bomb_count[cell.first][cell.second];
}

const int& minesweeper::get_bomb_remaining() const {
    return bomb_remaining;
}

bool minesweeper::is_revealed(const std::pair<int, int>& cell) const {
    return revealed[cell.first][cell.second];
}
