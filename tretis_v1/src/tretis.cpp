#include <deque>
#include <algorithm>
#include <array>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <termios.h>
#include <unistd.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>
#include <atomic>

// Flag global para manejar un cierre seguro con Ctrl+C
volatile sig_atomic_t g_quit = 0;

void handle_sigint(int signum)
{
    g_quit = 1;
}

constexpr int ROWS = 20;
constexpr int COLS = 10;
constexpr int HIDDEN_ROWS = 0;
constexpr int COLOR_INDEX = 8;
constexpr std::array<int, 5> score_table = {0, 100, 300, 500, 800};

using Tetromino = std::array<int, 9>;

const Tetromino FIG_Z = {-1, -1, -1, 0, 0, 0, 0, 1, 1};
const Tetromino FIG_T = {-1, 0, 0, -1, 0, 0, 0, 1, 2};
const Tetromino FIG_S = {-1, 0, -1, 1, 0, -1, 0, 0, 3};
const Tetromino FIG_O = {-1, 0, -1, 1, 0, 0, 0, 1, 4};
const Tetromino FIG_J = {-1, -1, 0, -1, 0, 0, 0, 1, 5};
const Tetromino FIG_L = {-1, 1, 0, -1, 0, 0, 0, 1, 6};
const Tetromino FIG_I = {0, -1, 0, 0, 0, 1, 0, 2, 7};

// ============================================================================
// CLASE REPRODUCTOR DE MÚSICA
// ============================================================================
class MusicPlayer
{
private:
    pid_t pid;

public:
    MusicPlayer(const std::string &filepath) : pid(-1)
    {
        pid = fork();
        if (pid == 0)
        {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull != -1)
            {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            execlp("mpv", "mpv", "--no-video", "--loop=inf", "--quiet", filepath.c_str(), nullptr);
            execlp("ffplay", "ffplay", "-nodisp", "-loop", "0", "-loglevel", "quiet", filepath.c_str(), nullptr);
            exit(1);
        }
    }

    ~MusicPlayer()
    {
        if (pid > 0)
        {
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
        }
    }
};

// ============================================================================
// CLASE TABLERO
// ============================================================================
class Board
{
public:
    std::array<int, ROWS * COLS> data;

    Board() { data.fill(0); }

    bool check_collision(int pos_x, int pos_y, const Tetromino &figure) const
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

    void lock_piece(int pos_x, int pos_y, const Tetromino &figure)
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

    int clear_lines()
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
                    auto source_begin = data.begin() + (r - 2) * COLS;
                    auto source_end = data.begin() + (r - 1) * COLS;
                    auto dest_begin = data.begin() + (r - 1) * COLS;
                    std::copy(source_begin, source_end, dest_begin);
                }
                std::fill(data.begin(), data.begin() + COLS, 0);
                lines_cleared++;
                cleared_in_pass = true;
            }
        } while (cleared_in_pass);

        return lines_cleared;
    }
};

// ============================================================================
// CLASE JUEGO
// ============================================================================
class Game
{
private:
    Board board;
    Tetromino active_piece;
    int active_x, active_y;
    int score;
    int lock_delay_count;
    int total_lines;
    int level;

    std::array<int, 7> bag = {0, 1, 2, 3, 4, 5, 6};
    int bag_index = 7;
    std::deque<Tetromino> next_queue;
    Tetromino hold_piece;
    bool has_held = false;
    bool can_hold = true;

    struct termios orig_termios;
    int orig_fcntl;

    std::mt19937 rng;

    void init_terminal()
    {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        orig_fcntl = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, orig_fcntl | O_NONBLOCK);
    }

public:
    bool is_running;
    bool is_paused;
    bool needs_restart;
    bool force_render; // Nueva flag para optimizar cuándo pintar

    Game() : score(0), lock_delay_count(0), rng(std::random_device{}()),
             is_running(true), is_paused(false), needs_restart(false),
             level(1), total_lines(0), force_render(true)
    {
        init_terminal();
        spawn_piece();
    }

    ~Game()
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        fcntl(STDIN_FILENO, F_SETFL, orig_fcntl);
    }

    void fill_queue()
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

    void spawn_piece()
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

    void hold_piece_action()
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

    void lock_and_advance()
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

    void tick()
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

    void process_input(char key)
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

    void rotate_piece()
    {
        if (active_piece[COLOR_INDEX] == 4)
            return; // Figura 'O'
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

    char get_input()
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

    // Calcula el tiempo entre caídas en milisegundos (mejor que ticks abstractos)
    long long get_drop_speed_ms() const
    {
        return std::max(100LL, 1000LL - ((level - 1) * 80LL));
    }

    // ... (Mantengo tu lógica de strings para padding y render_piece_row igual) ...
    std::string get_padding() const
    {
        int term_width = 80;
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1)
            term_width = w.ws_col;
        return std::string(std::max(0, (term_width - 72) / 2), ' ');
    }

    std::string get_block_str(int cell)
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

    std::string pad_center(const std::string &text, int width)
    {
        int padding = width - text.length();
        if (padding <= 0)
            return text;
        int left = padding / 2;
        return std::string(left, ' ') + text + std::string(padding - left, ' ');
    }

    std::string render_piece_row(const Tetromino &p, int y)
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

    void render()
    {
        if (!force_render)
            return; // Solo pintamos si hubo un cambio
        force_render = false;

        std::cout << "\033[H\033[?25l";
        std::string pad = get_padding();

        std::array<int, ROWS * COLS> display_buffer = board.data;

        // Pieza fantasma
        int ghost_y = active_y;
        while (!board.check_collision(active_x, ghost_y + 1, active_piece))
            ghost_y++;
        for (size_t i = 0; i < active_piece.size() - 1; i += 2)
        {
            int r = ghost_y + active_piece[i], c = active_x + active_piece[i + 1];
            if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
            {
                if (display_buffer[r * COLS + c] == 0)
                    display_buffer[r * COLS + c] = -active_piece[COLOR_INDEX];
            }
        }

        // Pieza activa
        int color = active_piece[COLOR_INDEX];
        for (size_t i = 0; i < active_piece.size() - 1; i += 2)
        {
            int r = active_y + active_piece[i], c = active_x + active_piece[i + 1];
            if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
                display_buffer[r * COLS + c] = color;
        }

        Tetromino empty = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        // Usamos std::array (memoria en el stack) en lugar de std::vector
        std::array<std::string, ROWS> left;
        left.fill(std::string(20, ' ')); // Rellenamos con espacios por defecto

        left[0] = "      HOLD (C)      ";
        left[1] = " +----------------+ ";
        left[2] = " |                | ";
        left[3] = " |  " + render_piece_row(has_held ? hold_piece : empty, -1) + "  | ";
        left[4] = " |  " + render_piece_row(has_held ? hold_piece : empty, 0) + "  | ";
        left[5] = " |                | ";
        left[6] = " +----------------+ ";
        left[8] = "       STATS        ";
        left[9] = " +----------------+ ";
        left[10] = " | LEVEL          | ";
        left[11] = " | " + pad_center(std::to_string(level), 14) + " | ";
        left[12] = " | LINES          | ";
        left[13] = " | " + pad_center(std::to_string(total_lines), 14) + " | ";
        left[14] = " +----------------+ ";

        std::array<std::string, ROWS> right;
        right.fill(std::string(20, ' '));

        right[0] = "        NEXT        ";
        right[1] = " +----------------+ ";
        right[2] = " |                | ";
        right[3] = " |  " + render_piece_row(next_queue[0], -1) + "  | ";
        right[4] = " |  " + render_piece_row(next_queue[0], 0) + "  | ";
        right[5] = " |                | ";
        right[6] = " |  " + render_piece_row(next_queue[1], -1) + "  | ";
        right[7] = " |  " + render_piece_row(next_queue[1], 0) + "  | ";
        right[8] = " |                | ";
        right[9] = " |  " + render_piece_row(next_queue[2], -1) + "  | ";
        right[10] = " |  " + render_piece_row(next_queue[2], 0) + "  | ";
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

        std::string raw_title = "T E T R I S | SCORE: " + std::to_string(score);
        int title_padding = std::max(0, (32 - (int)raw_title.length()) / 2);

        std::cout << pad << std::string(20, ' ') << std::string(title_padding, ' ')
                  << "\033[36mT E T R I S\033[0m | SCORE: " << score << "\033[K\n";
        std::cout << pad << std::string(20, ' ') << "+------------------------------+\033[K\n";

        for (int i = HIDDEN_ROWS; i < ROWS; i++)
        {
            std::cout << pad << left[i - HIDDEN_ROWS] << "|";
            if (is_paused && i >= 6 && i <= 12)
                std::cout << "\033[38;5;222m" << pause_menu[i - 6] << "\033[0m";
            else
            {
                for (int j = 0; j < COLS; j++)
                    std::cout << get_block_str(display_buffer[i * COLS + j]);
            }
            std::cout << "|" << right[i - HIDDEN_ROWS] << "\033[K\n";
        }
        std::cout << pad << std::string(20, ' ') << "+------------------------------+\033[K\n";
        std::cout << pad << "     [//] Move   [] Rotate   [Space] Drop   [C] Hold   [Esc] Pause     \033[K\n";
    }

    void print_game_over()
    {
        std::string pad = get_padding();
        std::cout << "\n"
                  << pad << "                    +------------------------------+\n";
        std::cout << pad << "                    |          GAME OVER           |\n";
        std::cout << pad << "                    +------------------------------+\n";
    }
};

// ============================================================================
// BUCLE PRINCIPAL (Mejorado con std::chrono)
// ============================================================================
int main()
{
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    MusicPlayer bg_music("tetris.opus");
    char play_again = 'n';

    do
    {
        bool auto_restart = false;
        play_again = 'n';
        std::cout << "\033[2J";

        {
            Game tetris;
            auto last_tick = std::chrono::steady_clock::now();
            tetris.render();

            while (tetris.is_running && !g_quit)
            {
                char key;
                while ((key = tetris.get_input()) != 0)
                {
                    tetris.process_input(key);
                }

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick).count();

                if (elapsed >= tetris.get_drop_speed_ms())
                {
                    tetris.tick();
                    last_tick = now;
                }

                tetris.render();
                usleep(5000);
            }

            if (g_quit)
                break;

            if (tetris.needs_restart)
            {
                auto_restart = true;
            }
            else
            {
                tetris.print_game_over();
            }
        } // <--- AQUÍ SE DESTRUYE 'tetris' Y SE RESTAURA LA TERMINAL AL 100%

        if (auto_restart)
        {
            play_again = 'y';
            tcflush(STDIN_FILENO, TCIFLUSH);
        }
        else if (!g_quit)
        {
            // 1. Aseguramos por completo que STDIN es bloqueante
            int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
            fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

            // 2. Descartamos teclas pulsadas al final de la partida
            tcflush(STDIN_FILENO, TCIFLUSH);

            // 3. Mostramos cursor para que el usuario vea dónde escribe
            std::cout << "\e[?25h";

            // Pausa breve para asimilar el Game Over
            sleep(1);

            // Centrado de la pregunta
            std::string pad;
            int term_width = 80;
            struct winsize w;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1)
                term_width = w.ws_col;
            pad = std::string(std::max(0, (term_width - 36) / 2), ' ');

            std::cout << "\n"
                      << pad << "¿Quieres jugar de nuevo? [y/N]: ";

            // 4. Limpiamos cualquier flag de error previo en cin y leemos
            std::cin.clear();
            if (!(std::cin >> play_again))
            {
                play_again = 'n';
            }
        }

    } while ((play_again == 'y' || play_again == 'Y') && !g_quit);

    std::cout << "\e[?25h"; // Restaura el cursor definitivamente

    return 0;
}