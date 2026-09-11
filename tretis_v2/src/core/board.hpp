#pragma once
#include <array>
#include "constants.hpp"
#include "tetromino.hpp"

class Board
{
public:
    std::array<int, ROWS * COLS> data;

    Board();
    bool check_collision(int pos_x, int pos_y, const Tetromino &figure) const;
    void lock_piece(int pos_x, int pos_y, const Tetromino &figure);
    int clear_lines();
};