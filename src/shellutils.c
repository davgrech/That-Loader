#include "../include/shellutils.h"
#include "../include/logs.h"
#include "../include/bootutils.h"
#include "../include/ErrorCodes.h"
#include "../include/display.h"

#define DIRECTORY_DELIM ('\\')
#define DIRECTORY_DELIM_STR ("\\")
#define CURRENT_DIR (".")
#define PREVIOUS_DIR ("..")


/*
* This function concatenates two path sides
* lhs = "/usr/bin" + rhs = "/lib/lol" = /usr/bin/lib/lol
* lhs - left hand side, rhs - right hand side
* func caller has to free the char_t* buffer
*/
char_t* ConcatPaths(const char_t* lhs, const char_t* rhs)
{
    size_t lhsLen = strlen(lhs);
    size_t rhsLen = strlen(rhs);

    // null terminator + additinal '/' between the rhs and lhs
    char_t* newPath = malloc(lhsLen + rhsLen + 2);
    if(newPath == NULL)
    {
        Log(LL_ERROR, 0, "Failed to allocate memory for two path concatenation...");
        return NULL;
    }
    strncpy(newPath, lhs, lhsLen); // copy the left path to newly allocated bufer

    //before adding the right hand side, we have to make sure that only one side has has '/'
    // if the last char in lhs is not '/' and the first char in rhs is not '/', we have to append it
    size_t lhsLastIndex = strlen(lhs) -1;
    if(lhs[lhsLastIndex] != '\\' && rhs[0] != '\\')
    {
        strcpy(newPath + lhsLen, '\\', 1);
    }
    // Avoid duplicating '\\' in newPath
    if(lhs[lhsLastIndex] == rhs[0] == '\\')
    {
        rhs++; // advance pointer to next character
    }
    strncat(newPath + lhsLen, rhs, rhsLen);

    return newPath; // note the newPath wasnt freed, the once calling the function has to free it
}
// remove the "." and the ".." dirs from the given path
// shoutout chatgpt for this
uint8_t NormalizePath(char_t** path)
{
     // count the amount of tokens
    char_t* copy = *path;
    uint16_t tokenAmount = 0;
    while (*copy != CHAR_NULL)
    {
        if (*copy == DIRECTORY_DELIM)
        {
            tokenAmount++;
        }
        copy++;
    }
    
    // Nothing to normalize
    if (tokenAmount <= 1) 
    {
        return CMD_SUCCESS;
    }

    char_t** tokens = malloc(tokenAmount * sizeof(char_t*));
    if (tokens == NULL)
    {
        Log(LL_ERROR, 0, "Failed to allocate memory while normalizing the path.");
        return CMD_OUT_OF_MEMORY;
    }
    tokens[0] = NULL;
    char_t* token = NULL;
    char_t* srcCopy = *path + 1; // Pass the first character (which is always "\")
    uint16_t i = 0;
    // Evaluate the path
    while ((token = strtok_r(srcCopy, DIRECTORY_DELIM_STR, &srcCopy)) != NULL)
    {
        // Ignore the "." directory
        if (strcmp(token, CURRENT_DIR) == 0)
        {
            tokenAmount--;
        }
        // Go backwards in the path
        else if (strcmp(token, PREVIOUS_DIR) == 0)
        {
            if (tokenAmount > 0) 
            {
                tokenAmount--;
            }
            else
            {
                tokenAmount = 0;
            }

            // Don't go backwards past the beginning
            if (i > 0) 
            {
                i--;
            }

            if (tokens[i] != NULL)
            {
                if (tokenAmount > 0) 
                {
                    tokenAmount--;
                }
                free(tokens[i]);
                tokens[i] = NULL;
            }
        }
        else
        {
            tokens[i] = strdup(token);
            i++;
        }
    }
    // Rebuild the string
    (*path)[0] = '\\';
    (*path)[1] = CHAR_NULL;
    for (i = 0; i < tokenAmount; i++)
    {
        strcat(*path, tokens[i]);

        if (i + 1 != tokenAmount)
        {
            strcat(*path, "\\");
        }
            
        free(tokens[i]);
    }
    free(tokens);
    return CMD_SUCCESS;
}

// make path usable and clean
// get rid of unnecessary backslashes
void CleanPath(char_t** path)
{
    path = TrimSpaces(*path);

    //remove duplicate backslashes from the command
    RemoveRepeatedChars(*path, DIRECTORY_DELIM);

    // Remove a backslash from the end if it exists
    size_t lastIndex = strlen(*path) - 1;
    if ((*path)[lastIndex] == DIRECTORY_DELIM && lastIndex + 1 > 1) 
    {
        (*path)[lastIndex] = 0;
    }

}