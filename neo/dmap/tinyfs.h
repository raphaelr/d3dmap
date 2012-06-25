#pragma once

#include <stdio.h>

FILE* TFS_OpenFileRead(const char *filename);
FILE* TFS_OpenFileWrite(const char *filename);
void TFS_ReadFile(const char *filename, void **buffer, int *length = NULL);
char* TFS_OSPathToRelativePath(const char * path);
