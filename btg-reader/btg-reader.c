#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <zlib.h>

#include "btg-reader.h"

#define MAX(a,b) (((a)>(b))?(a):(b))

/*Private definiations*/
enum BtgObjectTypes {
    BTG_BOUNDING_SPHERE = 0,

    BTG_VERTEX_LIST = 1,
    BTG_NORMAL_LIST = 2,
    BTG_TEXCOORD_LIST = 3,
    BTG_COLOR_LIST = 4,
    BTG_VA_FLOAT_LIST = 6,
    BTG_VA_INTEGER_LIST = 6,

    BTG_POINTS = 9,

    BTG_TRIANGLE_FACES = 10,
    BTG_TRIANGLE_STRIPS = 11,
    BTG_TRIANGLE_FANS = 12,
    N_BTG_OBJECT_TYPES
};

/*Standard vertex attributes*/
enum BtgIndexTypes {
    BTG_IDX_VERTICES =    0x01,
    BTG_IDX_NORMALS =     0x02,
    BTG_IDX_COLORS =      0x04,
    BTG_IDX_TEXCOORDS_0 = 0x08,
    BTG_IDX_TEXCOORDS_1 = 0x10,
    BTG_IDX_TEXCOORDS_2 = 0x20,
    BTG_IDX_TEXCOORDS_3 = 0x40,
};

/*Generic vertex attributes*/
enum BtgVertexAttributeTypes {
    BTG_VA_INTEGER_0 = 0x00000001,
    BTG_VA_INTEGER_1 = 0x00000002,
    BTG_VA_INTEGER_2 = 0x00000004,
    BTG_VA_INTEGER_3 = 0x00000008,

    BTG_VA_FLOAT_0 =   0x00000100,
    BTG_VA_FLOAT_1 =   0x00000200,
    BTG_VA_FLOAT_2 =   0x00000400,
    BTG_VA_FLOAT_3 =   0x00000800,
};

/*Properties of Geometry Ojects*/
enum BtgPropertyTypes {
    BTG_MATERIAL = 0,
    BTG_INDEX_TYPES = 1,
    BTG_VERT_ATTRIBS = 2
};

typedef struct {
    char obj_type;
    uint32_t nproperties;
    uint32_t nelements;
}BtgObjectHeader;

static bool btg_reader_read_header(BtgReader *self);
static bool btg_reader_read_object_header(BtgReader *self,
                                          BtgObjectHeader *header);
static bool btg_reader_skip_object_properties(BtgReader *self,
                                              BtgObjectHeader *header);
static size_t btg_reader_get_object_size(BtgReader *self,
                                         BtgObjectHeader *header);
static bool btg_reader_read_bounding_sphere(BtgReader *self,
                                            BtgObjectHeader *header);
static bool btg_reader_read_vertex_list(BtgReader *self,
                                        BtgObjectHeader *header);
static bool btg_reader_read_color_list(BtgReader *self,
                                       BtgObjectHeader *header);
static bool btg_reader_read_normal_list(BtgReader *self,
                                        BtgObjectHeader *header);
static bool btg_reader_read_texcoord_list(BtgReader *self,
                                          BtgObjectHeader *header);
static bool btg_reader_read_va_float_list(BtgReader *self,
                                          BtgObjectHeader *header);
static bool btg_reader_read_va_int_list(BtgReader *self,
                                        BtgObjectHeader *header);
static bool btg_reader_skip_object(BtgReader *self,
                                   BtgObjectHeader *header);

static bool btg_reader_read_geometry_header(BtgReader *self,
                                            BtgObjectHeader *header,
                                            BtgGeometryObject *object);

static const char *pretty_type(enum BtgObjectTypes type);
/*end of private definitions*/

/*TODO macro+header inline*/
BtgReader *btg_reader_new(const char *filename)
{
    BtgReader *self;

    self = calloc(1, sizeof(BtgReader));
    if(self){
        if(!btg_reader_init(self, filename))
            return btg_reader_free(btg_reader_dispose(self));
    }
    return self;
}

BtgReader *btg_reader_init(BtgReader *self, const char *filename)
{
    void *rv;
    bool rv2;

    rv = gz_reader_init(GZ_READER(self), filename);
    if(!rv)
        return NULL;

    rv2 = btg_reader_read_header(self);
    if(!rv2)
        return NULL;

    z_off_t begin = gztell(GZ_READER(self)->fp);
    for(int i = 0; i < self->nobjects; i++ ){
        BtgObjectHeader oh;

        // read object header
        rv2 = btg_reader_read_object_header(self, &oh);
        if(!rv2){
            printf("Error reading object header %d, bailing out\n", i);
            return NULL;
        }
        if ( oh.obj_type == BTG_POINTS ) {
            self->npoints++;
            btg_reader_skip_object(self, &oh);
        }else if ( oh.obj_type == BTG_TRIANGLE_FACES ) {
            self->ntriangles++;
            btg_reader_skip_object(self, &oh);
        } else if ( oh.obj_type == BTG_TRIANGLE_STRIPS ) {
            self->ntriangle_strips++;
            btg_reader_skip_object(self, &oh);
        } else if ( oh.obj_type == BTG_TRIANGLE_FANS ) {
            self->ntriangle_fans++;
            btg_reader_skip_object(self, &oh);
        } else {
            btg_reader_skip_object(self, &oh);
        }
    }

    /*TODO: failure*/
    self->points = calloc(self->npoints, sizeof(BtgGeometryObject));
    self->triangles = calloc(self->ntriangles, sizeof(BtgGeometryObject));
    self->triangle_strips = calloc(self->ntriangle_strips, sizeof(BtgGeometryObject));
    self->triangle_fans = calloc(self->ntriangle_fans, sizeof(BtgGeometryObject));

    size_t current_point = 0;
    size_t current_triangle = 0;
    size_t current_triangle_strip = 0;
    size_t current_triangle_fan = 0;

    gzseek(GZ_READER(self)->fp, begin, SEEK_SET);
    for(int i = 0; i < self->nobjects; i++ ){
        BtgObjectHeader oh;

        // read object header
        z_off_t current_offset = gztell(GZ_READER(self)->fp);
        rv2 = btg_reader_read_object_header(self, &oh);
        if(!rv2){
            printf("Error reading object header %d, bailing out\n", i);
            return NULL;
        }

        /*printf("Object #%d: type %d(%s), %d props, %d elements\n",*/
            /*i,*/
            /*(int)oh.obj_type, pretty_type(oh.obj_type),*/
            /*oh.nproperties, oh.nelements*/
        /*);*/

        if ( oh.obj_type == BTG_BOUNDING_SPHERE ) {
            btg_reader_read_bounding_sphere(self, &oh);
        } else if ( oh.obj_type == BTG_VERTEX_LIST ) {
            btg_reader_read_vertex_list(self, &oh);
        } else if ( oh.obj_type == BTG_COLOR_LIST ) {
            btg_reader_read_color_list(self, &oh);
        } else if ( oh.obj_type == BTG_NORMAL_LIST ) {
            btg_reader_read_normal_list(self, &oh);
        } else if ( oh.obj_type == BTG_TEXCOORD_LIST ) {
            btg_reader_read_texcoord_list(self, &oh);
        } else if ( oh.obj_type == BTG_VA_FLOAT_LIST ) {
            btg_reader_read_va_float_list(self, &oh);
        } else if ( oh.obj_type == BTG_VA_INTEGER_LIST ) {
            btg_reader_read_va_int_list(self, &oh);
        } else if ( oh.obj_type == BTG_POINTS ) {
            btg_reader_read_geometry_header(self, &oh, &self->points[current_point++]);
        } else if ( oh.obj_type == BTG_TRIANGLE_FACES ) {
            btg_reader_read_geometry_header(self, &oh, &self->triangles[current_triangle++]);
        } else if ( oh.obj_type == BTG_TRIANGLE_STRIPS ) {
            btg_reader_read_geometry_header(self, &oh, &self->triangle_strips[current_triangle_strip++]);
        } else if ( oh.obj_type == BTG_TRIANGLE_FANS ) {
            btg_reader_read_geometry_header(self, &oh, &self->triangle_fans[current_triangle_fan++]);
        } else {
            btg_reader_skip_object(self, &oh);
        }

        if( GZ_READER(self)->read_error ){
            printf("Error while reading object %d\n",i);
        }
    }

    return self;
}

BtgReader *btg_reader_dispose(BtgReader *self)
{
    gz_reader_dispose(GZ_READER(self));

    if(self->vertices)
        free(self->vertices);
    if(self->normals)
        free(self->normals);
    if(self->texcoords)
        free(self->texcoords);
    if(self->colors)
        free(self->colors);

    for(int i = 0; i < self->npoints; i++){
        if(self->points[i].material)
            free(self->points[i].material);
    }

    for(int i = 0; i < self->ntriangles; i++){
        if(self->triangles[i].material)
            free(self->triangles[i].material);
    }

    for(int i = 0; i < self->ntriangle_strips; i++){
        if(self->triangle_strips[i].material)
            free(self->triangle_strips[i].material);
    }

    for(int i = 0; i < self->ntriangle_fans; i++){
        if(self->triangle_fans[i].material)
            free(self->triangle_fans[i].material);
    }

    return self;
}

/*TODO macro+header inline*/
BtgReader *btg_reader_free(BtgReader *self)
{
    free(btg_reader_dispose(self));
    return NULL;
}



bool btg_reader_init_geometry_iterator(BtgReader *self,
                                       BtgGeometryObject *object,
                                       BtgGeometryIterator *iterator)
{
    z_off_t eoe; /*end of element*/
    size_t nvertices; /*Each index group describe a vertex*/
    size_t group_nbytes;
    uint32_t nbytes;

    gzseek(GZ_READER(self)->fp, object->offset, SEEK_SET);
    iterator->fpos = object->offset;
    /*printf("Reading triangle at %lx, Material: %s\n",object->offset, object->material);*/
    iterator->stride = object->stride;
    iterator->uint32_indices = (self->version >= 10)
                                ? true
                                : false;
    iterator->has_positions = (object->va_mask & BTG_IDX_VERTICES)
                              ? true
                              : false;
    iterator->reader = self;
    iterator->max_buffer_size = 0;

    for(int j = 0; j < object->nelements; j++){
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        eoe = gztell(GZ_READER(self)->fp) + nbytes;
        if(GZ_READER(self)->read_error) break;

        if (self->version >= 10) {
            group_nbytes = object->stride * sizeof(uint32_t);
            nvertices = nbytes / (group_nbytes);
        } else {
            group_nbytes = object->stride * sizeof(uint16_t);
            nvertices = nbytes / (group_nbytes);

        }
        iterator->max_buffer_size = MAX(iterator->max_buffer_size, object->stride*nvertices*sizeof(uint32_t));
        gzseek(GZ_READER(self)->fp, eoe, SEEK_SET);
    }
    iterator->eoo = gztell(GZ_READER(self)->fp);
    /* We don't really need to rewind the file pointer as all reading functions start
     * by going to the offset they need and that is known in the read structs*/
    /*printf("Max buffer size: %d\n",iterator->max_buffer_size);*/
    return GZ_READER(self)->read_error ? false : true;
}

bool btg_geometry_iterator_read(BtgGeometryIterator *self,
                                uint32_t *buffer, size_t *ngroups)
{
    z_off_t finish;
    uint32_t nbytes;

    if(self->fpos >= self->eoo){
        printf("GeometryIterator trying to read after the end of the iterated object\n");
    }

    gzseek(GZ_READER(self->reader)->fp, self->fpos, SEEK_SET);
    /*printf("Reading element at %lx\n",self->fpos);*/

    gz_reader_read_uint32(GZ_READER(self->reader), &nbytes);
    if(GZ_READER(self->reader)->read_error) return false;
    finish = gztell(GZ_READER(self->reader)->fp) + nbytes;

    /* We are going to read group of indices that reference global lists
     * of known (positions, normals, etc.) and generic vertex attributes
     * These indices are grouped together by vertex.
     * One group describes one vertex.*/
    size_t nvertices; /*Each index group describe a vertex*/
    if (self->uint32_indices) {
        /* The group size (how many indices in the group/vertex)
         *
         * The number of set bits(1) in each mask determine the group size
         * in terms of either uint32_t or uint16_t depending on the btg version.
         *
         * Each element(in the BTG format parlance) can hold one or more vertex/group.
         * This group count is determined by the size of the element (nbytes) divided
         * by the byte size of one group.
         * */
        size_t group_nbytes = self->stride * sizeof(uint32_t);
        nvertices = nbytes / (group_nbytes);
        *ngroups= nvertices;

        gz_reader_read_uint32s(GZ_READER(self->reader),
            self->stride * nvertices,
            buffer
        );
    } else {
        size_t group_nbytes = self->stride * sizeof(uint16_t);
        nvertices = nbytes / (group_nbytes);
        *ngroups= nvertices;

        size_t bidx = 0;
        for(int i = 0; i < nvertices; i++){
            uint16_t tmp;
            for(int l = 0; l < self->stride; l++){
                off_t off = gztell(GZ_READER(self->reader)->fp);
                gz_reader_read_uint16(GZ_READER(self->reader), &tmp);
                buffer[bidx++] = tmp;
//                printf("%lx: read ushort: buffer[%d] = %d\n",gztell(GZ_READER(self->reader)->fp),bidx-1,buffer[bidx-1]);
            }
        }
//           printf("eol\n");
    }
    if(finish != gztell(GZ_READER(self->reader)->fp)){
        printf("WARNING finish != gztell !\n");
    }
    self->fpos = finish;

    if( (nvertices == 3 ) && self->has_positions ){
        int i0 = buffer[0];
        int i1 = buffer[self->stride];
        int i2 = buffer[2*self->stride];
        if(i0 == i1 || i1 == i2 || i2 == i0)
            return false;
    }

    return GZ_READER(self->reader)->read_error ? false : true;
}

void btg_geometry_object_get_va_indices(BtgGeometryObject *self, int8_t *position,
                                        int8_t *normal, int8_t *color,
                                        int8_t *texcoord0, int8_t *texcoord1,
                                        int8_t *texcoord2, int8_t *texcoord3)
{
    int8_t i = 0;

    if(position) *position = -1;
    if(normal) *normal = -1;
    if(color) *color = -1;
    if(texcoord0) *texcoord0 = -1;
    if(texcoord1) *texcoord1 = -1;
    if(texcoord2) *texcoord2 = -1;
    if(texcoord3) *texcoord3 = -1;

    if(self->va_mask & BTG_IDX_VERTICES){
        if(position) *position = i;
        i++;
    }
    if(self->va_mask & BTG_IDX_NORMALS){
        if(normal) *normal = i;
        i++;
    }
    if(self->va_mask & BTG_IDX_COLORS){
        if(color) *color = i;
        i++;
    }
    if(self->va_mask & BTG_IDX_TEXCOORDS_0){
        if(texcoord0) *texcoord0 = i;
        i++;
    }
    if(self->va_mask & BTG_IDX_TEXCOORDS_1){
        if(texcoord1) *texcoord1 = i;
        i++;
    }

    if(self->va_mask & BTG_IDX_TEXCOORDS_2){
        if(texcoord2) *texcoord2 = i;
        i++;
    }

    if(self->va_mask & BTG_IDX_TEXCOORDS_3){
        if(texcoord3) *texcoord3 = i;
        i++;
    }
    return;
}

void btg_geometry_object_get_generic_va_indices(BtgGeometryObject *self,
                                                int8_t *iva0, int8_t *iva1,
                                                int8_t *iva2, int8_t *iva3,
                                                int8_t *fva0, int8_t *fva1,
                                                int8_t *fva2, int8_t *fva3)
{
    int8_t i = 0;

    if(iva0) *iva0 = -1;
    if(iva1) *iva1 = -1;
    if(iva2) *iva2 = -1;
    if(iva3) *iva3 = -1;
    if(fva0) *fva0 = -1;
    if(fva1) *fva1 = -1;
    if(fva2) *fva2 = -1;
    if(fva3) *fva3 = -1;

    if(self->generic_va_mask & BTG_VA_INTEGER_0){
        if(iva0) *iva0 = i;
        i++;
    }
    if(self->generic_va_mask & BTG_VA_INTEGER_1){
        if(iva1) *iva1 = i;
        i++;
    }
    if(self->generic_va_mask & BTG_VA_INTEGER_2){
        if(iva2) *iva2 = i;
        i++;
    }
    if(self->generic_va_mask & BTG_VA_INTEGER_3){
        if(iva3) *iva3 = i;
        i++;
    }

    if(self->generic_va_mask & BTG_VA_FLOAT_0){
        if(fva0) *fva0 = i;
        i++;
    }
    if(self->generic_va_mask & BTG_VA_FLOAT_1){
        if(fva1) *fva1 = i;
        i++;
    }
    if(self->generic_va_mask & BTG_VA_FLOAT_2){
        if(fva2) *fva2 = i;
        i++;
    }
    if(self->generic_va_mask & BTG_VA_FLOAT_3){
        if(fva3) *fva3 = i;
        i++;
    }

    return;
}

static bool btg_reader_read_header(BtgReader *self)
{
    unsigned int header;
    unsigned int foo_calendar_time;

    // read headers
    gz_reader_read_uint32(GZ_READER(self), &header);
    if( ((header & 0xFF000000) >> 24) == 'S' && ((header & 0x00FF0000) >> 16) == 'G') {
        // read file version
        self->version = (header & 0x0000FFFF);
    }else{
        printf("Bad BTG magic/version\n");
        return false;
    }

    // read creation time
    gz_reader_read_uint32(GZ_READER(self), &foo_calendar_time);

    // read number of top level objects
    if(self->version >= 10){ // version 10 extends everything to be 32-bit
        gz_reader_read_int32(GZ_READER(self), &self->nobjects);
    } else if ( self->version >= 7 ) {
        uint16_t v;
        gz_reader_read_uint16(GZ_READER(self), &v);
        self->nobjects = v;
    } else {
        int16_t v;
        gz_reader_read_int16(GZ_READER(self), &v);
        self->nobjects = v;
    }

    /*printf("%s Total objects to read = %d\n",__FUNCTION__, self->nobjects);*/
    if(GZ_READER(self)->read_error){
        printf("Error reading BTG file header\n");
        return false;
    }

    return true;
}

static bool btg_reader_read_object_header(BtgReader *self, BtgObjectHeader *header)
{
    gz_reader_read_char(GZ_READER(self), &header->obj_type );
    if ( self->version >= 10 ) {
        gz_reader_read_uint32(GZ_READER(self), &header->nproperties );
        gz_reader_read_uint32(GZ_READER(self), &header->nelements );
    } else if ( self->version >= 7 ) {
        uint16_t v;
        gz_reader_read_uint16(GZ_READER(self), &v );
        header->nproperties = v;
        gz_reader_read_uint16(GZ_READER(self), &v );
        header->nelements = v;
    } else {
        int16_t v;
        gz_reader_read_int16(GZ_READER(self), &v );
        header->nproperties = v;
        gz_reader_read_int16(GZ_READER(self), &v );
        header->nelements = v;
    }
    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_skip_object_properties(BtgReader *self,
                                              BtgObjectHeader *header)
{
    uint32_t nbytes;

    // read properties
    for( int j = 0; j < header->nproperties; j++ ){
        char prop_type;
        gz_reader_read_char(GZ_READER(self), &prop_type);
        gz_reader_read_uint32(GZ_READER(self), &nbytes); /*uint = uint32_t*/
//        printf("property %d size = %d (skipping)\n", j, nbytes);
        gzseek(GZ_READER(self)->fp, nbytes, SEEK_CUR);
    }
    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_skip_object(BtgReader *self,
                                   BtgObjectHeader *header)
{
    btg_reader_skip_object_properties(self, header);
#if 0
    printf("Object of type %s: %d elements (skipping)\n",
        pretty_type(header->obj_type),
        header->nelements
    );
#endif
    for(int j = 0; j < header->nelements; j++){
        uint32_t nbytes;
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
#if 0
        printf("\tElement %d is %d bytes long (skipping)\n",
            j, nbytes
        );
#endif
        gzseek(GZ_READER(self)->fp, nbytes, SEEK_CUR);
    }

    return GZ_READER(self)->read_error ? false : true;
}

static size_t btg_reader_get_object_size(BtgReader *self,
                                         BtgObjectHeader *header)
{
    z_off_t begin;
    uint32_t e_size;
    size_t rv;

    rv = 0;
    begin = gztell(GZ_READER(self)->fp);
    for(int j = 0; j < header->nelements; j++){
        gz_reader_read_uint32(GZ_READER(self), &e_size);
        //TODO: Read error
        rv += e_size;
        gzseek(GZ_READER(self)->fp, e_size, SEEK_CUR);
    }
    gzseek(GZ_READER(self)->fp, begin, SEEK_SET);

    return rv;
}

static bool btg_reader_read_bounding_sphere(BtgReader *self,
                                            BtgObjectHeader *header)
{
    uint32_t nbytes;
    double unit[3];
    // read bounding sphere properties
    btg_reader_skip_object_properties(self, header);

    // read bounding sphere elements
    for(int j = 0; j < header->nelements; j++){
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        /*printf("Object of type %s element %d is %d bytes long\n",*/
            /*pretty_type(header->obj_type),*/
            /*j, nbytes*/
        /*);*/
        gz_reader_read_doubles(GZ_READER(self), 3, unit);
        self->bs.cx = unit[0];
        self->bs.cy = unit[1];
        self->bs.cz = unit[2];
        gz_reader_read_float(GZ_READER(self), &self->bs.radius);
    }

    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_read_vertex_list(BtgReader *self,
                                        BtgObjectHeader *header)
{
    uint32_t nbytes;

    // read vertex list properties
    btg_reader_skip_object_properties(self, header);

    nbytes = btg_reader_get_object_size(self, header);
    self->nvertices = nbytes / (sizeof(float) * 3);
    self->vertices = calloc(self->nvertices, sizeof(BtgVertex));
    if(!self->vertices){
        printf("%s: Couldn't allocate memory\n", __FUNCTION__);
        return false;
    }
    /*printf("Object of type %s: %d elements total size %d bytes\n",*/
        /*pretty_type(header->obj_type),*/
        /*header->nelements, nbytes*/
    /*);*/

    size_t start_idx = 0;
    for(int j = 0; j < header->nelements; j++){
        size_t element_size; /*not in bytes*/
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        element_size = nbytes / (sizeof(float)*3);
        /*printf("\tElement #%d is %d bytes long (%d vertices)\n",*/
            /*j, nbytes, element_size*/
        /*);*/
        if(sizeof(BtgVertex) == sizeof(float) * 3){/*TODO: static_assert*/
            /*printf("Reading vertices at once\n");*/
            gz_reader_read_floats(GZ_READER(self),
                element_size*3,
                (float*)(self->vertices + start_idx)
            );
        }else{
            float unit[3];
            int idx;
            for(int k = 0; k < element_size; k++ ){
                gz_reader_read_floats(GZ_READER(self), 3, unit);
                idx = start_idx + k;
                self->vertices[idx].x = unit[0];
                self->vertices[idx].y = unit[1];
                self->vertices[idx].z = unit[2];
            }
        }
        start_idx += element_size;
    }

    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_read_color_list(BtgReader *self,
                                       BtgObjectHeader *header)
{
    uint32_t nbytes;

    // read vertex list properties
    btg_reader_skip_object_properties(self, header);

    nbytes = btg_reader_get_object_size(self, header);
    self->ncolors = nbytes / (sizeof(float) * 4);
    self->colors = calloc(self->ncolors, sizeof(BtgColor));
    if(!self->colors){
        printf("%s: Couldn't allocate memory\n", __FUNCTION__);
        return false;
    }
    /*printf("Object of type %s: %d elements total size %d bytes\n",*/
        /*pretty_type(header->obj_type),*/
        /*header->nelements, nbytes*/
    /*);*/

    size_t start_idx = 0;
    for(int j = 0; j < header->nelements; j++){
        size_t element_size; /*not in bytes*/
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        element_size = nbytes / (sizeof(float)*4);
        /*printf("\tElement #%d is %d bytes long (%d colors)\n",*/
            /*j, nbytes, element_size*/
        /*);*/
        if(sizeof(BtgColor) == sizeof(float) * 4){/*TODO: static_assert*/
            /*printf("Reading colors at once\n");*/
            gz_reader_read_floats(GZ_READER(self),
                element_size*4,
                (float*)(self->colors + start_idx)
            );
        }else{
            float unit[4];
            int idx;
            for(int k = 0; k < element_size; k++ ){
                gz_reader_read_floats(GZ_READER(self), 4, unit);
                idx = start_idx + k;
                self->colors[idx].r = unit[0];
                self->colors[idx].g = unit[1];
                self->colors[idx].b = unit[2];
                self->colors[idx].a = unit[3];
            }
        }
        start_idx += element_size;
    }

    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_read_normal_list(BtgReader *self,
                                        BtgObjectHeader *header)
{
    uint32_t nbytes;

    btg_reader_skip_object_properties(self, header);

    nbytes = btg_reader_get_object_size(self, header);
    self->nnormals = nbytes / (sizeof(uint8_t)*3);
    self->normals = calloc(self->nnormals, sizeof(BtgNormal));
    if(!self->normals){
        printf("%s: Couldn't allocate memory\n", __FUNCTION__);
        return false;
    }

    size_t start_idx = 0;
    for(int j = 0; j < header->nelements; j++){
        size_t element_size; /*not in bytes*/
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        element_size = nbytes / (sizeof(uint8_t) * 3);

        int idx;
        uint8_t unit[3];
        for(int k = 0; k < element_size; k++ ){
            /* Normals are not stored as floats in the file.
             * BTG format spec states that each component
             * must be decoded as float_v = uint8v/127.5-1.0
             * Thus we must read and handle them one by one.
             * */
            gz_reader_read_bytes(GZ_READER(self), 3, unit);
            idx = start_idx + k;

            self->normals[idx].x = unit[0] / 127.5 - 1.0;
            self->normals[idx].y = unit[1] / 127.5 - 1.0;
            self->normals[idx].z = unit[2] / 127.5 - 1.0;

            /*Normalize*/
            float normv = sqrt(
                  (self->normals[idx].x*self->normals[idx].x)
                + (self->normals[idx].y*self->normals[idx].y)
                + (self->normals[idx].z*self->normals[idx].z)
            );
            self->normals[idx] = (normv <= FLT_MIN)
                ? (BtgNormal){0.0, 0.0, 0.0}
                : (BtgNormal){
                    .x = 1/normv*self->normals[idx].x,
                    .y = 1/normv*self->normals[idx].y,
                    .z = 1/normv*self->normals[idx].z
                };
        }
        start_idx += element_size;
    }

    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_read_texcoord_list(BtgReader *self,
                                          BtgObjectHeader *header)
{
    uint32_t nbytes;

    // read vertex list properties
    btg_reader_skip_object_properties(self, header);

    nbytes = btg_reader_get_object_size(self, header);
    self->ntexcoords = nbytes / (sizeof(float) * 2);
    self->texcoords = calloc(self->ntexcoords, sizeof(BtgTextureCoordinate));
    if(!self->texcoords){
        printf("%s: Couldn't allocate memory\n", __FUNCTION__);
        return false;
    }
    /*printf("Object of type %s: %d elements total size %d bytes\n",*/
        /*pretty_type(header->obj_type),*/
        /*header->nelements, nbytes*/
    /*);*/

    size_t start_idx = 0;
    for(int j = 0; j < header->nelements; j++){
        size_t element_size; /*not in bytes*/
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        element_size = nbytes / (sizeof(float)*2);
        /*printf("\tElement #%d is %d bytes long (%d texcoords)\n",*/
            /*j, nbytes, element_size*/
        /*);*/

        /*TODO: static_assert*/
        if(sizeof(BtgTextureCoordinate) == sizeof(float) * 2){
            /*printf("Reading texcoords at once\n");*/
            gz_reader_read_floats(GZ_READER(self),
                element_size*2,
                (float*)(self->texcoords + start_idx)
            );
        }else{
            float unit[2];
            int idx;
            for(int k = 0; k < element_size; k++ ){
                gz_reader_read_floats(GZ_READER(self), 2, unit);
                idx = start_idx + k;
                self->texcoords[idx].u = unit[0];
                self->texcoords[idx].v = unit[1];
            }
        }
        start_idx += element_size;
    }

    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_read_va_float_list(BtgReader *self,
                                          BtgObjectHeader *header)
{
    uint32_t nbytes;

    // read vertex list properties
    btg_reader_skip_object_properties(self, header);

    nbytes = btg_reader_get_object_size(self, header);
    self->nva_flt = nbytes / (sizeof(float));
    self->va_flt = calloc(self->nva_flt, sizeof(float));
    if(!self->va_flt){
        printf("%s: Couldn't allocate memory\n", __FUNCTION__);
        return false;
    }
    /*printf("Object of type %s: %d elements total size %d bytes\n",*/
        /*pretty_type(header->obj_type),*/
        /*header->nelements, nbytes*/
    /*);*/

    size_t start_idx = 0;
    for(int j = 0; j < header->nelements; j++){
        size_t element_size; /*not in bytes*/
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        element_size = nbytes / sizeof(float);
        /*printf("\tElement #%d is %d bytes long (%d floats)\n",*/
            /*j, nbytes, element_size*/
        /*);*/
        gz_reader_read_floats(GZ_READER(self),
            element_size,
            self->va_flt + start_idx
        );
        start_idx += element_size;
    }

    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_read_va_int_list(BtgReader *self,
                                        BtgObjectHeader *header)
{
    uint32_t nbytes;

    // read vertex list properties
    btg_reader_skip_object_properties(self, header);

    nbytes = btg_reader_get_object_size(self, header);
    self->nva_int = nbytes / (sizeof(int));
    self->va_int = calloc(self->nva_flt, sizeof(int));
    if(!self->va_int){
        printf("%s: Couldn't allocate memory\n", __FUNCTION__);
        return false;
    }
    /*printf("Object of type %s: %d elements total size %d bytes\n",*/
        /*pretty_type(header->obj_type),*/
        /*header->nelements, nbytes*/
    /*);*/

    size_t start_idx = 0;
    for(int j = 0; j < header->nelements; j++){
        size_t element_size; /*not in bytes*/
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        element_size = nbytes / sizeof(int);
        /*printf("\tElement #%d is %d bytes long (%d ints)\n",*/
            /*j, nbytes, element_size*/
        /*);*/
        gz_reader_read_int32s(GZ_READER(self),
            element_size,
            self->va_int + start_idx
        );
        start_idx += element_size;
    }

    return GZ_READER(self)->read_error ? false : true;
}

static bool btg_reader_read_geometry_header(BtgReader *self,
                                            BtgObjectHeader *header,
                                            BtgGeometryObject *object)
{
    int material_len;

    /*Default values*/
    object->va_mask = (header->obj_type == BTG_POINTS)
             ? BTG_IDX_VERTICES
             : (uint8_t)(BTG_IDX_VERTICES | BTG_IDX_TEXCOORDS_0);
    object->generic_va_mask = 0;

    for(int j = 0; j < header->nproperties; j++ ){
        char prop_type;
        uint32_t nbytes;

        gz_reader_read_char(GZ_READER(self), &prop_type);
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        switch(prop_type){
            case BTG_MATERIAL:
                material_len = (nbytes <= 255) ? nbytes : 255;
                object->material = malloc((material_len+1)*sizeof(char));
                gz_reader_read_bytes(GZ_READER(self),
                    material_len,
                    object->material
                );
                object->material[material_len] = '\0';
                if(nbytes > material_len)
                    gzseek(GZ_READER(self)->fp, nbytes-material_len, SEEK_CUR);
                break;

            case BTG_INDEX_TYPES:
                if(nbytes == 1)
                    gz_reader_read_char(GZ_READER(self), (char*)&(object->va_mask));
                else
                    gzseek(GZ_READER(self)->fp, nbytes, SEEK_CUR); /*TODO: Error/message ?*/
                break;

            case BTG_VERT_ATTRIBS:
                if(nbytes == 4)
                    gz_reader_read_uint32(GZ_READER(self), &(object->generic_va_mask));
                else
                    gzseek(GZ_READER(self)->fp, nbytes, SEEK_CUR); /*TODO: Error/message ?*/
                break;

            default:
                gzseek(GZ_READER(self)->fp, nbytes, SEEK_CUR); /*TODO: Error/message ?*/
                printf("Found UNKNOWN property type with nbytes == %d mask is %d\n", nbytes, (int)object->va_mask);
                break;
        }
    }

    /* The group size/stride (how many indices in the group/vertex)
     * controlled by masks.
     * Example 1:
     * -First index is position (position flag bit is set in the mask)
     * -Second index is normal(normal flag bit is set in the mask).
     * -Third index is color (color flag bit is set in the mask)
     * Example 2:
     * -First index is position (position flag bit is set in the mask)
     * -Second index is color (color flag bit is set in the mask)
     * Example 3:
     * -First index is position (position flag bit is set in the mask)
     * -Second index is normal(normal flag bit is set in the mask).
     * -Third index is texcoords (texcoord0 flag bit is set in the mask)
     *
     * The sequence is never changes(position/normal/color/texcoords),
     * but one index can be present or not depeding on the corresponding
     * bit in the mask. i.e texcoords will never be before posistion.
     * */
    object->stride = __builtin_popcount(object->va_mask)
                   + __builtin_popcount(object->generic_va_mask);

    object->offset = gztell(GZ_READER(self)->fp);
    object->nelements = header->nelements;
    /*printf("%s: triangle at %lx\n",__PRETTY_FUNCTION__, gztell(GZ_READER(self)->fp));*/
    /*Advance to the next object*/
    for(int j = 0; j < header->nelements; j++){
        uint32_t nbytes;
        gz_reader_read_uint32(GZ_READER(self), &nbytes);
        gzseek(GZ_READER(self)->fp, nbytes, SEEK_CUR);
    }

    return GZ_READER(self)->read_error ? false : true;
}

static const char *pretty_type(enum BtgObjectTypes type)
{
    if(type == BTG_BOUNDING_SPHERE) return "BTG_BOUNDING_SPHERE";
    if(type == BTG_VERTEX_LIST) return "BTG_VERTEX_LIST";
    if(type == BTG_NORMAL_LIST) return "BTG_NORMAL_LIST";
    if(type == BTG_TEXCOORD_LIST) return "BTG_TEXCOORD_LIST";
    if(type == BTG_COLOR_LIST) return "BTG_COLOR_LIST";
    if(type == BTG_VA_FLOAT_LIST) return "BTG_VA_FLOAT_LIST";
    if(type == BTG_VA_INTEGER_LIST) return "BTG_VA_INTEGER_LIST";
    if(type == BTG_POINTS) return "BTG_POINTS";
    if(type == BTG_TRIANGLE_FACES) return "BTG_TRIANGLE_FACES";
    if(type == BTG_TRIANGLE_STRIPS) return "BTG_TRIANGLE_STRIPS";
    if(type == BTG_TRIANGLE_FANS) return "BTG_TRIANGLE_FANS";
    return "UNKOWN TYPE";
}

