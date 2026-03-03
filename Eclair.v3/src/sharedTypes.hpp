#pragma once

enum Status
{
    Success = 1,
    AttributeGettingError = 10,
    AttributeSettingError = 11,
    ReadingError = 20,
    NoInput = 21,
    SignalInterrupt = 22,
    WritingError = 30
};

enum Interrupt
{
    StandBy = 100,
    WinResize = 101
};

struct WinSize
{
    int rows;
    int cols;
};
