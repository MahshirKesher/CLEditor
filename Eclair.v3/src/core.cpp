#include <string>

#include "core.hpp"
#include "sharedTypes.hpp"
#include "keys.hpp"

EditorCore::EditorCore(std::string filename)
: terminal_(),
  input_(terminal_),
  view_(terminal_.getWindowSize()),
  cursor_(),
  file_(),
  text_(file_.open(filename)),
  render_(terminal_, view_, text_) 
{
    running = true;
    render_.updateScreen(cursor_.row(), cursor_.col());
}

Status EditorCore::handleMovement(Movement input)
{
    int row = cursor_.row();
    int col = cursor_.col();

    switch(input)
    {
        case UP:
            if(row > 0) cursor_.setRow(row - 1);
            break;
        case DOWN:
            if(row < view_.rows()) cursor_.setRow(row + 1);
            break;
        case LEFT:
            if(col > 0) cursor_.setCol(col - 1);
            break;
        case RIGHT:
            if(col < view_.cols()) cursor_.setCol(col + 1);
            break;
    }
    std::string buffer = "\x1b[" 
                       + std::to_string(row + 1) 
                       + ";" 
                       + std::to_string(col + 1) 
                       + "H";
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
    int input = 0;
    Status status = Success;

    while(running)
    {
        status = terminal_.read(&input); 
        
        if(status == Success) processInput(input_.define(input));
        else if(status == SignalInterrupt || status == NoInput) 
        {
            if(terminal_.checkFlag(WinResize))  
            {
                view_.setWindowSize(terminal_.getWindowSize());
                render_.updateScreen(cursor_.row(), cursor_.col());
                terminal_.clearFlag();
            }
        }
    }
} 
