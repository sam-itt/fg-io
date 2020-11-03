#ifndef FG_TAPE_H
#define FG_TAPE_H

#include "sg-file.h"

#define IPOL_LINEAR 0
#define IPOL_ANGULAR_DEG 1
#define IPOL_ANGULAR_RAD 2
#define NIPOLS 3

#define KDOUBLE 0
#define KFLOAT 1
#define KINT 2
#define KINT16 3
#define KINT8 4
#define KBOOL 5
#define NKINDS 6

typedef struct{
    char **names;
    uint8_t *ipol_types;

    /* Numbert of actually stored names/ipol_types
     * NOT availalbe/allocated space
     */
    size_t count;
}FGTapeSignalKind;

typedef struct{
    float duration;
    size_t record_size;
    size_t record_count;

    FGTapeSignalKind signals[NKINDS];
    size_t signal_count;

    uint8_t *data;
}FGTape;

typedef struct{
    size_t idx;
    uint8_t type;
}FGTapeSignal;

typedef struct{
    double sim_time;

    uint8_t data[];
}FGTapeRecord;

#define fg_tape_get_value(self, record, __type, signal) (*(__type *)fg_tape_get_value_ptr(self, record, (signal)->type, (signal)->idx))

FGTape *fg_tape_new_from_file(const char *filename);
void fg_tape_free(FGTape *self);

bool fg_tape_get_signal(FGTape *self, const char *name, FGTapeSignal *signal);
void *fg_tape_get_value_ptr(FGTape *self, FGTapeRecord *record, uint8_t kind, size_t idx);

void fg_tape_dump(FGTape *self);
#endif /* FG_TAPE_H */
