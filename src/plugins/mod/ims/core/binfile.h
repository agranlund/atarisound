#ifndef __BINFILE_H
#define __BINFILE_H

typedef struct
{
    unsigned char* data;
    unsigned long len;
    long pos;
    long flags;
} binfile;

long bf_load(binfile* bf, const char* filename);
long bf_init(binfile* bf, unsigned long len);
long bf_initref(binfile* bf, unsigned char* data, unsigned long len);


int bf_eof(binfile* bf);

void bf_read(binfile* bf, void* dst, unsigned long len);
void bf_seekcur(binfile* bf, long offs);

void bf_seek(binfile* bf, long pos);
long bf_getl(binfile* bf);

#endif
