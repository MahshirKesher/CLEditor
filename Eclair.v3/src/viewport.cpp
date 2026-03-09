#include "viewport.hpp"
#include <signal.h>

Viewport::Viewport(WinSize initSize)
{
    rows_ = initSize.rows;
    cols_ = initSize.cols;
    rowOffset_ = 0;
}

void Viewport::setWindowSize(WinSize newWinsize)
{
    rows_ = newWinsize.rows;
    cols_ = newWinsize.cols;
}

int Viewport::rows() const { return rows_; }
int Viewport::cols() const { return cols_; }
int Viewport::rowOffset() const { return rowOffset_; }
