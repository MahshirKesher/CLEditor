#include "input.hpp"
#include "terminal.hpp"
#include "sharedTypes.hpp"

#include <string>

InputHandler::InputHandler(Terminal& terminal)
: terminal_(terminal)
{}

int InputHandler::define(int input)
{
    switch(input)
    {
        case CTRL_('q'):
            return QUIT;
            
        case '\x1b':
            if(terminal_.read(&input) == Success && input == '[') 
                return defineSequence();
            else return input;
            
        default:
            return input;
    }
}

int InputHandler::defineSequence()
{
    unsigned char byte = 0;
    std::string key_sequence;
    while(terminal_.read(&byte) == Success)
    {
        key_sequence += byte;
        if((byte >= 'A' && byte <= 'Z') || byte == '~') break;
    }
    if(key_sequence.empty()) return '\x1b';
    
    switch(key_sequence.at(0))
    {
        case 'A':
            return UP;
        case 'B':
            return DOWN;
        case 'C':
            return RIGHT;
        case 'D':
            return LEFT;
        default:
            return '\x1b';
    }
}
