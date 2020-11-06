/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2020 Samuel Cuella */

#ifndef SG_CONTAINER_FILE_H
#define SG_CONTAINER_FILE_H

#include <stdint.h>
#include <stdbool.h>
#include <zlib.h>

#define ENDIAN_MAGIC (uint32_t)0x11223344

typedef struct gzFile_s SGFile;

typedef struct{
    int type;
    off_t start_offset;
    size_t size;
}SGContainer;

#define sg_container_payload(self) ((self)->start_offset+sizeof(uint32_t)+sizeof(uint64_t))

SGFile *sg_file_open(const char *filename);
int sg_file_close(SGFile *self);
bool sg_file_read_next(SGFile *self, SGContainer *dest);
bool sg_file_get_container(SGFile *self, int type, SGContainer *dest);
bool sg_file_get_payload(SGFile *self, SGContainer *container, uint8_t **payload);


void sg_container_dump(SGContainer *self);
#endif /* SG_CONTAINER_FILE_H */
