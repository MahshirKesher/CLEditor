#pragma once

#include <string>
#include "sharedTypes.hpp"

class Terminal;
class Viewport;
class TextBuffer;

class Renderer
{
    public:
        Renderer(Terminal&, Viewport&, TextBuffer&);
    
        void updateScreen(int row, int col);
        void clearScreen();
        
        void fillFrame();
        
    private:
        Terminal& terminal_;
        Viewport& view_;
        TextBuffer& text_;
};
