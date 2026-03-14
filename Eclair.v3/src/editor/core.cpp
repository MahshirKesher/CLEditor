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
  render_(terminal_, view_, text_, cursor_) 
{
    running = true;
    render_.updateScreen();
}

Location EditorCore::findNextStart()
{
    Location initStart = view_.viewStart();
    Location currentStart = initStart;
    int pieceCount = text_.pieceCount();
    
    for(int i = currentStart.pieceIndex; i < pieceCount; i++)
    {
        Piece currentPiece = text_.piece(i);
        const std::string& buffer = text_.giveBuffer(currentPiece);
        
        for(int j = currentStart.inPieceOffset; j < currentPiece.length; j++)
        {
            if(buffer.at(j) == '\n')
            {
                bool endOfFile = (i == pieceCount - 1 && j == currentPiece.length - 1);
                if(endOfFile) return initStart;
                else if(j == currentPiece.length - 1) return {i + 1, 0}; 
                else return {i, j + 1};
            }
        }
        currentStart.inPieceOffset = 0;
    }
    return initStart;
}

bool EditorCore::enoughSpace(Location currentStart, int steps)
{
    int freespace = 0;
    for(int i = 0; i < currentStart.pieceIndex; i++)
    {
        freespace += text_.piece(i).length;
        if(freespace >= steps) return true;
    }
    freespace += currentStart.inPieceOffset;
    if(freespace >= steps) return true;
    else return false;
}

Location EditorCore::stepBack(Location currentStart, int steps)
{
    if(!enoughSpace(currentStart, steps)) return currentStart;

    if(currentStart.inPieceOffset >= steps) 
        return {currentStart.pieceIndex, currentStart.inPieceOffset - steps};
    steps -= (currentStart.inPieceOffset + 1);
    if(currentStart.pieceIndex > 0) currentStart.pieceIndex--;
    currentStart.inPieceOffset = text_.piece(currentStart.pieceIndex).length - 1;
    while(steps > 0)
    {
        if(currentStart.inPieceOffset >= steps)
            return {currentStart.pieceIndex, currentStart.inPieceOffset - steps};
       
        steps -= (currentStart.inPieceOffset + 1);
        if(currentStart.pieceIndex > 0) currentStart.pieceIndex--;
        currentStart.inPieceOffset = text_.piece(currentStart.pieceIndex).length - 1;
    }
    
    return currentStart;
}

Location EditorCore::findPreviousStart()
{
    Location initStart = view_.viewStart();
    if(initStart.inPieceOffset < 2 && initStart.pieceIndex == 0) return {0, 0};
    
    Location currentStart = stepBack(initStart, 2);
    if(initStart.sameAs(currentStart)) return {0, 0};
    
    for(int i = currentStart.pieceIndex; i >= 0; i--)
    {
        Piece currentPiece = text_.piece(i);
        const std::string& buffer = text_.giveBuffer(currentPiece);
        
        for(int j = currentStart.inPieceOffset; j >= 0; j--)
        {
            if(buffer.at(j) == '\n')
            {
                if(j == currentPiece.length - 1) return {i + 1, 0}; 
                else return {i, j + 1};
            }
        }
        if(i > 0) currentStart.inPieceOffset = (text_.piece(i - 1).length - 1);
    }
    return {0, 0};
}

Status EditorCore::handleMovement(Movement input)
{
    switch(input)
    {
        case UP:
            if(cursor_.row() > 0) cursor_.setRow(cursor_.row() - 1);
            if(cursor_.row() <= view_.rowOffset() + 3)
            {
                view_.setStart(findPreviousStart());
                render_.updateScreen();
                view_.setRowOffset(view_.rowOffset() - 1);
            }
            break;
        case DOWN:
            cursor_.setRow(cursor_.row() + 1);
            if(cursor_.row() >= (view_.rowOffset() + view_.rows()) - 3)
            {
                view_.setStart(findNextStart());
                render_.updateScreen();
                view_.setRowOffset(view_.rowOffset() + 1);
            }
            break;
        case LEFT:
            if(cursor_.col() > 0) cursor_.setCol(cursor_.col() - 1);
            break;
        case RIGHT:
            if(cursor_.col() < view_.cols()) cursor_.setCol(cursor_.col() + 1);
            break;
    }
    std::string buffer = "\x1b[" 
                       + std::to_string((cursor_.row() - view_.rowOffset()) + 1) 
                       + ";" 
                       + std::to_string(cursor_.col() + 1) 
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
        else if(status == SignalInterrupt) 
        {
            if(terminal_.checkFlag(WinResize))  
            {
                view_.setWindowSize(terminal_.getWindowSize());
                render_.updateScreen();
                terminal_.clearFlag();
            }
        }
    }
} 
