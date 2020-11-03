#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <inttypes.h>

#include "sg-file.h"
#include "fg-tape.h"

typedef struct  __attribute__((__packed__)){
    double sim_time;
    double doubles[4];
    float floats[132];
    int ints[0];
    short int int16s[0];
    signed char int8s[0];
    unsigned char bools[(52+7)/8];
}SimRecord;

int main(int argc, char *argv[])
{

    size_t total_record_size;
    SGFile *fp;
    SimRecord record;



#if 0
    total_record_size = sizeof(double)        * 1 /* sim time */        +
                        sizeof(double)        * m_CaptureDouble.size()  +
                        sizeof(float)         * m_CaptureFloat.size()   +
                        sizeof(int)           * m_CaptureInteger.size() +
                        sizeof(short int)     * m_CaptureInt16.size()   +
                        sizeof(signed char)   * m_CaptureInt8.size()    +
                        sizeof(unsigned char) * ((m_CaptureBool.size()+7)/8); // 8 bools per byte
#endif
    total_record_size = sizeof(double)        * 1 /* sim time */        +
                        sizeof(double)        * 4                       +
                        sizeof(float)         * 132                     +
                        sizeof(int)           * 0                       +
                        sizeof(short int)     * 0                       +
                        sizeof(signed char)   * 0                       +
                        sizeof(unsigned char) * ((52+7)/8); // 8 bools per byte

    printf("sizeof(SimRecord):%d, total_record_size:%d\n",sizeof(SimRecord),total_record_size);
#if 1
    FGTape *tape;
    FGTapeSignal latitude, longitude;
    FGTapeSignal roll, pitch, heading;
    FGTapeSignal engine0, engine1;
    FGTapeRecord *rec;
    bool rv;

    tape = fg_tape_new_from_file("dr400.fgtape");
//    tape = fg_tape_open("dr400.fgtape");

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

    printf("record_size: %d\n",tape->record_size);
    //exit(0);
    for(int i = 0; i < 1314; i++){
        rec = (FGTapeRecord*)(tape->data + tape->record_size*i);
#if 0
        printf("Record %d:\n"
                "\tsim_time: %f\n"
                "\tlatitude: %f\n"
                "\tlongitude: %f\n"
                "\troll: %f\n",
                i,
                rec->sim_time,
                fg_tape_get_record_signal_double_value(tape, rec, &latitude),
                fg_tape_get_record_signal_double_value(tape, rec, &longitude),
                fg_tape_get_record_signal_float_value(tape, rec, &roll)
        );
#else
        printf("%x: sim_time: %f, latitude: %f, longitude: %f, engines 0: %s, 1: %s\n",
            0xA423+i,
            rec->sim_time,
            fg_tape_get_value(tape, rec, double, &latitude),
            fg_tape_get_value(tape, rec, double, &longitude),
            fg_tape_get_value(tape, rec, bool, &engine0) ? "on" : "off",
            fg_tape_get_value(tape, rec, bool, &engine1) ? "on" : "off"
        );
#endif
    }


    exit(0);
#endif
    fp = sg_file_open("dr400.fgtape");
    if(!fp){
        printf("Couldn't open %s, bailing out\n","dr400.fgtape");
        exit(EXIT_FAILURE);
    }

    SGContainer containers[4];

    sg_file_read_next(fp, &containers[0]);
    sg_container_dump(&containers[0]);

    sg_file_read_next(fp, &containers[1]);
    sg_container_dump(&containers[1]);

    unsigned char *data = NULL;
    sg_file_get_payload(fp, &containers[1], &data);
    printf("%s", data);

    sg_file_read_next(fp, &containers[2]);
    sg_container_dump(&containers[2]);
    free(data);
    data = NULL;
    sg_file_get_payload(fp, &containers[2], &data);
    printf("%s", data);

    sg_file_read_next(fp, &containers[3]);
    sg_container_dump(&containers[3]);
    free(data);
    data = NULL;
    sg_file_get_payload(fp, &containers[3], &data);

    int nrecords = containers[3].size/sizeof(SimRecord);
    printf("Container should have %d records\n",nrecords);

    for(int i = 0; i < nrecords; i++){
        memcpy(&record, data+sizeof(SimRecord)*i,sizeof(SimRecord));
        printf("%x: sim_time: %f, latitude: %f, longitude: %f\n",0xA423+i ,record.sim_time,record.doubles[1],record.doubles[0]);
    }

    sg_file_close(fp);
	exit(EXIT_SUCCESS);
}

