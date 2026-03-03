#include "core.hpp"
#include <string>

int main(int argc, char* argv[])
{
    std::string filename;

    if(argc >= 2) filename = argv[1];
    else filename = "";

    EditorCore editor;
    editor.run();
    
    return 0;
}
