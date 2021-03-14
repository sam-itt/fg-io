#ifndef GZ_READER_H
#define GZ_READER_H
#include <stdint.h>
#include <stdbool.h>

#include <zlib.h>

typedef struct{
    gzFile fp;
    bool read_error;

}GzReader;

#define GZ_READER(self) ((GzReader*)(self))

GzReader *gz_reader_new(const char *filename);
GzReader *gz_reader_init(GzReader *self, const char *filename);
GzReader *gz_reader_dispose(GzReader *self);
GzReader *gz_reader_free(GzReader *self);

static inline void gz_reader_clear_error(GzReader *self)
{
    self->read_error = false;
}

bool gz_reader_read_floats(GzReader *self, const size_t n, float *var);
bool gz_reader_read_doubles(GzReader *self, const size_t n, double *var);
bool gz_reader_read_bytes(GzReader *self, const size_t n, void *var);
bool gz_reader_read_uint16s(GzReader *self, const size_t n, uint16_t *var);
bool gz_reader_read_int16s(GzReader *self, const size_t n, int16_t *var);
bool gz_reader_read_uint32s(GzReader *self, const size_t n, uint32_t *var);
bool gz_reader_read_int32s(GzReader *self, const size_t n, int32_t *var);

static inline bool gz_reader_read_byte(GzReader *self, char *var)
{
    return gz_reader_read_bytes(self, 1, var);
}

static inline bool gz_reader_read_char(GzReader *self, char *var)
{
    return gz_reader_read_byte(self, var);
}

static inline bool gz_reader_read_float(GzReader *self, float *var)
{
    return gz_reader_read_floats(self, 1, var);
}

static inline bool gz_reader_read_double(GzReader *self, double *var)
{
    return gz_reader_read_doubles(self, 1, var);
}

static inline bool gz_reader_read_uint16(GzReader *self, uint16_t *var)
{
    return gz_reader_read_uint16s(self, 1, var);
}

static inline bool gz_reader_read_int16(GzReader *self, int16_t *var)
{
    return gz_reader_read_int16s(self, 1, var);
}

static inline bool gz_reader_read_uint32(GzReader *self, uint32_t *var)
{
    return gz_reader_read_uint32s(self, 1, var);
}

static inline bool gz_reader_read_int32(GzReader *self, int32_t *var)
{
    return gz_reader_read_int32s(self, 1, var);
}
#endif /* GZ_READER_H */
