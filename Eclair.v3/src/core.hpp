#pragma once

#include "terminal.hpp"
#include "cursor.hpp"
#include "input.hpp"

#include <string>

class EditorCore
{
    public:
        EditorCore();
    
        void run();
        
        Status processInput(int input);
        Status handleMovement(Movement input);

    private:
        Terminal terminal_;
        InputHandler input_;
        Cursor cursor_;
        
        bool running;
};
