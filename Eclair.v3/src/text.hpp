#pragma once

#include <string>
#include <vector>
#include "sharedTypes.hpp"

class TextBuffer
{
    public:
        TextBuffer(std::string);
        
        size_t pieceCount() const;
        Piece piece(int index) const;
        
        const std::string& giveBuffer(Piece& piece) const;
        
    private:
        std::string original_;
        std::string appended_;
        
        std::vector<Piece> pieces;
};
