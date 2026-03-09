#pragma once

#include "sharedTypes.hpp"

class Viewport
{
    public:
        Viewport(WinSize initSize);
    
        void setWindowSize(WinSize newWinSize);
        
        int rows() const;
        int cols() const;
        
        int rowOffset() const;
    private:
        int rows_, cols_;
        int rowOffset_;
};
