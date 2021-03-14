#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <zlib.h>

#include "gz-reader.h"

GzReader *gz_reader_new(const char *filename)
{
    GzReader *self;

    self = calloc(1, sizeof(GzReader));
    if(!self){
        if(!gz_reader_init(self, filename))
            return gz_reader_free(self);
    }
    return self;
}

GzReader *gz_reader_init(GzReader *self, const char *filename)
{
    char *with_gz;

    self->fp = gzopen(filename, "rb");
    if(!self->fp){ /*TODO: Check if filename doesn't already ends with '.gz'*/
        asprintf(&with_gz, "%s.gz", filename);
        self->fp = gzopen(with_gz, "rb");
        free(with_gz);
    }

    return self->fp ? self : NULL;
}

GzReader *gz_reader_dispose(GzReader *self)
{
    if(!self->fp)
        gzclose(self->fp);
    return self;
}

GzReader *gz_reader_free(GzReader *self)
{
    gz_reader_dispose(self);
    return NULL;
}

bool gz_reader_read_floats(GzReader *self, const size_t n, float *var)
{
    if( gzread(self->fp, var, sizeof(float)*n) != (int)sizeof(float)*n ){
        self->read_error = true;
        return false;
    }
#if TARGET_BIG_ENDIAN
    /*Here do a swap on each float from 0 to n, as a uint32_t*/
#endif
    return true;
}

bool gz_reader_read_doubles(GzReader *self, const size_t n, double *var)
{
    if( gzread(self->fp, var, sizeof(double)*n) != (int)sizeof(double)*n ){
        self->read_error = true;
        return false;
    }
#if TARGET_BIG_ENDIAN
    /*Here do a swap on each double from 0 to n, as a uint64_t*/
#endif
    return true;
}

bool gz_reader_read_bytes(GzReader *self, const size_t n, void *var)
{
    if(n == 0) return true;
    if( gzread(self->fp, var, n) != (int)n ){
        self->read_error = true;
        return false;
    }
    return true;
}


bool gz_reader_read_uint16s(GzReader *self, const size_t n, uint16_t *var)
{
    if( gzread(self->fp, var, sizeof(uint16_t)*n) != (int)sizeof(uint16_t)*n ){
        self->read_error = true;
        return false;
    }
#if TARGET_BIG_ENDIAN
    /*Here do a swap on each unsigned short from 0 to n, as a uint16_t*/
#endif
    return true;
}

bool gz_reader_read_int16s(GzReader *self, const size_t n, int16_t *var)
{
    if( gzread(self->fp, var, sizeof(int16_t)*n) != (int)sizeof(int16_t)*n ){
        self->read_error = true;
        return false;
    }
#if TARGET_BIG_ENDIAN
    /*Here do a swap on each short from 0 to n, as a uint16_t*/
#endif
    return true;
}

bool gz_reader_read_uint32s(GzReader *self, const size_t n, uint32_t *var)
{
    if( gzread(self->fp, var, sizeof(uint32_t)*n) != (int)sizeof(uint32_t)*n ){
        self->read_error = true;
        return false;
    }
#if TARGET_BIG_ENDIAN
    /*Here do a swap on each unsigned int from 0 to n, as a uint32_t*/
#endif
    return true;
}

bool gz_reader_read_int32s(GzReader *self, const size_t n, int32_t *var)
{
    if( gzread(self->fp, var, sizeof(int32_t)*n) != (int32_t)sizeof(int32_t)*n ){
        self->read_error = true;
        return false;
    }
#if TARGET_BIG_ENDIAN
    /*Here do a swap on each int from 0 to n, as a uint32_t*/
#endif
    return true;
}

