#pragma once
#include <uefi.h>


typedef struct kernel_scan_info_s{
    char_t* kernelDirectory; // path to dir of the kernel
    char_t* kernelVersionString; 
} kernel_scan_info_s;

typedef struct boot_entry_s{
    char_t* name; // name of image (for the menu printing)
    char_t* imageToLoad; // Holds the path to the image to load
    char_t* imageArgs; // Holds the arguments for the image (if needed)

    boolean_t isDirectoryToKernel; // checks if imageToLoad is a dir or path to an image
    // without it we wont know if to scan a dir or simply load the image
    kernel_scan_info_s* kernelScanInfo;
} boot_entry_s;


typedef struct boot_entry_array_s{
    boot_entry_s* entryArray;
    int32_t numOfEntries;
} boot_entry_array_s;

boot_entry_array_s ParseConfig(void);
boolean_t ParseKeyValuePair(char_t* token, const char_t delimiter, char_t** key, char_t** value);
void FreeConfigEntries(boot_entry_array_s* entryArr);