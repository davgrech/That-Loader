#include "bootmenu.h"
#include "shellutils.h"
#include "configfile.h"
#include "logs.h"
#include "bootutils.h"
#include "display.h"
#include "shell.h"
#include "LoadImage.h"
#include "efilibs.h"

#define F5_KEY_SCANCODE (0x0F) // Used to refresh the menu (reparse config)

#define SHELL_CHAR  ('c')
#define INFO_CHAR   ('i')

#define BAD_CONFIGURATION_ERR_MSG ("An error has occurred while parsing the config file.")
#define FAILED_BOOT_ERR_MSG ("An error has occurred during the booting process.")

// temp forward functions

static void BootMenu(boot_entry_array_s* entryArr);
static void InitBootMenuOutput(void);

static void FailMenu(const char_t* errorMsg);

static inline void BootHighlightedEntry(boot_entry_array_s* entryArr);
static inline void PrintHighlightedEntryInfo(boot_entry_array_s* entryArr);
static void PrintBootMenu(boot_entry_array_s* entryArr);
static inline void PrintInstructions(void);
static void BootEntry(boot_entry_s* selectedEntry);
static void PrintEntryInfo(boot_entry_s* selectedEntry);

static void PrintMenuEntries(boot_entry_array_s* entryArr);
static void scrollEntries(void);
static void PrintTimeout(void);




boot_menu_cfg_s bmcfg; // boot menu config
void StartBootManager()
{
    InitBootMenuOutput();
    while(TRUE)
    {
        
        ST->ConOut->ClearScreen(ST->ConOut);
        ST->ConOut->SetAttribute(ST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
        ST->ConOut->EnableCursor(ST->ConOut, FALSE);
        SetTextPosition(3, 0);
        ST->ConOut->OutputString(ST->ConOut, L"Welcome to That Loader!\r\n");
        

        // Config parsing is in the loop because i want the config to be updatable even when the program is running 
        SetTextPosition(0, 1);
        ST->ConOut->OutputString(ST->ConOut, L"Parsing config...\n");
        boot_entry_array_s bootEntries = ParseConfig();
        


        ST->ConIn->Reset(ST->ConIn, 0); // clean input buffer

        ClearScreen();
        //check if Boot entries were parsed correctly
        if (bootEntries.numOfEntries == 0)
        {
            FailMenu(BAD_CONFIGURATION_ERR_MSG);
        }
        else
        {
            BootMenu(&bootEntries);
            
        }


        //clear up boot entries
        FreeConfigEntries(&bootEntries);
        bmcfg.selectedEntryIndex = 0;
        bmcfg.entryOffset = 0;

    }
    
}
/*
* Fail menu - use it whenever somethng goes wrong
* user gets a few options to recover failed attmept
* (return to main menu, open shell, show error log file, reboot and shutdown)
*/
static void FailMenu(const char_t* errorMsg)
{
    boolean_t returnToMainMenu = FALSE;
    bmcfg.timeoutCancelled = TRUE;

    while(!returnToMainMenu)
    {
        ClearScreen();
        SetTextPosition(0, 0);
        ST->ConOut->OutputString(ST->ConOut, L"That-Loader\r\n");
        printf("Something went wrong\r\n");
        printf("%s\n\n", errorMsg);
           printf("1) Return to Main Menu\n"
           "2) Open shell\r\n"
           "3) Show error log file\r\n"
           "4) Warm reboot (Restart)\r\n"
           "5) Shutdown\r\n"
           "Choose desired option to continue (1-5)\n");
        
        ST->ConIn->Reset(ST->ConIn, 0);
        efi_input_key_t key = {0};

        // check if key is valid
        do
        {
            key = GetInputKey();
        } while ((key.UnicodeChar < '1') || (key.UnicodeChar > '5'));

        switch(key.UnicodeChar)
        {
            case '1':
                
                returnToMainMenu = TRUE;
                break;
            case '2':
                StartShell();
                
                break;
            case '3':
                ShowLogFile();
                
                break;
            case '4':
                // warm reboot
                ClearScreen();
                Log(LL_INFO, 0, "Warm resetting machine...");
                efi_status_t status = ST->RuntimeServices->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, 0);
                Log(LL_ERROR, 0, "Failed to reboot machine");
                break;
            case '5':
                ClearScreen();
                Log(LL_INFO, 0, "Shutting down machine...");
                // shutdown
                ST->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, 0);
                Log(LL_ERROR, 0, "Failed to shutdown machine");
                break;
            default:
                // nothing
                break;
        }
    }
    
    ST->ConOut->ClearScreen(ST->ConOut);

}

/*
* Print the recent log file
*/
void ShowLogFile(void)
{
    ST->ConOut->ClearScreen(ST->ConOut);

    PrintLogFile();

    printf("\nPress any key to return...");
    GetInputKey();
}


/*
* This function sets up the screen for boot entries menu
* uses the bmcfg (boot menu config) struct to initialize the boot screen
* inits rows, columns, and all bcmfg attributes
*/
static void InitBootMenuOutput(void)
{
    // reserved rows for other entries
    const int32_t reserved_rows = 10;
    if(screenModeSet) // if screen size is set and ready to use
    {
        bmcfg.maxEntriesOnScreen = screenRows - reserved_rows;
    }
    else
    {
        bmcfg.maxEntriesOnScreen = DEFAULT_CONSOLE_ROWS - reserved_rows;
    }
    bmcfg.entryOffset = 0;
    bmcfg.timeoutSeconds = DEFAULT_TIMEOUT_SECONDS;
    bmcfg.selectedEntryIndex = 0;
    bmcfg.timeoutCancelled = FALSE;
    bmcfg.bootImmediately = FALSE;

}

/*
* change the selected index (scrool the entries)
*/
static void scrollEntries(void)
{
    // 0
    // 1
    // 2
    // 3
    if(bmcfg.selectedEntryIndex < bmcfg.entryOffset) // scroll up
    {
        bmcfg.entryOffset = bmcfg.selectedEntryIndex;
    }
    if (bmcfg.selectedEntryIndex >= bmcfg.entryOffset + bmcfg.maxEntriesOnScreen) // scroll down (bmcfg.maxEntriesOnScreen to avoid scrolling out of bounds)
    {
        bmcfg.entryOffset = bmcfg.selectedEntryIndex - bmcfg.maxEntriesOnScreen + 1;
    }
}


/*
*   This function prints the current menu entries, and highlights the selcted one
*/
static void PrintMenuEntries(boot_entry_array_s* entryArr)
{
    int32_t index = bmcfg.entryOffset;

    // Print hidden entries
    if (index > 0)
    {
        printf(" . . . %d more", index);
    }
    else{
        PrintEmptyLine();
    }
    for(int32_t i =0; i < bmcfg.maxEntriesOnScreen; i++)
    {
        //prevent goind out of bounds
        if(index >= entryArr->numOfEntries)
        {
            break;
        }

        int32_t entryNum = index + 1;
        char_t* entryName = entryArr->entryArray[index].name;
        if(index == bmcfg.selectedEntryIndex) // highlight entry
        {
            ST->ConOut->SetAttribute(ST->ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY)); // higlight text
            printf("* %d) %s", entryNum, entryName); // print stuff
            ST->ConOut->SetAttribute(ST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK)); // go back to normal
        }
        else // print normally
        {
            printf(" %d) %s", entryNum, entryName);
        }
        PadRow();

        index++;

    }

    // Print how many hidden entries are at the bottom of the screen
    if(index < entryArr->numOfEntries)
    {
        printf(" . . . . %d more", entryArr->numOfEntries);
        PadRow();
    }
    else
    {
        PrintEmptyLine();
    }


}

/*
*   Print simple user instructions
*/
static inline void PrintInstructions(void)
{
    printf("\nUse the ↑ and ↓ arrow keys to select which entry is highlighted.\n"
    "Press enter to boot the seleted entry, press 'i' to get more info about the entry\n"
    "Press 'c' for a command-line, and 'F5' to refresh the menu.\n");
}


/*
* This function prints the boot menu config timeout, alerting user it will boot automatically 
* the highlighted entry
*/
static void PrintTimeout(void)
{
    ST->ConOut->SetAttribute(ST->ConOut, EFI_TEXT_ATTR(EFI_DARKGRAY, EFI_BLACK));
    //ST->ConOut->SetCursorPosition(ST->ConOut,0 , DEFAULT_CONSOLE_ROWS);
    printf("\nThe highlighted entry will boot automatically in %d seconds.",bmcfg.timeoutSeconds);
    ST->ConOut->SetAttribute(ST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
    PadRow();
}


/*
*   This function to print the boot fully. (all entries and timeouts included)
*/
static void PrintBootMenu(boot_entry_array_s* entryArr)
{
    if (screenModeSet)
    {
        ST->ConOut->SetCursorPosition(ST->ConOut, 0, 0);
    }
    else
    {
        ST->ConOut->ClearScreen(ST->ConOut);
    }

    ST->ConOut->SetCursorPosition(ST->ConOut, DEFAULT_CONSOLE_COLUMNS/2, 0);
    ST->ConOut->SetAttribute(ST->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
    printf("That-Loader - 1.2\n");
    //ST->ConOut->SetCursorPosition(ST->ConOut, 0, 0);

    PrintMenuEntries(entryArr);

    PrintInstructions();

    if(!bmcfg.timeoutCancelled)
    {
        PrintTimeout();
    }
    else{
        PrintEmptyLine();
    }

}


/*
* This is the main boot menu function, the main loop runs, waiting for input of up to 10 seconds
* utlizes the timeout, and handles all input types (scrolling, shell initalization, opening a config file)
*/
static void BootMenu(boot_entry_array_s* entryArr)
{
    while(TRUE)
    {
        PrintBootMenu(entryArr);
        if(!bmcfg.timeoutCancelled)
        {
            if(bmcfg.bootImmediately)
            {
                BootHighlightedEntry(entryArr);
                return;
            }

            int32_t timerStatus = WaitForInput(10000);
            if(timerStatus == INPUT_TIMER_TIMEOUT)
            {
                bmcfg.timeoutSeconds--;
                //Boot the selected entry if the timer ends
                if(bmcfg.timeoutSeconds == 0)
                {
                    BootHighlightedEntry(entryArr);
                    return;
                }
                continue;
            }
            else if(timerStatus == INPUT_TIMER_KEY)
            {
                // Cancel the timer if a key was pressed
                bmcfg.timeoutSeconds = TRUE;
            }
        }
        efi_input_key_t key = GetInputKey();

        switch (key.ScanCode)
        {
        case UP_ARROW_SCANCODE:
            if(bmcfg.selectedEntryIndex != 0)
            {
                // scroll up
                bmcfg.selectedEntryIndex--;
                scrollEntries();
                
            }
            break;
        case DOWN_ARROW_SCANCODE:
            if(bmcfg.selectedEntryIndex + 1 < entryArr->numOfEntries)
            {
                // scroll down
                bmcfg.selectedEntryIndex++;
                scrollEntries();
                
            }
            break;
        case F5_KEY_SCANCODE:
            // redo the loop - and reparse the config
            return;

        
        default:
            switch (key.UnicodeChar)
            {
            case CHAR_CARRIAGE_RETURN:
            // hit enter
                BootHighlightedEntry(entryArr);
                break;
            case SHELL_CHAR:
                StartShell();
                break;
            case INFO_CHAR:
                PrintHighlightedEntryInfo(entryArr);
            default:
            //nun
                break;
            }
        }
    }
}

/*
* This function prints the selected entry's additional info, taken from the config file
* such as path, arguments and name
* Used when pressing 'i' on a highlighted entry
*/
static void PrintEntryInfo(boot_entry_s* selectedEntry)
{
    ClearScreen();
    SetTextPosition(0, 0);
    printf("Entry indexed at: %d\n\n", bmcfg.selectedEntryIndex + 1);
    printf("Name: %s\n"
    "Path: %s\n"
    "Args: %s\n",
    selectedEntry->name, selectedEntry->imageToLoad, selectedEntry->imageArgs);

    if (selectedEntry->isDirectoryToKernel)
    {
        printf("\nKernel directory: %s\n", selectedEntry->kernelScanInfo->kernelDirectory);
        printf("Kernel version string: %s\n", selectedEntry->kernelScanInfo->kernelVersionString);
    }
    printf("Press any key to return...");
    GetInputKey();
    ClearScreen();
}


// wrapper func to print the info with a better usage
static inline void PrintHighlightedEntryInfo(boot_entry_array_s* entryArr)
{
    PrintEntryInfo(&entryArr->entryArray[bmcfg.selectedEntryIndex]);
}


/*
* Print additional information about the image to be loaded
* and call chainloading func
*/
static void BootEntry(boot_entry_s* selectedEntry)
{
    // Print info before booting
    ST->ConOut->ClearScreen(ST->ConOut);
    printf("Booting '%s'...\n"
    "Image Args: '%s'\n"
    "Image path: '%s'\n", selectedEntry->name, selectedEntry->imageArgs, selectedEntry->imageToLoad);


    ChainloadImage(selectedEntry->imageToLoad, selectedEntry->imageArgs);

    printf("\nFailed to boot.\n"
    "Press any key to return to menu...");

    GetInputKey();
}

static inline void BootHighlightedEntry(boot_entry_array_s* entryArr)
{
    BootEntry(&entryArr->entryArray[bmcfg.selectedEntryIndex]);
    // if booting failes, we end up here
    FailMenu(FAILED_BOOT_ERR_MSG);
}





