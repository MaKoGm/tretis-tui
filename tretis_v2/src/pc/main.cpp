#include "../core/game.hpp"
#include "mpv_audio.hpp"
#include "terminal_ui.hpp"
#include <iostream>
#include <chrono>

volatile sig_atomic_t g_quit = 0;

void handle_sigint(int signum)
{
    g_quit = 1;
}

int main()
{
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    MusicPlayer bg_music;
    bg_music.play("tetris.opus");
    char play_again = 'n';

    do
    {
        bool auto_restart = false;
        play_again = 'n';
        std::cout << "\033[2J";

        {
            Game tetris;
            TerminalUI ui;
            auto last_tick = std::chrono::steady_clock::now();

            ui.render(tetris);

            while (tetris.is_running && !g_quit)
            {
                char key;
                while ((key = ui.get_input()) != 0)
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

                ui.render(tetris);
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
                ui.print_game_over();
            }

            ui.restore_blocking();
            ui.flush_input();
        } // ui y tetris se destruyen aquí

        if (auto_restart)
        {
            play_again = 'y';
        }
        else if (!g_quit)
        {
            std::cout << "\e[?25h";
            sleep(1);
            std::cout << "\n¿Quieres jugar de nuevo? [y/N]: ";
            std::cin.clear();
            if (!(std::cin >> play_again))
                play_again = 'n';
        }

    } while ((play_again == 'y' || play_again == 'Y') && !g_quit);

    std::cout << "\e[?25h";
    return 0;
}