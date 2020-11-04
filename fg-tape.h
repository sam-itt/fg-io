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

#define RC_INVALID -1
#define RC_HEADER 0
#define RC_METADATA 1
#define RC_PROPERTIES 2
#define RC_RAWDATA 3

#define SHORT_TERM 0
#define MID_TERM 1
#define LONG_TERM 2
#define NTERMS 3

typedef struct{
    char **names;
    uint8_t *ipol_types;

    /* Numbert of actually stored names/ipol_types
     * NOT availalbe/allocated space
     */
    size_t count;
}FGTapeSignalKind;

typedef struct{
    uint8_t *data;
    size_t record_count;
}FGTapeRecordSet;

typedef struct{
    float duration;

    double first_stamp;
    double sec2sim;

    size_t record_size;

    FGTapeSignalKind signals[NKINDS];
    size_t signal_count;

    FGTapeRecordSet records[NTERMS];
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
#define fg_tape_get_record(self, term, idx) ((FGTapeRecord*)((self)->records[(term)].data + (self)->record_size*(idx)))
#define fg_tape_first(self, term) fg_tape_get_record(self, term, 0)
#define fg_tape_last(self, term) fg_tape_get_record(self, term, (self)->records[(term)].record_count-1)
#define fg_tape_term_get_record(self, rset, idx) ((FGTapeRecord*)((rset)->data + (self)->record_size*(idx)))

FGTape *fg_tape_new_from_file(const char *filename);
void fg_tape_free(FGTape *self);

bool fg_tape_get_signal(FGTape *self, const char *name, FGTapeSignal *signal);
void *fg_tape_get_value_ptr(FGTape *self, FGTapeRecord *record, uint8_t kind, size_t idx);

//void fg_tape_get_for(FGTape *self, double seconds);
bool fg_tape_get_keyframes_for(FGTape *self, double time, FGTapeRecord **k1, FGTapeRecord **k2);

void fg_tape_dump(FGTape *self);
#endif /* FG_TAPE_H */
