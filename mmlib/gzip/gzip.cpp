#include "gzip.h"
#define err(msg) perror(msg)
int CGzip::gzip_uncompress(const char *bufin, int lenin, char *bufout, int lenout)
{
    z_stream d_stream;
    int result;
    memset(bufout, '\0', lenout);
    d_stream.zalloc = NULL;
    d_stream.zfree  = NULL;
    d_stream.opaque = NULL;

    result = inflateInit2(&d_stream, MAX_WBITS + 16);
    d_stream.next_in   =(Bytef *) bufin;
    d_stream.avail_in  = lenin;
    d_stream.next_out  =(Bytef *) bufout;
    d_stream.avail_out = lenout;

    inflate(&d_stream, Z_SYNC_FLUSH);
    inflateEnd(&d_stream);
    return d_stream.total_out;
}

int CGzip::gzip_compress(const char *bufin, int lenin, char *bufout, int lenout)
{
    z_stream d_stream;
    int result;
    memset(bufout, '\0', lenout);
    d_stream.zalloc = NULL;
    d_stream.zfree  = NULL;
    d_stream.opaque = NULL;

    result = deflateInit2(&d_stream, Z_DEFAULT_COMPRESSION,
            Z_DEFLATED, MAX_WBITS + 16,
            9, Z_DEFAULT_STRATEGY);
    if (result != 0) {
        err("deflateInit2");
        return -1;
    }
    d_stream.next_in   = (Bytef *)bufin;
    d_stream.avail_in  = lenin;
    d_stream.next_out  = (Bytef *)bufout;
    d_stream.avail_out = lenout;

    deflate(&d_stream, /*Z_SYNC_FLUSH*/Z_FINISH);
    deflateEnd(&d_stream);

    return d_stream.total_out;
}
