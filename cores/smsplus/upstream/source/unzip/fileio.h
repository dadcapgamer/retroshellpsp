#ifndef FILEIO_H_
#define FILEIO_H_

#include <stdint.h>

/* Function prototypes */
uint8_t *loadFromZipByName(char *archive, char *filename, uint32_t *filesize);
int32_t check_zip(const char *filename);
//int gzsize(gzFile *gd);

#endif /* FILEIO_H_ */
