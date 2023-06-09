#include "bootutils.h"
#include "logs.h"




/*
*   Convert normal string to wide string
*   normal string uses 1 byte per char
*   wide string uses 2 bytes per char (to represent diffrent types of chars like hebrew, arabics, chinese, etc)
*/
wchar_t* StringToWideString(char_t* str)
{
    // The size has to be multiplied by the size of wchar_t
    // because wchar_t is 2 bytes, while char_t is 1 byte
    const size_t size = strlen(str) * sizeof(wchar_t);
    wchar_t* wpath = malloc(size + 1);
    if (wpath == NULL)
    {
        Log(LL_ERROR, 0, "Failed to allocate memory for wide string");
        return NULL;
    }

    wpath[size] = 0;
    mbstowcs(wpath, str, size);
    return wpath;

}

/*
*   Get a file device handler
*   Used when loading a filesystem (to acsses file in the filesystem)
*/
efi_handle_t GetFileDeviceHandle(char_t* path)
{
    // Get all the simple file system protocol handles
    efi_guid_t sfsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    uintn_t bufSize = 0;
    efi_handle_t* handles = NULL;

    efi_status_t status = BS->LocateHandle(ByProtocol, &sfsGuid, NULL, &bufSize, handles);
    if(status == EFI_BUFFER_TOO_SMALL)
    {
        Log(LL_ERROR, status, "Inital location of the simple file system protocol handle failed");
        return NULL;
    }

    handles = malloc(bufSize);
    if(handles == NULL)
    {
        Log(LL_ERROR, status, "Failed to allocate memory for handles");
        return NULL;
    }

    // load all handles to handles
    status = BS->LocateHandle(ByProtocol, &sfsGuid, NULL, &bufSize, handles);
    if(EFI_ERROR(status))
    {
        Log(LL_ERROR, status, "Unable to locate the simple file system protocol handles");
        return NULL;
    }
    uintn_t numHandles = bufSize / sizeof(efi_handle_t);
    efi_simple_file_system_protocol_t* sfsProt = NULL;
    efi_guid_t devGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;

    efi_device_path_t* devPath = NULL;
    efi_file_handle_t* rootDir = NULL;
    efi_file_handle_t* fileHandle = NULL;

    //Find the right protocols
    efi_handle_t devHandle = NULL;
    for(uintn_t i = 0; i < numHandles; i++)
    {
        efi_handle_t handle = handles[i];


        // find the fs protocol (fat/NTFS blabla)
        status = BS->HandleProtocol(handle, &sfsGuid, (void**)&sfsProt);
        if (EFI_ERROR(status))
        {
            if(i+1 == numHandles)
            {
                Log(LL_ERROR, status, "Failed to obtain the simple file system protocols");
                return NULL;
            }
            continue;
        }
        // Find the device path protocol - help our program to identify the hardware the device represents
        status = BS->HandleProtocol(handle,&devGuid, (void**)&devPath);
        if (EFI_ERROR(status))
        {
            if (i+1 == numHandles)
            {
                Log(LL_ERROR, status, "Failed to obtain the device path protocol");
                return NULL;
            }
            continue;
        }
        // Check if were accsesing the right FAT volume
        // if a file with the same name exists on 2 diffrent voulumes
        // boot manager will load the first instance

        // open root volume
        status = sfsProt->OpenVolume(sfsProt, &rootDir);
        if(EFI_ERROR(status))
        {
            continue;
        }  
        // check if file exists
        wchar_t* wpath = StringToWideString(path);
        status = rootDir->Open(rootDir, &fileHandle, wpath, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);

        rootDir->Close(rootDir);
        free(wpath);
        if(!EFI_ERROR(status))
        {
            // Break if file not found
            devHandle = handle;
            break;
        }
    }
    free(handles);
    if (fileHandle == NULL)
    {
        Log(LL_ERROR,0, "Unable to find the file '%s' on the machine.", path);
        return NULL;
    }
    return devHandle;

}


// Start a watchdog timer
//It works by continuously counting down from a specified time interval
// and if the software fails to reset the timer before it reaches zero
// the watchdog timer generates a system reset
void EnableWatchdogTimer(uintn_t seconds)
{
    efi_status_t status = BS->SetWatchdogTimer(seconds, 0, 0, NULL);
    if (EFI_ERROR(status))
    {
        Log(LL_WARNING, status, "Failed to set watchdog timeout to %d seconds.", seconds);
    }
}


// The function reads the file content into a dynamically allocated buffer (null terminated)
// The buffer must be freed by the user
// outFileSize is an optional parameter, it will contain the file size
char_t* GetFileContent(char_t* path, uint64_t* outFileSize)
{
    char_t* buffer = NULL;
    FILE* file = fopen(path, "r");
    if (file != NULL)
    {
        // Get file size
        uint64_t fileSize = GetFileSize(file);
        if (outFileSize != NULL)
        {
            *outFileSize = fileSize;
        }

        buffer = malloc(fileSize + 1);
        if (buffer != NULL)
        {
            fread(buffer, 1, fileSize, file);
            buffer[fileSize] = CHAR_NULL;
        }
        else
        {
            Log(LL_ERROR, 0, "Failed to create buffer to read file.");
        }
        fclose(file);
    }
    return buffer;
}


/*
* This Function recieves a file handle a file info handle,
* write file handle, its GUID, and its size
*/
efi_status_t GetFileInfo(efi_file_handle_t* fileHandle, efi_file_info_t* fileInfo)
{
    efi_guid_t infGuid = EFI_FILE_INFO_GUID;
    uintn_t size = sizeof(efi_file_info_t);
    return fileHandle->GetInfo(fileHandle, &infGuid, &size, (void*)fileInfo);
}

// Returns the size of the file in bytes
uint64_t GetFileSize(FILE* file)
{
    if (file == NULL)
    {
        return 0;
    }
    efi_file_info_t info;
    efi_status_t status = GetFileInfo(file, &info);
    if (EFI_ERROR(status)){
        Log(LL_ERROR, status, "Failed to get file info when getting file size.");
        return 0;
    }
    return info.FileSize;

}

/*
* Waits a certain number of ms (timeoutms) before returning
* or returning on key press
* (create a 10 second timer event, if a keypress is registered, cancell timer and return)
*/
int32_t WaitForInput(uint32_t timeout)
{
    // The first index is for the timer event, and the second is for WaitForKey event
    uintn_t idx;
    efi_event_t events[2];


    efi_event_t* timerEvent = events;  // Index 0 (Timer)
    events[1] = ST->ConIn->WaitForKey; // Index 1 (Input)

    efi_status_t status = BS->CreateEvent(EVT_TIMER, 0, NULL, NULL, timerEvent);
    if (EFI_ERROR(status))
    {
        Log(LL_ERROR, status, "Failed to create timer event.");
        return INPUT_TIMER_ERROR;
    }

    status = BS->SetTimer(*timerEvent, TimerRelative, timeout * 10000);
    if(EFI_ERROR(status))
    {
        Log(LL_ERROR, status, "Failed to set timer event (for %d milliseconds).", timeout);
        return INPUT_TIMER_ERROR;
    }
    // wait for input event
    status = BS->WaitForEvent(2, events, &idx);
    BS->CloseEvent(*timerEvent);

    if(EFI_ERROR(status))
    {
        Log(LL_ERROR, status, "Failed to set for timer event.");
        return INPUT_TIMER_ERROR;
    }
    else if(idx == 1)
    {
        return INPUT_TIMER_KEY;
    }
    return INPUT_TIMER_TIMEOUT;

  

}

void DisableWatchdogTimer(void)
{
    efi_status_t status = BS->SetWatchdogTimer(0, 0,0, NULL);
    if(EFI_ERROR(status))
    {
        Log(LL_WARNING, status, "Failed to disable watchdog timer.");
    }
}

