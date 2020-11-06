/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2020 Samuel Cuella */

#include <stdio.h>
#include <stdlib.h>
#include <zconf.h>
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

bool sg_file_read_next(SGFile *self, SGContainer *dest)
{

    uint32_t ui32Data=0;
    uint64_t ui64Data=0;
    int containerType = -1;

    dest->start_offset = gztell(self);

    gzfread(&ui32Data, sizeof(uint32_t), 1, self);
    gzfread(&ui64Data, sizeof(uint64_t), 1, self);

    if(gzeof(self))
        return false;

    dest->type = (int)ui32Data;
    dest->size = ui64Data;

    gzseek(self, dest->size, SEEK_CUR);
    return true;
}

/* *
 * Searches for the first encountered container of type type
 * in the file, starting at current position.
 *
 * If not found, the find is rewinded and searched again.
 *
 * returns: true on success, false on failure. On success
 * dest is filed in. On failure it's undefined.
 *
 */
bool sg_file_get_container(SGFile *self, int type, SGContainer *dest)
{
    bool rewound = false;

begin:
    while(sg_file_read_next(self, dest)){
        if(dest->type == type){
            return true;
        }
    }
    //We've reached the end of the file, let's try again.
    if(!rewound){
        rewound = true;
        gzseek(self, 0 + sizeof(uint32_t), SEEK_SET); //Skip the endian magic check
        goto begin;
    }

    return false;
}

/**
 * Puts the payload in the given pointer. If not NULL, payload must
 * hold the adresse of a pointer to a large enough chunk to store the
 * container's content.
 *
 * if payload is NULL the area will be malloc'ed.
 *
 * The caller MUST free payload after use.
 *
 * Returns true on success, false on failure.
 */
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
