/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2020 Samuel Cuella */

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
    IpolType interpolation;
    size_t offset;
}FGTapeSignal;

typedef struct{
    double sim_time;
    uint8_t data[];
}FGTapeRecord;

typedef struct{
    double ratio;
    /*keyframes for time*/
    FGTapeRecord *k1;
    FGTapeRecord *k2;
}FGTapeCursor;


FGTape *fg_tape_new_from_file(const char *filename);
void fg_tape_free(FGTape *self);

bool fg_tape_get_signal(FGTape *self, const char *name, FGTapeSignal *signal);
int fg_tape_get_signals(FGTape *self, FGTapeSignal *signals, ...);

bool fg_tape_get_keyframes_for(FGTape *self, double time, FGTapeRecord **k1, FGTapeRecord **k2);

int fg_tape_get_cursor(FGTape *self, double time, FGTapeCursor *cursor);
bool fg_tape_cursor_get_signal_value(FGTapeCursor *self, size_t nsignals, FGTapeSignal *signals, void *buffer);
int fg_tape_get_data_at(FGTape *self, double time, size_t nsignals, FGTapeSignal *signals, void *buffer);

void fg_tape_dump(FGTape *self);
#endif /* FG_TAPE_H */
