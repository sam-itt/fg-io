#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <inttypes.h>

#include "fg-tape.h"

static char *pos_signals[] = {
    "/position[0]/latitude-deg[0]",
    "/position[0]/longitude-deg[0]",
    "/position[0]/altitude-ft[0]",
    "/position[0]/altitude-agl-ft[0]",
    NULL
};

static char *att_signals[] = {
    "/orientation[0]/roll-deg[0]",
    "/orientation[0]/pitch-deg[0]",
    "/orientation[0]/heading-deg[0]",
    NULL
};

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
    FGTapeSignal location[4];
    FGTapeSignal attitude[3];
    FGTapeSignal engine_running[2];
    FGTapeRecord *rec;
    TmpBuffer buff;
    AttBuffer buf2;
    EngineRunningBuffer ebuff;
    bool rv;


    tape = fg_tape_new_from_file("dr400.fgtape");


    for(int i = 0; pos_signals[i] != NULL; i++){
        rv = fg_tape_get_signal(tape, pos_signals[i], &location[i]);
        if(!rv){
            printf("Couldn't get signal %s\n",pos_signals[i]);
        }
    }

    for(int i = 0; att_signals[i] != NULL; i++){
        rv = fg_tape_get_signal(tape, att_signals[i], &attitude[i]);
        if(!rv){
            printf("Couldn't get signal %s\n",att_signals[i]);
        }
    }

    rv = fg_tape_get_signal(tape, "/engines[0]/engine[0]/running[0]", &engine_running[0]);
    if(!rv){
        printf("Couldn't get signal %s\n","/engines[0]/engine[0]/running[0]");
    }
    rv = fg_tape_get_signal(tape, "/engines[0]/engine[1]/running[0]", &engine_running[1]);
    if(!rv){
        printf("Couldn't get signal %s\n","/engines[0]/engine[1]/running[0]");
    }

    printf("Longitude: %d - %d\n",location[1].idx, location[1].type);
    printf("Latitude: %d - %d\n",location[0].idx, location[0].type);
    printf("Roll: %d - %d\n",attitude[0].idx, attitude[0].type);
    printf("Engine0: %d - %d\n",engine_running[0].idx, engine_running[0].type);
    printf("Engine1: %d - %d\n",engine_running[1].idx, engine_running[1].type);

    fg_tape_dump(tape);
#if 0
    for(int i = 0; i < tape->record_count; i++){
        rec = (FGTapeRecord*)(tape->data + tape->record_size*i);
        printf("%x: sim_time: %f, latitude: %f, longitude: %f, engines 0: %s, 1: %s\n",
            0xA423+i,
            rec->sim_time,
            fg_tape_get_value(tape, rec, double, &latitude),
            fg_tape_get_value(tape, rec, double, &longitude),
            fg_tape_get_value(tape, rec, bool, &engine0) ? "on" : "off",
            fg_tape_get_value(tape, rec, bool, &engine1) ? "on" : "off"
        );
    }
#endif
#if 1
    bool rv2;
    FGTapeRecord *first, *second;
    for(double i = 0; i < tape->duration; i += 10){
#if 0
        rv2 = fg_tape_get_keyframes_for(tape, i, &first, &second);
        printf("Found time=%f to be between records t=%f and t=%f\n",
            i,
            first->sim_time,
            second ? second->sim_time : -1
        );
#endif
        fg_tape_get_data_at(tape, i, location, 4, &buff);
        fg_tape_get_data_at(tape, i, attitude, 3, &buf2);
        fg_tape_get_data_at(tape, i, engine_running, 2, &ebuff);
        printf("For time %f, lat,lon,alt - roll,pitch,heading is: %f,%f,%f - %f,%f,%f. Engine0: %s Engine1: %s\n",i,
            buff.latitude,buff.longitude, buff.altitude,
            buf2.roll, buf2.pitch, buf2.heading,
            ebuff.engine0 ? "on" : "off",
            ebuff.engine1 ? "on" : "off"
        );
//        exit(0);
    }
#endif
	exit(EXIT_SUCCESS);
}

