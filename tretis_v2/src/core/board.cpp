#include "board.hpp"
#include <algorithm>

Board::Board() { data.fill(0); }

bool Board::check_collision(int pos_x, int pos_y, const Tetromino &figure) const
{
    for (size_t i = 0; i < figure.size() - 1; i += 2)
    {
        int target_row = pos_y + figure[i];
        int target_col = pos_x + figure[i + 1];
        if (target_col < 0 || target_col >= COLS || target_row >= ROWS)
            return true;
        if (target_row < 0)
            continue;
        if (data[target_row * COLS + target_col] != 0)
            return true;
    }
    return false;
}

void Board::lock_piece(int pos_x, int pos_y, const Tetromino &figure)
{
    int color = figure[COLOR_INDEX];
    for (size_t i = 0; i < figure.size() - 1; i += 2)
    {
        int target_row = pos_y + figure[i];
        int target_col = pos_x + figure[i + 1];
        if (target_row >= 0 && target_row < ROWS)
        {
            data[target_row * COLS + target_col] = color;
        }
    }
}

int Board::clear_lines()
{
    int lines_cleared = 0;
    bool cleared_in_pass;
    do
    {
        cleared_in_pass = false;
        std::array<int, COLS> buffer;
        int row_to_clear = -1;

        for (int r = ROWS; r > 0; r--)
        {
            std::copy(data.begin() + (r - 1) * COLS, data.begin() + r * COLS, buffer.begin());
            if (std::all_of(buffer.begin(), buffer.end(), [](int cell)
                            { return cell != 0; }))
            {
                row_to_clear = r;
                break;
            }
        }

        if (row_to_clear != -1)
        {
            for (int r = row_to_clear; r > 1; r--)
            {
                std::copy(data.begin() + (r - 2) * COLS, data.begin() + (r - 1) * COLS, data.begin() + (r - 1) * COLS);
            }
            std::fill(data.begin(), data.begin() + COLS, 0);
            lines_cleared++;
            cleared_in_pass = true;
        }
    } while (cleared_in_pass);

    return lines_cleared;
}
