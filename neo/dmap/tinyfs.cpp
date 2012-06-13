#include "tinyfs.h"
#include "stdlib.h"
#include "string.h"

FILE* TFS_OpenFileRead(const char *filename)
{
	return fopen(filename, "r");
}

void TFS_ReadFile(const char *filename, void **buffer)
{
	FILE *f = fopen(filename, "rb");
	fseek(f, 0, SEEK_END);
	long length = ftell(f);

	*buffer = malloc(length);
	char *dst = (char*) *buffer;

	long dataRead = 0;
	while(dataRead < length) {
		int result = fread(dst + dataRead, 1, length - dataRead, f);
		if(result == -1) {
			free(*buffer);
			*buffer = NULL;
			return;
		}
		dataRead += result;
	}
}

char* TFS_OSPathToRelativePath(const char * path)
{
	char *dst = (char*) malloc(strlen(path) + 1);
	strcpy(dst, path);
	return dst;
}
