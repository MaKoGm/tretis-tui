#pragma once
#include "board.hpp"
#include <deque>
#include <random>

class Game
{
public:
    Board board;
    Tetromino active_piece;
    int active_x, active_y;
    int score;
    int lock_delay_count;
    int total_lines;
    int level;

    std::array<int, 7> bag;
    int bag_index;
    std::deque<Tetromino> next_queue;
    Tetromino hold_piece;
    bool has_held;
    bool can_hold;

    bool is_running;
    bool is_paused;
    bool needs_restart;
    bool force_render;

    std::mt19937 rng;

    Game();
    void fill_queue();
    void spawn_piece();
    void hold_piece_action();
    void lock_and_advance();
    void tick();
    void process_input(char key);
    void rotate_piece();
    long long get_drop_speed_ms() const;
};