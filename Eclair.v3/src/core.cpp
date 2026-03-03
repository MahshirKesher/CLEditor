#include <string>

#include "core.hpp"
#include "sharedTypes.hpp"
#include "keys.hpp"

WinSize winsize = {0, 0};

EditorCore::EditorCore()
: terminal_()
{
    running = true;
    terminal_.write("\x1b[2J");
    terminal_.write("\x1b[H");
    terminal_.getWindowSize();
}

Status EditorCore::processInput(unsigned char& input)
{
    switch(input)
    {
        case CTRL_('q'):
            running = false;
            return Success;
        default:
            return Success;
    }
}

void EditorCore::run()
{
    unsigned char input = 0;
    Status status = Success;

    while(running)
    {
        status = terminal_.read(&input); 
        
        if(status == Success) processInput(input);
        else if(status == SignalInterrupt) 
            if(terminal_.flag() == WinResize) winsize = terminal_.getWindowSize();
    }
}
