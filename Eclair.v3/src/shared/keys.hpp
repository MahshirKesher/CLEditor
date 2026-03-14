#pragma once

#define CTRL_(x) ((x) & 0x1F)

enum Movement
{
    UP = 65,
    DOWN,
    RIGHT,
    LEFT
};

enum Command
{
    QUIT
};
