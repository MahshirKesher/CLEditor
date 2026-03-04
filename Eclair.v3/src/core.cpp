#include <string>

#include "core.hpp"
#include "sharedTypes.hpp"
#include "keys.hpp"

WinSize winsize = {0, 0};

EditorCore::EditorCore()
: terminal_(),
  input_(terminal_)
{
    running = true;
    terminal_.write("\x1b[2J");
    terminal_.write("\x1b[H");
    winsize = terminal_.getWindowSize();
}

Status EditorCore::handleMovement(Movement input)
{
    switch(input)
    {
        case UP:
            if(cursor_.row() > 0) cursor_.setRow(cursor_.row() - 1);
            break;
        case DOWN:
            if(cursor_.row() < winsize.rows) cursor_.setRow(cursor_.row() + 1);
            break;
        case LEFT:
            if(cursor_.col() > 0) cursor_.setCol(cursor_.col() - 1);
            break;
        case RIGHT:
            if(cursor_.col() < winsize.cols) cursor_.setCol(cursor_.col() + 1);
            break;
    }
    // this part will be moved to renderer module
    int onScreenRow = cursor_.row() + 1;
    int onScreenCol = cursor_.col() + 1;
    std::string buffer = "\x1b[" 
                       + std::to_string(onScreenRow) 
                       + ';' 
                       + std::to_string(onScreenCol) 
                       + 'H';
    terminal_.write(buffer);
    return Success;
}

Status EditorCore::processInput(int input)
{
    switch(input)
    {
        case QUIT:
            running = false;
            return Success;
        case UP:
        case DOWN:
        case RIGHT:
        case LEFT:
            return handleMovement(static_cast<Movement>(input));
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
        
        if(status == Success) processInput(input_.define(input));
        else if(status == SignalInterrupt) 
        {
            if(terminal_.checkFlag(WinResize)) winsize = terminal_.getWindowSize();
            terminal_.clearFlag();
        }
    }
}
