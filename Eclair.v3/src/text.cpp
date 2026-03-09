#include "text.hpp"

TextBuffer::TextBuffer(std::string initContent)
{
    original_ = initContent;
    Piece initPiece = {0, static_cast<int>(original_.size()), ORIGINAL};
    pieces.push_back(initPiece);
}

const std::string& TextBuffer::giveBuffer(Piece& piece) const
{
    return (piece.source == ORIGINAL? original_ : appended_);
}

size_t TextBuffer::pieceCount() const { return pieces.size(); }
Piece TextBuffer::piece(int index) const { return pieces.at(index); }
