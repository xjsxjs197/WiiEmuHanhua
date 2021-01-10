#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <fat.h>

#include "../MessageBox.h"

// select Bios by game name
char * GetGameBios(char * biosPath, char * fileName)
{
    char * retVal = "/SCPH1001.BIN";

    char * tstLine = (char *)malloc(strlen(biosPath) + strlen(fileName) + 6);
    if (!tstLine)
    {
        return retVal;
    }
    char * tmpPtr = tstLine;
    memcpy(tstLine, biosPath, strlen(biosPath));
    tmpPtr += strlen(biosPath);
    *tmpPtr = '//';
    *(tmpPtr + 1) = '\0';

    strcat(tstLine, fileName);
    strcat(tstLine, ".bin");

    FILE* f = fopen(tstLine, "rb" );  //attempt to open file
    if (f) {

        fclose(f);

        memset(tstLine, 0, strlen(tstLine));
        *tstLine = '/';
        *(tstLine + 1) = '\0';
        strcat(tstLine, fileName);
        strcat(tstLine, ".bin");

        return tstLine;
    }
    else
    {
        return retVal;
    }

    return retVal;
}
