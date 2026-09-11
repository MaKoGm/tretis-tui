#pragma once
#include <string>

class AudioInterface
{
public:
    virtual ~AudioInterface() = default;
    virtual void play(const std::string &filepath) = 0;
    virtual void stop() = 0;
};