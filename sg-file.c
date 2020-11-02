#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <stdint.h>

#include "sg-file.h"

SGFile *sg_file_open(const char *filename)
{
    gzFile rv;
    uint32_t endian_check;

    rv = gzopen(filename,"rb");
    if(!rv)
        return NULL;

    gzfread(&endian_check, sizeof(endian_check),1, rv);
    if(endian_check != ENDIAN_MAGIC){
        gzclose(rv);
        return NULL;
    }

    return rv;
}

int sg_file_close(SGFile *self)
{
    return gzclose(self);
}

void sg_file_read_next(SGFile *self, SGContainer *dest)
{

    uint32_t ui32Data=0;
    uint64_t ui64Data=0;
    int containerType = -1;

    dest->start_offset = gztell(self);

    gzfread(&ui32Data, sizeof(uint32_t), 1, self);
    gzfread(&ui64Data, sizeof(uint64_t), 1, self);

    dest->type = (int)ui32Data;
    dest->size = ui64Data;

    gzseek(self, dest->size, SEEK_CUR);
}

bool sg_file_get_payload(SGFile *self, SGContainer *container, uint8_t **payload)
{
    gzseek(self, sg_container_payload(container),SEEK_SET);
    if(!(*payload)){
        *payload = malloc(sizeof(uint8_t)*container->size);
        if(!*payload)
            return false;
    }
    gzfread(*payload, container->size, 1, self);
    return true;
}

void sg_container_dump(SGContainer *self)
{
    printf("SGContainer(%p): type: %d, start:%ld, size:%d\n",
        self,
        self->type,
        self->start_offset,
        self->size
    );
}
