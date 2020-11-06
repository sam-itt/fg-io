/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2020 Samuel Cuella */

#include <stdio.h>
#include <stdlib.h>

#include "flightgear-connector.h"


int main(int argc, char *argv[])
{
    FlightgearConnector *fglink;
    FlightgearPacket packet;

    fglink = flightgear_connector_new(6789);
    //flightgear_connector_set_nonblocking(fglink);

    printf("Waiting for first packet from FlightGear\n");
    printf("Be sure to:\n");
    printf("1. have basic_proto.xml in $FG_ROOT/Protocol\n");
    printf("2. Run FlightGear(fgfs) with --generic=socket,out,5,LOCAL_IP,6798,udp,basic_proto\n");
    printf("REPLACE LOCAL_IP WITH THE IP OF THE LOCAL MACHINE\n");
    do{
        if(flightgear_connector_get_packet(fglink, &packet)){
            printf("Got packet:\n"
                    "\tAltitude: %d\n"
                    "\tAirspeed: %d\n"
                    "\tVertial speed: %f\n"
                    "\troll,pitch,heading: %f,%f,%f\n",
                    packet.altitude, packet.airspeed,
                    packet.vertical_speed,
                    packet.roll,packet.pitch, packet.heading
            );
        }
    }while(1);

	exit(EXIT_SUCCESS);
}

