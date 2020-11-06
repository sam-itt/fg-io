# Welcome to the fg-io repository

This repository contains code for interfacing with FlightGear to get
flight data such as position(lat, lon), attitude, etc.

There are two ways of doing so in this repo: Network-based and [FGTape](http://wiki.flightgear.org/Fgtape)
records reading.

The network-based code shows a sample FlightGear-side protocol description that
must be given on the command line where FlightGear runs (command line sample is
also provided) and a "client" side UDP client that reads data from said protocol.
This is 100% non-dynamic: Code and xml strictly match each other and if modified
(order, data read,...) both needs to be modified.

# FGTape Reader
The FGTape reader is more elaborate: It can read arbitrary data from a FGTape.
Code should be self-documenting (see sample). In a nutshell, steps are as follows:

## Open the tape
Using fg_tape_new_from_file:
```C
FGTape *tape;
tape = fg_tape_new_from_file("dr400.fgtape");
```
## Lookup signal descriptors
For each value (FlightGear calls them "signals") you want to read you need a
signal descriptor. Declare as many as you need (they might be in an array),
then call fg_tape_get_signal to look them up:
```C
FGTapeSignal signal;
bool success;
success = fg_tape_get_signal(tape, "/position[0]/latitude-deg[0]", &signal);
```
It will return true on success and false otherwise.

## Read values
After you have descriptors for each signal(value) you are interested in, you
can read the tape. There are two ways of reading the tape, both based on time.
Time goes from 0 (beginning of the tape) until tape->duration. Reading past
tape->duration will return the last frame and return false.

### Direct time-based reading
The first way is to fill a memory location with signal(s) value(s) for a given time.
It can be either a single value (double for a "double" signal, etc.) Or an
array/struct etc. This should be done where all the values you need lives in a
contiguous location (or that you only need a single value for that time position).
```C
double latitude;
fg_tape_get_data_at(tape,
    11.0, /*time in seconds*/
    1, /*Number of signals*/
    &signal, /*Pointer to the first signal*/
    &latitude
);
```

### Cursor-based reading
The second way is to get a FGTapeCursor representing a specific position in tape
and then get individual values from that cursor. This should be done when you
need multiple values for that position, each value being stored separately (not
contiguously memory-wise)
```C
FGTapeCursor cursor;
double latitude;

fg_tape_get_cursor(tape, 11.0, &cursor);
fg_tape_cursor_get_signal_value(&cursor, 1, &signal, &latitude);
/*Get other values from the cursor*/
```

## Close the tape
```C
fg_tape_free(tape);
```

## Dependencies

* zlib

## Building & running the sample

The fg-tape-reader.c sample code demonstrate features described above.
It should build on any Unix having zlib (and dev headers) installed.
Build and run:
```sh
$ make && ./test-tape
```

# FlightGear Connector

No documentation yet, code should be pretty self-explainatory. Works using
standard FlightGear feature described [here](http://wiki.flightgear.org/Generic_protocol)

## Building & running the sample

The fg-connector-test.c sample code demonstrate features described above.
It should build on any Unix having zlib (and dev headers) installed.
Build and run:
```sh
$ make && ./test-connector
```
