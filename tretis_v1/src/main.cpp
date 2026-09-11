#include <algorithm>
#include <array>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <termios.h>
#include <unistd.h>

constexpr int ROWS = 20;
constexpr int COLS = 10;
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
        std::copy(data.begin() + (r - 1) * COLS, data.begin() + r * COLS,
                  buffer.begin());
        bool is_row_full = std::all_of(buffer.begin(), buffer.end(),
                                       [](int cell)
                                       { return cell != 0; });

        if (is_row_full)
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

  // Terminal original para restaurarla
  struct termios orig_termios;
  int orig_fcntl;

  // C++ Moderno: Generador de números aleatorios (Mersenne Twister)
  std::mt19937 rng;
  std::uniform_int_distribution<int> dist;

  void init_terminal()
  {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    orig_fcntl = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, orig_fcntl | O_NONBLOCK);
  }

  void restore_terminal()
  {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    fcntl(STDIN_FILENO, F_SETFL, orig_fcntl);
  }

public:
  bool is_running;

  Game() : score(0), lock_delay_count(0), rng(std::random_device{}()), dist(0, 6), is_running(true), level(1), total_lines(0)
  {
    init_terminal();
    spawn_piece(get_random_piece());
  }

  ~Game() { restore_terminal(); }

  Tetromino get_random_piece()
  {
    const std::array<Tetromino, 7> catalog = {FIG_Z, FIG_T, FIG_S, FIG_O,
                                              FIG_J, FIG_L, FIG_I};
    return catalog[dist(rng)];
  }

  void spawn_piece(const Tetromino &new_piece)
  {
    active_piece = new_piece;
    active_x = 4;
    active_y = 1;
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

    spawn_piece(get_random_piece());
    lock_delay_count = 0;

    if (board.check_collision(active_x, active_y, active_piece))
    {
      is_running = false;
    }
  }

  void
  tick()
  {
    if (!board.check_collision(active_x, active_y + 1, active_piece))
    {
      active_y++;
      lock_delay_count = 0;
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

  void hard_drop()
  {
    while (!board.check_collision(active_x, active_y + 1, active_piece))
    {
      active_y++;
      score += 2;
    }
    lock_and_advance();
  }

  void rotate_piece()
  {
    Tetromino rotated = active_piece;
    if (active_piece[COLOR_INDEX] == 4)
      return;

    for (size_t i = 0; i < 8; i += 2)
    {
      int r = active_piece[i];
      int c = active_piece[i + 1];

      rotated[i] = c;
      rotated[i + 1] = -r;
    }

    const int kicks[6][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}};

    for (int i = 0; i < 4; i++)
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

  void process_input(char key)
  {
    if (key == 'a' || key == 'h')
    {
      if (!board.check_collision(active_x - 1, active_y, active_piece))
      {
        active_x--;
      }
    }
    else if (key == 'd' || key == 'l')
    {
      if (!board.check_collision(active_x + 1, active_y, active_piece))
      {
        active_x++;
      }
    }
    else if (key == 's' || key == 'j')
    {
      if (!board.check_collision(active_x, active_y + 1, active_piece))
      {
        active_y++;
      }
      score++;
    }
    else if (key == 'w' || key == 'k')
    {
      rotate_piece();
    }
    else if (key == ' ')
    {
      hard_drop();
    }
  }

  int get_drop_speed() const
  {
    int speed = 100000 - ((level - 1) * 2000);
    return std::max(speed, 10000);
  }

  void render()
  {
    std::cout << "\033[H\033[J";
    std::cout << "\033[?25l"; // ocultar el cursor

    std::array<int, ROWS * COLS> display_buffer = board.data;

    int ghost_y = active_y;

    while (!board.check_collision(active_x, ghost_y + 1, active_piece))
    {
      ghost_y++;
    }

    // Proyectamos el fantasma en el buffer usando un "color" especial (ej: 9)
    for (size_t i = 0; i < active_piece.size() - 1; i += 2)
    {
      int r = ghost_y + active_piece[i];
      int c = active_x + active_piece[i + 1];
      if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
      {
        if (display_buffer[r * COLS + c] == 0)
        {
          display_buffer[r * COLS + c] = -active_piece[COLOR_INDEX];
        }
      }
    }

    int color = active_piece[COLOR_INDEX];
    for (size_t i = 0; i < active_piece.size() - 1; i += 2)
    {
      int r = active_y + active_piece[i];
      int c = active_x + active_piece[i + 1];
      if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
      {
        display_buffer[r * COLS + c] = color;
      }
    }

    std::cout << "TETRIS | SCORE: " << score
              << "\n+------------------------------+\n";
    for (int i = 0; i < ROWS; i++)
    {
      std::cout << "|";
      for (int j = 0; j < COLS; j++)
      {
        int cell = display_buffer[i * COLS + j];
        switch (cell)
        {
        case 0:
          std::cout << " . ";
          break;
        case 1:
          std::cout << "\033[31m # \033[0m";
          break;
        case 2:
          std::cout << "\033[35m # \033[0m";
          break;
        case 3:
          std::cout << "\033[32m # \033[0m";
          break;
        case 4:
          std::cout << "\033[38;5;222m # \033[0m";
          break;
        case 5:
          std::cout << "\033[34m # \033[0m";
          break;
        case 6:
          std::cout << "\033[38;5;208m # \033[0m";
          break;
        case 7:
          std::cout << "\033[36m # \033[0m";
          break;
        case -1:
          std::cout << "\033[31m[ ]\033[0m";
          break;
        case -2:
          std::cout << "\033[35m[ ]\033[0m";
          break;
        case -3:
          std::cout << "\033[32m[ ]\033[0m";
          break;
        case -4:
          std::cout << "\033[38;5;222m[ ]\033[0m";
          break;
        case -5:
          std::cout << "\033[34m[ ]\033[0m";
          break;
        case -6:
          std::cout << "\033[38;5;208m[ ]\033[0m";
          break;
        case -7:
          std::cout << "\033[36m[ ]\033[0m";
          break;
        }
      }
      std::cout << "|\n";
    }
    std::cout << "+------------------------------+\n";
  }

  void print_game_over()
  {
    std::cout << "\n ==============================\n";
    std::cout << "           GAME OVER            \n";
    std::cout << " ==============================\n";
  }
};

// ============================================================================
// BUCLE PRINCIPAL
// ============================================================================
int main()
{
  char play_again;

  do
  {
    {
      Game tetris;

      while (tetris.is_running)
      {

        for (int i = 0; i < 10; i++)
        {
          bool screen_needs_update = false;
          char key;

          while ((key = tetris.get_input()) != 0)
          {
            if (key == 'q')
            {
              tetris.is_running = false;
              break;
            }
            tetris.process_input(key);
            screen_needs_update = true;
          }

          if (screen_needs_update && tetris.is_running)
          {
            tetris.render();
          }

          usleep(tetris.get_drop_speed());
        }

        if (tetris.is_running)
        {
          tetris.tick();
          tetris.render();
        }
      }

      tetris.print_game_over();
    }

    sleep(2);
    std::cout << "\n  ¿Quieres jugar de nuevo? [y/N]: ";
    std::cin >> play_again;

  } while (play_again == 'y' || play_again == 'Y');
  std::cout << "\e[?25h";
  return 0;
}
