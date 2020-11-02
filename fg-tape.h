#ifndef FG_TAPE_H
#define FG_TAPE_H

#include "sg-file.h"

#define IPOL_LINEAR 0
#define IPOL_ANGULAR_DEG 1
#define IPOL_ANGULAR_RAD 2

#define TDOUBLE 0
#define TFLOAT 1
#define TINT 2
#define TINT16 3
#define TINT8 4
#define TBOOL 5

typedef struct{
    char **names;
    uint8_t *ipol_types;

    /* Numbert of actually stored names/ipol_types
     * NOT availalbe/allocated space
     */
    size_t count;
}FGTapeSignalKind;

typedef struct{
    FGTapeSignalKind doubles;
    FGTapeSignalKind floats;
    FGTapeSignalKind ints;
    FGTapeSignalKind int16s;
    FGTapeSignalKind int8s;
    FGTapeSignalKind bools;
    size_t total_count;
}FGTapeSignals;

typedef struct{
    SGFile *file;

    float duration;
    size_t record_size;
//    size_t record_count;

    FGTapeSignals signals;

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

FGTape *fg_tape_open(const char *filename);


bool fg_tape_get_signal(FGTape *self, const char *name, FGTapeSignal *signal);


double fg_tape_get_record_signal_double_value(FGTape *self, FGTapeRecord *record, FGTapeSignal *signal);
float fg_tape_get_record_signal_float_value(FGTape *self, FGTapeRecord *record, FGTapeSignal *signal);
#endif /* FG_TAPE_H */
