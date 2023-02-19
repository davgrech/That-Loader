#include "include\bootmenu.h"
#include "include\logs.h"
#include "include\display.h"

int main(int argc, char **argv)
{
    if(!InitLogger())
    {
        printf("Failed to initialize logs, guess logs are now disabled\n");
    }

    // set max console size and store as global vars
    if(!SetMaxConsoleSize())
    {
        if(!QueryCurrentConsoleSize())
        {
            Log(LL_WARNING, 0, "Failed to set console size, Clearing screen and redrawing screen.");
        }
    }

    StartBootManager();

    return 1;
}

