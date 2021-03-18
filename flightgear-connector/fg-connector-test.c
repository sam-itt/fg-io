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
    printf("2. Run FlightGear(fgfs) with --generic=socket,out,5,LOCAL_IP,6789,udp,basic_proto\n");
    printf("REPLACE LOCAL_IP WITH THE IP OF THE LOCAL MACHINE\n");
    do{
        if(flightgear_connector_get_packet(fglink, &packet)){
            printf("Got packet:\n"
                "lat,lon,alt: %f, %f, %f\n"
                "\troll,pitch,heading: %f,%f,%f\n"
                "\tslip: %f\n"
                "\tAirspeed: %f\n"
                "\tVertial speed: %f\n"
                "\trpm: %f\n"
                "\tfuel flow: %f\n"
                "\tCHT:%f\n"
                "\tOil temp: %f px:%f\n"
                "\tFuel px: %f\n"
                "\tFuel qty: %f\n",
                packet.latitude, packet.longitude, packet.altitude,
                packet.roll,packet.pitch, packet.heading,
                packet.side_slip,
                packet.airspeed,
                packet.vertical_speed,
                packet.rpm,
                packet.fuel_flow,
                packet.cht,
                packet.oil_temp, packet.oil_px,
                packet.fuel_px,
                packet.fuel_qty
            );
        }
    }while(1);

	exit(EXIT_SUCCESS);
}

