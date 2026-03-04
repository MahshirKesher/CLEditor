#pragma once

class Cursor
{
    public:
        Cursor();
    
        void moveTo(int row, int col);
        
        int row() const;
        int col() const;
        
        void setRow(int row);
        void setCol(int col);
        
    private:
        int row_;
        int col_;
};
