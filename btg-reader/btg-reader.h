#ifndef BTG_READER_H
#define BTG_READER_H
#include <stdint.h>
#include <stdbool.h>

#include "gz-reader.h"

typedef struct{
    double cx;
    double cy;
    double cz;
    float radius;
}BtgSphere;

typedef struct{
    float x;
    float y;
    float z;
}BtgVertex;

typedef struct{
    float x;
    float y;
    float z;
}BtgNormal;

typedef struct{
    float u;
    float v;
}BtgTextureCoordinate;

typedef struct{
    float r;
    float g;
    float b;
    float a;
}BtgColor;

typedef struct {
    char *material;
    /*mask for position/color/texcoords vertex attributes*/
    uint8_t va_mask;
    /*mask for generic(0..3 float/integer) vertex attributes*/
    uint32_t generic_va_mask;

    /* Number of attribute indices per vertex
     * if all vertices have a position idx and a color idx
     * stride will be 2.
     * */
    size_t stride;

    z_off_t offset;
    uint32_t nelements;
}BtgGeometryObject;


typedef struct{
    GzReader super;

    uint16_t version;
    int32_t nobjects;

    BtgSphere bs;

    BtgVertex *vertices;
    size_t nvertices;

    BtgNormal *normals;
    size_t nnormals;

    BtgTextureCoordinate *texcoords;
    size_t ntexcoords;

    BtgColor *colors;
    size_t ncolors;

    float *va_flt;
    size_t nva_flt;

    int32_t  *va_int; /*readInt -> int32_t ?*/
    size_t nva_int;

    BtgGeometryObject *points;
    size_t npoints;

    BtgGeometryObject *triangles;
    size_t ntriangles;

    BtgGeometryObject *triangle_strips;
    size_t ntriangle_strips;

    BtgGeometryObject *triangle_fans;
    size_t ntriangle_fans;
}BtgReader;

typedef struct{
    BtgReader *reader;

    bool has_positions;
    bool uint32_indices;

    size_t stride;
    size_t max_buffer_size;
    z_off_t fpos;
    z_off_t eoo; /*end of object*/
}BtgGeometryIterator;

BtgReader *btg_reader_new(const char *filename);
BtgReader *btg_reader_init(BtgReader *self, const char *filename);
BtgReader *btg_reader_dispose(BtgReader *self);
BtgReader *btg_reader_free(BtgReader *self);

bool btg_reader_init_geometry_iterator(BtgReader *self,
                                       BtgGeometryObject *object,
                                       BtgGeometryIterator *iterator);

bool btg_geometry_iterator_read(BtgGeometryIterator *self,
                                uint32_t *buffer, size_t *ngroups);

void btg_geometry_object_get_va_indices(BtgGeometryObject *self, int8_t *position,
                                        int8_t *normal, int8_t *color,
                                        int8_t *texcoord0, int8_t *texcoord1,
                                        int8_t *texcoord2, int8_t *texcoord3);

void btg_geometry_object_get_generic_va_indices(BtgGeometryObject *self,
                                                int8_t *iva0, int8_t *iva1,
                                                int8_t *iva2, int8_t *iva3,
                                                int8_t *fva0, int8_t *fva1,
                                                int8_t *fva2, int8_t *fva3);

#endif /* BTG_READER_H */
