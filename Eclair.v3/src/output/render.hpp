#pragma once

#include <string>
#include "sharedTypes.hpp"

class Terminal;
class Viewport;
class TextBuffer;
class Cursor;

class Renderer
{
    public:
        Renderer(Terminal&, Viewport&, TextBuffer&, Cursor&);
    
        void updateScreen();
        void clearScreen();
        
        void fillFrame();
        void blank();
        
    private:
        Terminal& terminal_;
        Viewport& view_;
        TextBuffer& text_;
        Cursor& cursor_;
};
