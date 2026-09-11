#include "terminal_ui.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

TerminalUI::TerminalUI()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    orig_fcntl = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, orig_fcntl | O_NONBLOCK);
}

TerminalUI::~TerminalUI()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    fcntl(STDIN_FILENO, F_SETFL, orig_fcntl);
}

void TerminalUI::restore_blocking()
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

void TerminalUI::flush_input()
{
    tcflush(STDIN_FILENO, TCIFLUSH);
}

char TerminalUI::get_input()
{
    char ch = 0;
    int c = getchar();
    if (c != EOF)
    {
        ch = (char)c;
        if (ch == '\033')
        {
            int c2 = getchar();
            if (c2 == EOF)
                return 27;
            int c3 = getchar();
            if (c2 == '[' && c3 != EOF)
            {
                if (c3 == 'A')
                    ch = 'w';
                if (c3 == 'B')
                    ch = 's';
                if (c3 == 'C')
                    ch = 'd';
                if (c3 == 'D')
                    ch = 'a';
            }
        }
    }
    return ch;
}

std::string TerminalUI::get_padding() const
{
    int term_width = 80;
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1)
        term_width = w.ws_col;
    return std::string(std::max(0, (term_width - 72) / 2), ' ');
}

std::string TerminalUI::get_block_str(int cell) const
{
    switch (cell)
    {
    case 0:
        return " . ";
    case 1:
        return "\033[31m # \033[0m";
    case 2:
        return "\033[35m # \033[0m";
    case 3:
        return "\033[32m # \033[0m";
    case 4:
        return "\033[38;5;222m # \033[0m";
    case 5:
        return "\033[34m # \033[0m";
    case 6:
        return "\033[38;5;208m # \033[0m";
    case 7:
        return "\033[36m # \033[0m";
    case -1:
        return "\033[31m[ ]\033[0m";
    case -2:
        return "\033[35m[ ]\033[0m";
    case -3:
        return "\033[32m[ ]\033[0m";
    case -4:
        return "\033[38;5;222m[ ]\033[0m";
    case -5:
        return "\033[34m[ ]\033[0m";
    case -6:
        return "\033[38;5;208m[ ]\033[0m";
    case -7:
        return "\033[36m[ ]\033[0m";
    }
    return "   ";
}

std::string TerminalUI::pad_center(const std::string &text, int width) const
{
    int padding = width - text.length();
    if (padding <= 0)
        return text;
    int left = padding / 2;
    return std::string(left, ' ') + text + std::string(padding - left, ' ');
}

std::string TerminalUI::render_piece_row(const Tetromino &p, int y) const
{
    std::string res = "";
    int color = p[COLOR_INDEX];
    if (color == 0)
        return "            ";
    for (int x = -1; x <= 2; x++)
    {
        bool is_block = false;
        for (size_t i = 0; i < p.size() - 1; i += 2)
        {
            if (p[i] == y && p[i + 1] == x)
            {
                is_block = true;
                break;
            }
        }
        res += is_block ? get_block_str(color) : "   ";
    }
    return res;
}

void TerminalUI::render(Game &game)
{
    if (!game.force_render)
        return;
    game.force_render = false;

    std::cout << "\033[H\033[?25l";
    std::string pad = get_padding();
    std::array<int, ROWS * COLS> display_buffer = game.board.data;

    // Ghost piece
    int ghost_y = game.active_y;
    while (!game.board.check_collision(game.active_x, ghost_y + 1, game.active_piece))
        ghost_y++;
    for (size_t i = 0; i < game.active_piece.size() - 1; i += 2)
    {
        int r = ghost_y + game.active_piece[i], c = game.active_x + game.active_piece[i + 1];
        if (r >= 0 && r < ROWS && c >= 0 && c < COLS && display_buffer[r * COLS + c] == 0)
        {
            display_buffer[r * COLS + c] = -game.active_piece[COLOR_INDEX];
        }
    }

    // Active piece
    int color = game.active_piece[COLOR_INDEX];
    for (size_t i = 0; i < game.active_piece.size() - 1; i += 2)
    {
        int r = game.active_y + game.active_piece[i], c = game.active_x + game.active_piece[i + 1];
        if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
            display_buffer[r * COLS + c] = color;
    }

    Tetromino empty = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<std::string, ROWS> left;
    left.fill(std::string(20, ' '));
    left[0] = "      HOLD (C)      ";
    left[1] = " +----------------+ ";
    left[2] = " |                | ";
    left[3] = " |  " + render_piece_row(game.has_held ? game.hold_piece : empty, -1) + "  | ";
    left[4] = " |  " + render_piece_row(game.has_held ? game.hold_piece : empty, 0) + "  | ";
    left[5] = " |                | ";
    left[6] = " +----------------+ ";
    left[8] = "       STATS        ";
    left[9] = " +----------------+ ";
    left[10] = " | LEVEL          | ";
    left[11] = " | " + pad_center(std::to_string(game.level), 14) + " | ";
    left[12] = " | LINES          | ";
    left[13] = " | " + pad_center(std::to_string(game.total_lines), 14) + " | ";
    left[14] = " +----------------+ ";

    std::array<std::string, ROWS> right;
    right.fill(std::string(20, ' '));
    right[0] = "        NEXT        ";
    right[1] = " +----------------+ ";
    right[2] = " |                | ";
    right[3] = " |  " + render_piece_row(game.next_queue[0], -1) + "  | ";
    right[4] = " |  " + render_piece_row(game.next_queue[0], 0) + "  | ";
    right[5] = " |                | ";
    right[6] = " |  " + render_piece_row(game.next_queue[1], -1) + "  | ";
    right[7] = " |  " + render_piece_row(game.next_queue[1], 0) + "  | ";
    right[8] = " |                | ";
    right[9] = " |  " + render_piece_row(game.next_queue[2], -1) + "  | ";
    right[10] = " |  " + render_piece_row(game.next_queue[2], 0) + "  | ";
    right[11] = " |                | ";
    right[12] = " +----------------+ ";

    std::array<std::string, 7> pause_menu = {
        "+--------- Paused -----------+",
        "|                            |",
        "|       || GAME PAUSED       |",
        "|                            |",
        "| Press [P] or [Esc] Resume  |",
        "| [R] Restart  |  [Q] Quit   |",
        "+----------------------------+"};

    std::string raw_title = "T E T R I S | SCORE: " + std::to_string(game.score);
    int title_padding = std::max(0, (32 - (int)raw_title.length()) / 2);

    std::cout << pad << std::string(20, ' ') << std::string(title_padding, ' ')
              << "\033[36mT E T R I S\033[0m | SCORE: " << game.score << "\033[K\n";
    std::cout << pad << std::string(20, ' ') << "+------------------------------+\033[K\n";

    for (int i = HIDDEN_ROWS; i < ROWS; i++)
    {
        std::cout << pad << left[i - HIDDEN_ROWS] << "|";
        if (game.is_paused && i >= 6 && i <= 12)
        {
            std::cout << "\033[38;5;222m" << pause_menu[i - 6] << "\033[0m";
        }
        else
        {
            for (int j = 0; j < COLS; j++)
                std::cout << get_block_str(display_buffer[i * COLS + j]);
        }
        std::cout << "|" << right[i - HIDDEN_ROWS] << "\033[K\n";
    }
    
    std::cout << pad << std::string(20, ' ') << "+------------------------------+\033[K\n\n";
    std::cout << pad << "     [←/→/↓] Move   [↑] Rotate   [Space] Drop   [C] Hold   [Esc] Pause     \033[K\n";
}

void TerminalUI::print_game_over()
{
    std::string pad = get_padding();
    std::cout << "\n"
              << pad << "                    +------------------------------+\n";
    std::cout << pad << "                    |          GAME OVER           |\n";
    std::cout << pad << "                    +------------------------------+\n";
}