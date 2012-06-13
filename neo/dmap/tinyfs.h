#pragma once

#include <stdio.h>

FILE* TFS_OpenFileRead(const char *filename);
void TFS_ReadFile(const char *filename, void **buffer);
char* TFS_OSPathToRelativePath(const char * path);
