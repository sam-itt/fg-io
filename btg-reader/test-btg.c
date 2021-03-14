#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "btg-reader.h"

int main(int argc, char *argv[])
{
    BtgReader *reader;
    BtgGeometryIterator iter;
    size_t ngroups;
    uint32_t *buffer = NULL;
    size_t bsize = 0;
    FILE *fp;


    clock_t start, end;

    if(argc < 2){
        printf("Usage: %s filaname.btg[.gz] [output-file.obj]\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    start = clock();

    reader = btg_reader_new(argv[1]);

    fp = stdout;
    if(argc >= 3){
        fp = fopen(argv[2],"w");
    }
    if(!fp){
        printf("Couldn't open output file/stream, bailing out\n");
        exit(EXIT_FAILURE);
    }

    fprintf(fp,"# FGFS Scenery\n");
    fprintf(fp,"# Version 0.4\n");
    fprintf(fp,"\n");

    // write bounding sphere
    fprintf(fp,"# gbs %.5f %.5f %.5f %.2f\n",
        reader->bs.cx, reader->bs.cy, reader->bs.cz,
        reader->bs.radius
    );
    fprintf(fp,"\n");

    fprintf(fp,"# vertex list\n");
    for(int i = 0; i < reader->nvertices; i++){
        fprintf(fp,"v %.5f %.5f %.5f\n",
            reader->vertices[i].x - reader->bs.cx,
            reader->vertices[i].y - reader->bs.cy,
            reader->vertices[i].z - reader->bs.cz
        );
    }
    fprintf(fp,"\n");

    fprintf(fp,"# vertex normal list\n");
    for(int i = 0; i < reader->nnormals; i++){
        fprintf(fp,"vn %.5f %.5f %.5f\n",
            reader->normals[i].x,
            reader->normals[i].y,
            reader->normals[i].z
        );
    }
    fprintf(fp,"\n");

    fprintf(fp,"# texture coordinate list\n");
    for(int i = 0; i < reader->ntexcoords; i++){
        fprintf(fp,"vt %.5f %.5f\n",
            reader->texcoords[i].u,
            reader->texcoords[i].v
        );
    }
    fprintf(fp,"\n");

    fprintf(fp,"# triangle groups\n");
    fprintf(fp,"\n");
    for(int i = 0; i < reader->ntriangles; i++){
        btg_reader_init_geometry_iterator(reader, &reader->triangles[i], &iter);
        if(bsize < iter.max_buffer_size){
            buffer = realloc(buffer, iter.max_buffer_size);
            bsize = iter.max_buffer_size;
        }
        int8_t vidx, tidx;
        btg_geometry_object_get_va_indices(&reader->triangles[i], &vidx, NULL, NULL, &tidx, NULL, NULL, NULL);
        fprintf(fp,"# usemtl %s\n",reader->triangles[i].material);
        fprintf(fp,"# bs %.5f %.5f %.5f %.2f\n",0.0,0.0,0.0,0.0);
        for(int j = 0; j < reader->triangles[i].nelements; j++){
            bool valid = btg_geometry_iterator_read(&iter, buffer, &ngroups);
            if(!valid) continue;

            uint32_t pos[3], tex[3];
            for (int k = 2; k < ngroups; k += 3) {
                pos[2] = buffer[k*iter.stride + vidx];
                tex[2] = buffer[k*iter.stride + tidx];

                pos[1] = buffer[(k-1)*iter.stride + vidx];
                tex[1] = buffer[(k-1)*iter.stride + tidx];

                pos[0] = buffer[(k-2)*iter.stride + vidx];
                tex[0] = buffer[(k-2)*iter.stride + tidx];

                fprintf(fp, "f %d/%d %d/%d %d/%d\n",
                    pos[0]+1, tex[0]+1,
                    pos[1]+1, tex[1]+1,
                    pos[2]+1, tex[2]+1
                );
            }
        }
        fprintf(fp, "\n");
    }
    btg_reader_free(reader);
    end = clock();
    fprintf(stderr, "Total duration: %f\n",(double)(end - start) / CLOCKS_PER_SEC);
	exit(EXIT_SUCCESS);
}


