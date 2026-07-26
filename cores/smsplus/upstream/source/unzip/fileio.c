/*
    fileio.c --
    File management.
*/
#include "shared.h"
#include "unzip.h"

uint8_t *loadFromZipByName(char *archive, char *filename, uint32_t *filesize)
{
    char name[PATH_MAX];
    uint8_t *buffer;

    int32_t zerror = UNZ_OK;
    unzFile zhandle;
    unz_file_info zinfo;
    
    zinfo.uncompressed_size = 0;

    zhandle = unzOpen(archive);
    if(!zhandle) return (NULL);

    /* Seek to first file in archive */
    zerror = unzGoToFirstFile(zhandle);
    if(zerror != UNZ_OK)
    {
        unzClose(zhandle);
        return (NULL);
    }

    /* Get information about the file */
    unzGetCurrentFileInfo(zhandle, &zinfo, &name[0], 0xff, NULL, 0, NULL, 0);
    
    /* Force console if file extension is detected */
    if (strcmp(strrchr(&name[0], '.'), ".col") == 0) option.console = 6;
    else if (strcmp(strrchr(&name[0], '.'), ".gg") == 0) option.console = 3;
    
    *filesize = zinfo.uncompressed_size;

    /* Error: file size is zero */
    if(*filesize == 0)
    {
        unzClose(zhandle);
        return (NULL);
    }

    /* Open current file */
    zerror = unzOpenCurrentFile(zhandle);
    if(zerror != UNZ_OK)
    {
        unzClose(zhandle);
        return (NULL);
    }

    /* Allocate buffer and read in file */
    buffer = malloc(*filesize);
    if(!buffer) return (NULL);
    zerror = unzReadCurrentFile(zhandle, buffer, *filesize);

    /* Internal error: free buffer and close file */
    if(zerror < 0 || zerror != (int32_t)*filesize)
    {
        free(buffer);
        buffer = NULL;
        unzCloseCurrentFile(zhandle);
        unzClose(zhandle);
        return (NULL);
    }

    /* Close current file and archive file */
    unzCloseCurrentFile(zhandle);
    unzClose(zhandle);

	memcpy(filename, name, PATH_MAX);
    return (buffer);
}

/*
    Verifies if a file is a ZIP archive or not.
    Returns: 1= ZIP archive, 0= not a ZIP archive
*/
int32_t check_zip(const char *filename)
{
    uint8_t buf[2];
    FILE* fd = NULL;
    fd = fopen(filename, "rb");
    if(!fd) return (0);
    
    fread(buf, 2, 1, fd);
    fclose(fd);
    if(memcmp(buf, "PK", 2) == 0) return (1);
    return (0);
}
