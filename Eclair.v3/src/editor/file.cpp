#include "file.hpp"
#include <fstream>

std::string FileHandler::open(std::string filename)
{
    if(filename == "") return ""; 

    std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);
    file.seekg(0, file.end);
    int filesize = file.tellg();
    file.seekg(0, file.beg);
    
    std::string text(filesize, '*');
    file.read(text.data(), filesize);
    
    return text;
}
