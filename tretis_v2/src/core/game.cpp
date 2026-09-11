#include "game.hpp"
#include <algorithm>

Game::Game() : score(0), lock_delay_count(0), rng(std::random_device{}()),
               is_running(true), is_paused(false), needs_restart(false),
               level(1), total_lines(0), force_render(true),
               bag({0, 1, 2, 3, 4, 5, 6}), bag_index(7), has_held(false), can_hold(true)
{
    spawn_piece();
}

void Game::fill_queue()
{
    const std::array<Tetromino, 7> catalog = {FIG_Z, FIG_T, FIG_S, FIG_O, FIG_J, FIG_L, FIG_I};
    while (next_queue.size() < 3)
    {
        if (bag_index >= 7)
        {
            std::shuffle(bag.begin(), bag.end(), rng);
            bag_index = 0;
        }
        next_queue.push_back(catalog[bag[bag_index]]);
        bag_index++;
    }
}

void Game::spawn_piece()
{
    if (next_queue.empty())
        fill_queue();
    active_piece = next_queue.front();
    next_queue.pop_front();
    fill_queue();
    active_x = 4;
    active_y = HIDDEN_ROWS;
    can_hold = true;
}

void Game::hold_piece_action()
{
    if (!can_hold)
        return;
    if (!has_held)
    {
        hold_piece = active_piece;
        has_held = true;
        spawn_piece();
    }
    else
    {
        std::swap(active_piece, hold_piece);
        active_x = 4;
        active_y = HIDDEN_ROWS;
    }
    can_hold = false;
    lock_delay_count = 0;
    force_render = true;
}

void Game::lock_and_advance()
{
    board.lock_piece(active_x, active_y, active_piece);
    int lines_cleared = board.clear_lines();
    if (lines_cleared > 0)
    {
        total_lines += lines_cleared;
        score += score_table[lines_cleared] * level;
        level = (total_lines / 10) + 1;
    }
    spawn_piece();
    lock_delay_count = 0;
    force_render = true;
    if (board.check_collision(active_x, active_y, active_piece))
    {
        is_running = false;
    }
}

void Game::tick()
{
    if (is_paused)
        return;
    if (!board.check_collision(active_x, active_y + 1, active_piece))
    {
        active_y++;
        lock_delay_count = 0;
        force_render = true;
    }
    else
    {
        lock_delay_count++;
        if (lock_delay_count >= 2)
        {
            lock_and_advance();
        }
    }
}

void Game::process_input(char key)
{
    if (is_paused)
    {
        if (key == 'p' || key == 'P' || key == 27)
            is_paused = false;
        else if (key == 'r' || key == 'R')
        {
            is_running = false;
            needs_restart = true;
        }
        else if (key == 'q' || key == 'Q')
            is_running = false;
        force_render = true;
        return;
    }

    bool moved = false;
    if (key == 'p' || key == 'P' || key == 27)
    {
        is_paused = true;
        moved = true;
    }
    else if (key == 'q' || key == 'Q')
    {
        is_running = false;
    }
    else if (key == 'a' || key == 'h')
    {
        if (!board.check_collision(active_x - 1, active_y, active_piece))
        {
            active_x--;
            moved = true;
            lock_delay_count = 0;
        }
    }
    else if (key == 'd' || key == 'l')
    {
        if (!board.check_collision(active_x + 1, active_y, active_piece))
        {
            active_x++;
            moved = true;
            lock_delay_count = 0;
        }
    }
    else if (key == 's' || key == 'j')
    {
        if (!board.check_collision(active_x, active_y + 1, active_piece))
        {
            active_y++;
            score++;
            moved = true;
            lock_delay_count = 0;
        }
    }
    else if (key == 'w' || key == 'k')
    {
        rotate_piece();
        moved = true;
        lock_delay_count = 0;
    }
    else if (key == ' ')
    {
        while (!board.check_collision(active_x, active_y + 1, active_piece))
        {
            active_y++;
            score += 2;
        }
        lock_and_advance();
        moved = true;
    }
    else if (key == 'c' || key == 'C')
    {
        hold_piece_action();
        moved = true;
    }

    if (moved)
        force_render = true;
}

void Game::rotate_piece()
{
    if (active_piece[COLOR_INDEX] == 4)
        return; // Figura O
    Tetromino rotated = active_piece;
    for (size_t i = 0; i < 8; i += 2)
    {
        rotated[i] = active_piece[i + 1];
        rotated[i + 1] = -active_piece[i];
    }
    const int kicks[6][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}};
    for (int i = 0; i < 6; i++)
    {
        int test_x = active_x + kicks[i][0];
        int test_y = active_y + kicks[i][1];
        if (!board.check_collision(test_x, test_y, rotated))
        {
            active_piece = rotated;
            active_x = test_x;
            active_y = test_y;
            return;
        }
    }
}

long long Game::get_drop_speed_ms() const
{
    return std::max(100LL, 1000LL - ((level - 1) * 80LL));
}