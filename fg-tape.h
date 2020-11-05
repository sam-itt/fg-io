#ifndef FG_TAPE_H
#define FG_TAPE_H

#include "sg-file.h"

typedef enum{
    IPDiscrete,
    IPLinear,
    IPAngularRad,
    IPAngularDeg,
    NIpols
}IpolType;

typedef enum{
    TDouble,
    TFloat,
    TInt,
    TInt16,
    TIn8,
    TBool,
    NTypes
}SignalType;

#define RC_INVALID -1
#define RC_HEADER 0
#define RC_METADATA 1
#define RC_PROPERTIES 2
#define RC_RAWDATA 3

#define SHORT_TERM 0
#define MEDIUM_TERM 1
#define LONG_TERM 2
#define NTERMS 3

typedef struct{
    char **names;
    uint8_t *ipol_types;

    /* Numbert of actually stored names/ipol_types
     * NOT availalbe/allocated space
     */
    size_t count;
}FGTapeSignalSet;

typedef struct{
    uint8_t *data;
    size_t record_count;
}FGTapeRecordSet;

typedef struct{
    float duration;

    double first_stamp;
    double sec2sim;

    size_t record_size;

    /*All the signals that can be found in this
     * tape's records, arranged in sets as per their type
     * (i.e all double signals, all float signals, ...)*/
    FGTapeSignalSet signals[NTypes];
    size_t signal_count;

    FGTapeRecordSet records[NTERMS];
}FGTape;

typedef struct{
    size_t idx;
    uint8_t type;
    size_t offset;
}FGTapeSignal;

typedef struct{
    double sim_time;
    uint8_t data[];
}FGTapeRecord;

#define fg_tape_record_get_signal_value(self, __type, signal) (*(__type *)fg_tape_record_get_signal_value_ptr((self), (signal))
#define fg_tape_get_record(self, term, idx) ((FGTapeRecord*)((self)->records[(term)].data + (self)->record_size*(idx)))
#define fg_tape_first(self, term) fg_tape_get_record(self, term, 0)
#define fg_tape_last(self, term) fg_tape_get_record(self, term, (self)->records[(term)].record_count-1)
#define fg_tape_term_get_record(self, rset, idx) ((FGTapeRecord*)((rset)->data + (self)->record_size*(idx)))

FGTape *fg_tape_new_from_file(const char *filename);
void fg_tape_free(FGTape *self);

bool fg_tape_get_signal(FGTape *self, const char *name, FGTapeSignal *signal);

//void fg_tape_get_for(FGTape *self, double seconds);
bool fg_tape_get_keyframes_for(FGTape *self, double time, FGTapeRecord **k1, FGTapeRecord **k2);

bool fg_tape_get_data_at(FGTape *self, double time, FGTapeSignal *signals, size_t nsignals, void *buffer);

void fg_tape_dump(FGTape *self);
#endif /* FG_TAPE_H */
