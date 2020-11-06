/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2020 Samuel Cuella */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <inttypes.h>

#include "fg-tape.h"

typedef struct{
    double latitude;
    double longitude;
    double altitude;
    double altitude_agl;
}TmpBuffer;

typedef struct{
    float roll;
    float pitch;
    float heading;
}AttBuffer;

typedef struct{
    bool engine0;
    bool engine1;
}EngineRunningBuffer;

int main(int argc, char *argv[])
{
    FGTape *tape;
    FGTapeSignal location_signals[4];
    FGTapeSignal attitude_signals[3];
    FGTapeSignal engine_signals[2];
    FGTapeRecord *rec;
    TmpBuffer buff;
    AttBuffer buf2;
    EngineRunningBuffer ebuff;
    int found;


    tape = fg_tape_new_from_file("dr400.fgtape");

    found = fg_tape_get_signals(tape, location_signals,
        "/position[0]/latitude-deg[0]",
        "/position[0]/longitude-deg[0]",
        "/position[0]/altitude-ft[0]",
        "/position[0]/altitude-agl-ft[0]",
        NULL
    );
    printf("Location: found %d out of %d signals\n",found,4);

    found = fg_tape_get_signals(tape, attitude_signals,
        "/orientation[0]/roll-deg[0]",
        "/orientation[0]/pitch-deg[0]",
        "/orientation[0]/heading-deg[0]",
        NULL
    );
    printf("Location: found %d out of %d signals\n",found,3);

    found = fg_tape_get_signals(tape, engine_signals,
        "/engines[0]/engine[0]/running[0]",
        "/engines[0]/engine[1]/running[0]",
        NULL
    );
    printf("Location: found %d out of %d signals\n",found,2);


    printf("Longitude: %d - %d\n",location_signals[1].idx, location_signals[1].type);
    printf("Latitude: %d - %d\n",location_signals[0].idx, location_signals[0].type);
    printf("Roll: %d - %d\n",attitude_signals[0].idx, attitude_signals[0].type);
    printf("Engine0: %d - %d\n",engine_signals[0].idx, engine_signals[0].type);
    printf("Engine1: %d - %d\n",engine_signals[1].idx, engine_signals[1].type);

    fg_tape_dump(tape);

    FGTapeRecord *first, *second;
    for(double i = 0; i < tape->duration; i += 10){
        fg_tape_get_data_at(tape, i, 4, location_signals, &buff);
        fg_tape_get_data_at(tape, i, 3, attitude_signals, &buf2);
        fg_tape_get_data_at(tape, i, 2, engine_signals, &ebuff);
        printf("For time %f, lat,lon,alt - roll,pitch,heading is: %f,%f,%f - %f,%f,%f. Engine0: %s Engine1: %s\n",i,
            buff.latitude,buff.longitude, buff.altitude,
            buf2.roll, buf2.pitch, buf2.heading,
            ebuff.engine0 ? "on" : "off",
            ebuff.engine1 ? "on" : "off"
        );
    }
    fg_tape_free(tape);
	exit(EXIT_SUCCESS);
}

