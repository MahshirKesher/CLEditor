#pragma once

#include "sharedTypes.hpp"
#include <string_view>
#include <termios.h>

class Terminal
{
    public:
        Terminal();
        ~Terminal();
    
        Status enableRawMode();
        Status disableRawMode();
        
        Status read(void* storage);
        Status write(std::string_view input);
        
        Interrupt flag();
        
        void setupSignalHandler();
        static void handleInterrupt(int signal);
        
        WinSize getWindowSize();
        
    private:
        struct termios original_;
        static Interrupt signalFlag;
};
