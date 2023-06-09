#pragma once
#include <uefi.h>

// config file error codes
#define BAD_CONFIGURATION_ERR_MSG ("An error has occurred while parsing the config file.")

// booting error codes
#define FAILED_BOOT_ERR_MSG ("An error has occurred during the booting process.")


// CMD error codes
#define CMD_SUCCESS (0)
#define CMD_NO_FILE_SPECIFIED (35)
#define CMD_NO_DIR_SPEFICIED (36)
#define CMD_GENERAL_FILE_OPENING_ERROR (37)
#define CMD_GENERAL_DIR_OPENING_ERROR (38)
#define CMD_DIR_ALREADY_EXISTS (39)
#define CMD_DIR_NOT_FOUND (40)
#define CMD_CANT_READ_DIR (41)
#define CMD_OUT_OF_MEMORY (42)
#define CMD_NOT_FOUND (43)
#define CMD_BRIEF_HELP_NOT_AVAILABLE (44)
#define CMD_LONG_HELP_NOT_AVAILABLE (45)
#define CMD_QUOTATION_MARK_OPEN (46)
#define CMD_REBOOT_FAIL (47)
#define CMD_SHUTDOWN_FAIL (48)
#define CMD_REFUSE_REMOVE (49)
#define CMD_EFI_FAIL (50)
#define CMD_MISSING_SRC_FILE_OPERAND (51)
#define CMD_MISSING_DST_FILE_OPERAND (52)

const char_t* GetCommandErrorInfo(const uint8_t error);
void PrintCommandError(const char_t* cmd, const char_t* args, const uint8_t error);