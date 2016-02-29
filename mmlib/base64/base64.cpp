#include "base64.h"
#include <stdlib.h>
#include <limits.h>

static inline unsigned char to_uchar(char ch)
{
    return ch;
}

/* Base64 encode IN array of size INLEN into OUT array of size OUTLEN.
 If OUTLEN is less than BASE64_LENGTH(INLEN), write as many bytes as
 possible.  If OUTLEN is larger than BASE64_LENGTH(INLEN), also zero
 terminate the output buffer. */
void base64_encode(const char *in, size_t inlen, char *out, size_t outlen)
{
#if URL_BASE64
    static const char b64str[65] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789*-";
#else
    static const char b64str[65] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#endif
    while (inlen && outlen)
    {
        *out++ = b64str[(to_uchar(in[0]) >> 2) & 0x3f];
        if (!--outlen)
            break;

        *out++ = b64str[((to_uchar(in[0]) << 4) + (--inlen ? to_uchar(in[1]) >> 4 : 0)) & 0x3f];
        if (!--outlen)
            break;

        *out = (inlen ? b64str[((to_uchar(in[1]) << 2) + (--inlen ? to_uchar(in[2]) >> 6 : 0))
            & 0x3f] : '=');
#if URL_BASE64
        if (*out == '=')
            break;
#endif
        out++;
        if (!--outlen)
            break;

        *out = inlen ? b64str[to_uchar(in[2]) & 0x3f] : '=';
#if URL_BASE64
        if (*out == '=')
            break;
#endif
        out++;
        if (!--outlen)
            break;

        if (inlen)
            inlen--;
        if (inlen)
            in += 3;
    }

    if (outlen)
        *out = '\0';
}

/* Allocate a buffer and store zero terminated base64 encoded data
 from array IN of size INLEN, returning BASE64_LENGTH(INLEN), i.e.,
 the length of the encoded data, excluding the terminating zero.  On
 return, the OUT variable will hold a pointer to newly allocated
 memory that must be deallocated by the caller.  If output string
 length would overflow, 0 is returned and OUT is set to NULL.  If
 memory allocation failed, OUT is set to NULL, and the return value
 indicates length of the requested memory block, i.e.,
 BASE64_LENGTH(inlen) + 1. */
size_t base64_encode_alloc(const char *in, size_t inlen, char **out)
{
    size_t outlen = 1 + BASE64_LENGTH (inlen);

    /* Check for overflow in outlen computation.
     *
     * If there is no overflow, outlen >= inlen.
     *
     * If the operation (inlen + 2) overflows then it yields at most +1, so
     * outlen is 0.
     *
     * If the multiplication overflows, we lose at least half of the
     * correct value, so the result is < ((inlen + 2) / 3) * 2, which is
     * less than (inlen + 2) * 0.66667, which is less than inlen as soon as
     * (inlen > 4).
     */
    if (inlen > outlen)
    {
        *out = NULL;
        return 0;
    }

    *out = (char *)malloc(outlen);
    if (!*out)
        return outlen;

    base64_encode(in, inlen, *out, outlen);

    return outlen - 1;
}

#if URL_BASE64
static const signed char b64[0x100] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, 63, -1, -1,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
#else
static const signed char b64[0x100] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
#endif

#if UCHAR_MAX == 255
# define uchar_in_range(c) true
#else
# define uchar_in_range(c) ((c) <= 255)
#endif

bool isbase64(char ch)
{
    return uchar_in_range (to_uchar (ch)) && 0 <= b64[to_uchar(ch)];
}

/* Decode base64 encoded input array IN of length INLEN to output
 array OUT that can hold *OUTLEN bytes.  Return true if decoding was
 successful, i.e. if the input was valid base64 data, false
 otherwise.  If *OUTLEN is too small, as many bytes as possible will
 be written to OUT.  On return, *OUTLEN holds the length of decoded
 bytes in OUT.  Note that as soon as any non-alphabet characters are
 encountered, decoding is stopped and false is returned.  This means
 that, when applicable, you must remove any line terminators that is
 part of the data stream before calling this function.  */
bool base64_decode(const char *in, size_t inlen, char *out, size_t *outlen)
{
    size_t outleft = *outlen;

    while (inlen >= 2)
    {
        if (!isbase64(in[0]) || !isbase64(in[1]))
            break;

        if (outleft)
        {
            *out++ = ((b64[to_uchar(in[0])] << 2) | (b64[to_uchar(in[1])] >> 4));
            outleft--;
        }

        if (inlen == 2)
            break;

        if (in[2] == '=')
        {
            if (inlen != 4)
                break;

            if (in[3] != '=')
                break;

        }
        else
        {
            if (!isbase64(in[2]))
                break;

            if (outleft)
            {
                *out++ = (((b64[to_uchar(in[1])] << 4) & 0xf0) | (b64[to_uchar(in[2])] >> 2));
                outleft--;
            }

            if (inlen == 3)
                break;

            if (in[3] == '=')
            {
                if (inlen != 4)
                    break;
            }
            else
            {
                if (!isbase64(in[3]))
                    break;

                if (outleft)
                {
                    *out++ = (((b64[to_uchar(in[2])] << 6) & 0xc0) | b64[to_uchar(in[3])]);
                    outleft--;
                }
            }
        }

        in += 4;
        inlen -= 4;
    }

    *outlen -= outleft;

#if URL_BASE64
    return true;
#else
    if (inlen != 0)
        return false;

    return true;
#endif
}

/* Allocate an output buffer in *OUT, and decode the base64 encoded
 data stored in IN of size INLEN to the *OUT buffer.  On return, the
 size of the decoded data is stored in *OUTLEN.  OUTLEN may be NULL,
 if the caller is not interested in the decoded length.  *OUT may be
 NULL to indicate an out of memory error, in which case *OUTLEN
 contains the size of the memory block needed.  The function returns
 true on successful decoding and memory allocation errors.  (Use the
 *OUT and *OUTLEN parameters to differentiate between successful
 decoding and memory error.)  The function returns false if the
 input was invalid, in which case *OUT is NULL and *OUTLEN is
 undefined. */
bool base64_decode_alloc(const char *in, size_t inlen, char **out, size_t *outlen)
{
    /* This may allocate a few bytes too much, depending on input,
     but it's not worth the extra CPU time to compute the exact amount.
     The exact amount is 3 * inlen / 4, minus 1 if the input ends
     with "=" and minus another 1 if the input ends with "==".
     Dividing before multiplying avoids the possibility of overflow.  */
    size_t needlen = 3 * (inlen / 4) + 2;

    *out = (char *)malloc(needlen);
    if (!*out)
        return true;

    if (!base64_decode(in, inlen, *out, &needlen))
    {
        free(*out);
        *out = NULL;
        return false;
    }

    if (outlen)
        *outlen = needlen;

    return true;
}
