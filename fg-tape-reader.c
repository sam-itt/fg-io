#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <inttypes.h>

#include "fg-tape.h"

int main(int argc, char *argv[])
{
    FGTape *tape;
    FGTapeSignal latitude, longitude;
    FGTapeSignal roll, pitch, heading;
    FGTapeSignal engine0, engine1;
    FGTapeRecord *rec;
    bool rv;

    tape = fg_tape_new_from_file("dr400.fgtape");

    rv = fg_tape_get_signal(tape, "/position[0]/longitude-deg[0]", &longitude);
    if(!rv){
        printf("Couldn't get signal %s\n","/position[0]/longitude-deg[0]");
    }

    rv = fg_tape_get_signal(tape, "/position[0]/latitude-deg[0]", &latitude);
    if(!rv){
        printf("Couldn't get signal %s\n","/position[0]/latitude-deg[0]");
    }

    rv = fg_tape_get_signal(tape, "/orientation[0]/roll-deg[0]", &roll);
    if(!rv){
        printf("Couldn't get signal %s\n","/orientation[0]/roll-deg[0]");
    }

    rv = fg_tape_get_signal(tape, "/engines[0]/engine[0]/running[0]", &engine0);
    if(!rv){
        printf("Couldn't get signal %s\n","/engines[0]/engine[0]/running[0]");
    }
    rv = fg_tape_get_signal(tape, "/engines[0]/engine[1]/running[0]", &engine1);
    if(!rv){
        printf("Couldn't get signal %s\n","/engines[0]/engine[1]/running[0]");
    }

    printf("Longitude: %d - %d\n",longitude.idx, longitude.type);
    printf("Latitude: %d - %d\n",latitude.idx, latitude.type);
    printf("Roll: %d - %d\n",roll.idx, roll.type);
    printf("Engine0: %d - %d\n",engine0.idx, engine0.type);
    printf("Engine1: %d - %d\n",engine1.idx, engine1.type);

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
        rv2 = fg_tape_get_keyframes_for(tape, i, &first, &second);
        printf("Found time=%f to be between records t=%f and t=%f\n",
            i,
            first->sim_time,
            second ? second->sim_time : -1
        );
    }
#endif
	exit(EXIT_SUCCESS);
}

