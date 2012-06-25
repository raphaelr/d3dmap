#include "tinyfs.h"
#include "stdlib.h"
#include "string.h"

FILE* TFS_OpenFileRead(const char *filename)
{
	return fopen(filename, "r");
}

FILE* TFS_OpenFileWrite(const char *filename)
{
	return fopen(filename, "w");
}

void TFS_ReadFile(const char *filename, void **buffer, int *outLength)
{
	outLength = 0;
	FILE *f = fopen(filename, "rb");
	if(!f) {
		*buffer = NULL;
		return;
	}

	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);

	*buffer = malloc(length);
	char *dst = (char*) *buffer;

	int result = fread(dst, 1, length, f);
	if(result != length) {
		free(*buffer);
		*buffer = NULL;
		return;
	}
	
	if(outLength) {
		*outLength = length;
	}
}

char* TFS_OSPathToRelativePath(const char * path)
{
	char *dst = (char*) malloc(strlen(path) + 1);
	strcpy(dst, path);
	return dst;
}
