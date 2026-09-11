#pragma once
#include "../core/game.hpp"
#include <termios.h>

class TerminalUI
{
private:
    struct termios orig_termios;
    int orig_fcntl;

    std::string get_padding() const;
    std::string get_block_str(int cell) const;
    std::string pad_center(const std::string &text, int width) const;
    std::string render_piece_row(const Tetromino &p, int y) const;

public:
    TerminalUI();
    ~TerminalUI();

    char get_input();
    void render(Game &game);
    void print_game_over();
    void restore_blocking();
    void flush_input();
};