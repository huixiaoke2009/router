#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <zlib.h>

class CGzip
{
    public:
    static int gzip_uncompress(const char *bufin, int lenin, char *bufout, int lenout);
    static int gzip_compress(const char *bufin, int lenin, char *bufout, int lenout);
};
