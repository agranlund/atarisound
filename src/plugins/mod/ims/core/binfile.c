// minimal binfile replacement

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "binfile.h"

long bf_load(binfile* bf, const char* filename) {
    bf->flags = 0;
    int fhandle = open(filename, 0);
    if (fhandle > 0) {
        bf->pos = 0;
        bf->len = lseek(fhandle, 0, SEEK_END);
        bf->data = malloc(bf->len);
        if (bf->data) {
            lseek(fhandle, 0, SEEK_SET);
            read(fhandle, bf->data, bf->len);
            bf->flags = 3;
        }
        close(fhandle);
    }
    return bf->flags;
}

long bf_init(binfile* bf, unsigned long len) {
    bf->flags = 0;
    bf->pos = 0;
    bf->len = len;
    bf->data = malloc(len);
    if(bf->data) {
        bf->flags |= 3;
    }
    return bf->flags;
}

long bf_initref(binfile* bf, unsigned char* data, unsigned long len) {
    bf->pos = 0;
    bf->len = len;
    bf->data = data;
    bf->flags = bf->data ? 1 : 0;
    return bf->flags;
}

void bf_close(binfile* bf) {
    if (bf->data && (bf->flags & 2))
        free(bf->data);
    bf->flags = 0;
    bf->pos = 0;
    bf->len = 0;
    bf->data = 0;
}

int bf_eof(binfile* bf) {
    return (bf->pos >= bf->len) ? 1 : 0;
}

void bf_read(binfile* bf, void* dst, unsigned long len)
{
    memcpy(dst, &(bf->data[bf->pos]), len);
    bf->pos += len;
}

void bf_seekcur(binfile* bf, long offs) {
    bf->pos += offs;
}

void bf_seek(binfile* bf, long pos) {
    bf->pos = pos;
}

long bf_getl(binfile* bf) {
    unsigned long le;
    bf_read(bf, &le, 4);
    unsigned long be =
        ((le & 0xff000000) >> 24) |
        ((le & 0x00ff0000) >>  8) |
        ((le & 0x0000ff00) <<  8) |
        ((le & 0x000000ff) << 24);
    return be;
}

