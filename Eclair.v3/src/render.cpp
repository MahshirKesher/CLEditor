#include "render.hpp"
#include "terminal.hpp"
#include "viewport.hpp"
#include "text.hpp"

#include <string>

Renderer::Renderer(Terminal& terminal, Viewport& view, TextBuffer& text)
: terminal_(terminal),
  view_(view),
  text_(text)
{}

void Renderer::clearScreen()
{
    terminal_.write("\x1b[2J\x1b[H");
}

void Renderer::updateScreen(int row, int col)
{
    terminal_.write("\x1b[?25l");
    clearScreen();
    fillFrame();
    terminal_.write("\x1b[?25h");
    std::string buffer = "\x1b[" 
                       + std::to_string(row + 1) 
                       + ";" 
                       + std::to_string(col + 1) 
                       + "H";
    terminal_.write(buffer);
}

void Renderer::fillFrame()
{
    int rowCount = 0;
    int screenRows = view_.rows();
    Piece currentPiece;
    size_t pieceCount = text_.pieceCount();
    std::string frame;
    frame += std::to_string(rowCount + 1) + '\t';
    
    for(size_t pieceIndex = 0; pieceIndex < pieceCount; pieceIndex++)
    {
        currentPiece = text_.piece(pieceIndex);
        const std::string& buffer = text_.giveBuffer(currentPiece);
        for(int i = 0; i < currentPiece.length; i++)
        {
            frame += buffer.at(currentPiece.start + i);
            if(frame.back() == '\n')
            {
                rowCount++;
                frame += "\r";
                frame += std::to_string(rowCount + 1) + '\t';
            }
            if(rowCount == screenRows - 2) 
            {
                terminal_.write(frame);
                return;
            }
        }
    }
    terminal_.write(frame);
}
