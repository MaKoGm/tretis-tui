#pragma once
#include "../platform/audio_interface.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

class MusicPlayer : public AudioInterface
{
private:
    pid_t pid;

public:
    MusicPlayer() : pid(-1) {}
    ~MusicPlayer() override { stop(); }

    void play(const std::string &filepath) override
    {
        stop(); // Detener anterior si existe
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
            exit(1);
        }
    }

    void stop() override
    {
        if (pid > 0)
        {
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
            pid = -1;
        }
    }
};