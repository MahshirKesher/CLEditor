#pragma once

#include "terminal.hpp"

#include <string>

class EditorCore
{
    public:
        EditorCore();
    
        void run();
        
        Status processInput(unsigned char& input);

    private:
        Terminal terminal_;
        
        bool running;
};

#define CTRL_(x) ((x) & 0x1F)
