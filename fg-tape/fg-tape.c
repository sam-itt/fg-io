/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2020 Samuel Cuella */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "fg-tape.h"
#include "sg-file.h"

#define fg_tape_get_record(self, term, idx) ((FGTapeRecord*)((self)->records[(term)].data + (self)->record_size*(idx)))
#define fg_tape_first(self, term) fg_tape_get_record(self, term, 0)
#define fg_tape_last(self, term) fg_tape_get_record(self, term, (self)->records[(term)].record_count-1)

static char *pretty_types[NTypes] = {"double","float","int","int16","int8","bool"};
static size_t types_sizes[NTypes] = {
    sizeof(double), sizeof(float), sizeof(int),
    sizeof(short int), sizeof(signed char), sizeof(unsigned char)
};

static char *pretty_ipols[NIpols] = {"discrete", "linear", "angular-rad", "angular-deg"};

static char *pretty_terms[NTERMS] = {"short","mid","long"};

typedef struct{
    char *str;
    size_t len;
}XString;

bool _get_node_value(const char *xml, const char *node_name, XString *str);

bool fg_tape_read_duration(FGTape *self, SGFile *file)
{
    SGContainer container;
    char *xml;
    XString cursor;
    char *end;
    bool ret;

    ret = sg_file_get_container(file, 1, &container);
    if(!ret){
        printf("Couldn't find meta container, bailing out\n");
        return NULL;
    }

    xml = NULL;
    sg_file_get_payload(file, &container, (uint8_t**)&xml);

    _get_node_value(xml, "tape-duration", &cursor);
    if(!cursor.str){
        printf("Couldn't find tape-duration's value in file xml descriptor, bailing out\n");
        return false;
    }
    self->duration = atof(cursor.str);
    free(xml);
    return true;
}

/**
 * Searchs for <nodename in xml and return the address of '<'
 *
 * returns NULL when not found
 */
char *strnode(const char *xml, const char *node_name)
{
    int len;
    const char *cursor;

    len = strlen(node_name);
    cursor = xml;
    while((cursor = strchr(cursor, '<'))){
        if(!strncasecmp(cursor+1, node_name, len)){
            return (char*)cursor;
        }
        cursor++;
    }
    return NULL;
}

/* *
 * Hepler function to read XML.
 *
 * Look for the value starting as in <node foo="bar">VALUE</node>
 * and returns a pointer to where it starts (V) and the length
 * (5)
 *
 * Starts reading from the given pointer which can anywhay within
 * the actual content
 *
 */
bool _get_node_value(const char *xml, const char *node_name, XString *str)
{
    char *tmp;
    char *cursor;

    cursor = strnode(xml, node_name);
    if(!cursor){
        printf("Couldn't find <%s> tag in given xml string\n", node_name);
        return false;
    }
    cursor = strchr(cursor, '>');
    if(!cursor){
        printf("Couldn't find <%s's GT\n", node_name);
        return false;
    }

    //<type bar="foo">data</type> whe are at '>'
    //count distance to closing </type> to read what's in between
    tmp = strchr(cursor, '<');
    if(!tmp){
        printf("Couldn't find </%s> closing tag\n",node_name);
        return false;
    }
    str->len = (tmp-1) - cursor;
    str->str = cursor+1;

    return true;
}

bool fg_tape_read_signal(FGTape *self, char **cursor)
{
    char *begin, *end;
    char *tmp;

    begin = strstr(*cursor, "<signal");
    if(!begin){
        printf("Couldn't find next signal in signal list, bailing out\n");
        return false;
    }
    end = strstr(begin, "</signal>");
    if(!end){
        printf("Couldn't find matching closing </signal> tag for current signal, bailing out\n");
        return false;
    }

    XString tmptype;
    _get_node_value(begin, "type", &tmptype);
    //printf("Read signal type: %.*s\n", tlen, tmptype);

    XString prop;
    _get_node_value(begin, "property", &prop);
    if(!prop.str){
        printf("Couldn't find property name for current %.*s signal, bailing out\n", tmptype.len, tmptype.str);
        return false;
    }

    XString ipol_type;
    _get_node_value(begin, "interpolation", &ipol_type);
    if(!ipol_type.str){
        printf("Couldn't find interpolation for current  %.*s signal, bailing out\n", tmptype.len, tmptype.str);
        return false;
    }

    int tmpipol = -1;
    for(IpolType i = IPDiscrete; i < NIpols; i++){
        if(!strncmp(ipol_type.str, pretty_ipols[i], ipol_type.len)){
            tmpipol = i;
            break;
        }
    }
    if( tmpipol < 0){
        printf("Unknown interpolation type: %.*s, bailing out\n", ipol_type.len, ipol_type.str);
        return false;
    }

    FGTapeSignalSet *set = NULL;
    for(SignalType i = TDouble; i < NTypes; i++){
        if(!strncmp(tmptype.str, pretty_types[i], tmptype.len)){
            set = &self->signals[i];
        }
    }
    if(!set){
        printf("Couldn't find a matching signal type for read value %.*s",tmptype.len, tmptype.str);
        return false;
    }

    set->names[set->count] =  strndup(prop.str, prop.len); //TODO: free me
    set->ipol_types[set->count] = tmpipol;
    set->count++;

    *cursor = end;
    return true;
}

/* *
 * Use data from the xml to allocate enough space to hold
 * signals data of a given type (double, float, ...)
 *
 * self->count is set to 0 but names and ipol_types are allocated
 * to fit as many signals as declared in the file <type></type>
 * node
 *
 * returns: Number of allocated signals or -1 on failure
 */
int fg_tape_signal_set_init(FGTapeSignalSet *self, const char *type, const char *xml)
{
    int n_signals;
    XString cursor;

    _get_node_value(xml, type, &cursor);
    if(!cursor.str){
        printf("Couldn't find count of '%s' signals, bailing out\n", type);
        return -1;
    }
    n_signals = atoi(cursor.str);
    self->names = calloc(sizeof(char*), n_signals);
    self->ipol_types = calloc(sizeof(uint8_t), n_signals);

    return n_signals;
}

void fg_tape_signal_set_dispose(FGTapeSignalSet *self)
{
    for(int i = 0; i < self->count; i++){
        free(self->names[i]);
    }
    free(self->names);
    free(self->ipol_types);
}

void fg_tape_signal_set_dump(FGTapeSignalSet *self, uint8_t level)
{
    int i;

    for(i = 0; i < self->count; i++){
        for(int j = 0; j < level; j++) putchar('\t');
        printf("%s - %s\n",
            self->names[i],
            pretty_ipols[self->ipol_types[i]]
        );
    }
}

void fg_tape_dump(FGTape *self)
{

    printf("FGTape(%p):\n",self);
    printf("\tDuration: %f\n",self->duration);
    printf("\tRecord size(bytes): %d\n",self->record_size);
    printf("\tNumber of records:\n");
    for(int i = 0; i < NTERMS; i++){
        printf("\t%s term: %d\n",pretty_terms[i], self->records[i].record_count);
    }

    printf("\tEach record has:\n");
    for(SignalType i = TDouble; i < NTypes; i++){
        printf("\t\t%d %s\n",
            self->signals[i].count,
            pretty_types[i]
        );
    }

    for(SignalType i = TDouble; i < NTypes; i++){
        if(self->signals[i].count > 0){
            printf("\t%s signals:\n", pretty_types[i]);
            fg_tape_signal_set_dump(&(self->signals[i]), 2);
        }else{
            printf("\t%s signals: None\n", pretty_types[i]);
        }
    }

    for(int i = 0; i < NTERMS; i++){
        printf("\tTerm %s goes from %f to %f sim_time (dt: %f) with %d records\n",
            pretty_terms[i],
            fg_tape_first(self, i)->sim_time,
            fg_tape_last(self, i)->sim_time,
            fg_tape_last(self, i)->sim_time -  fg_tape_first(self, i)->sim_time,
            self->records[i].record_count
        );
    }
}

bool fg_tape_read_signals(FGTape *self, SGFile *file)
{
    SGContainer container;
    char *xml;
    char *cursor;
    char *end;
    bool ret;

    ret = sg_file_get_container(file, 2, &container);
    if(!ret){
        printf("Couldn't find XML descriptor container, bailaing out\n");
        return NULL;
    }

    xml = NULL;
    sg_file_get_payload(file, &container, (uint8_t**)&xml);
    end = xml + container.size;

    int count[NTypes];
    for(SignalType i = TDouble; i < NTypes; i++){
        count[i] = fg_tape_signal_set_init(
            &(self->signals[i]), /*Might as well use 'i' direcly?*/
            pretty_types[i],
            xml
        );
        self->signal_count += count[i] < 0 ? 0 : count[i];
    }

    self->record_size = sizeof(double)        * 1 /* sim time */        +
                        sizeof(double)        * count[0]                +
                        sizeof(float)         * count[1]                +
                        sizeof(int)           * count[2]                +
                        sizeof(short int)     * count[3]                +
                        sizeof(signed char)   * count[4]                +
                        sizeof(unsigned char) * ((count[5]+7)/8); // 8 bools per byte


    cursor = strstr(xml, "<signals>");
    if(!cursor){
        printf("Couldn't find signal list in file xml descriptor, bailing out\n");
        return false;
    }

    for(int i = 0; i < self->signal_count; i++)
        fg_tape_read_signal(self, &cursor);
    free(xml);
    return true;
}

bool fg_tape_read_records(FGTape *self, FGTapeRecordSet *set, SGFile *file)
{
    SGContainer container;
    bool rv;

    rv = sg_file_read_next(file, &container);
    if(!rv){
        printf("Couldn't get next container\n");
        return false;
    }

    if(container.type != RC_RAWDATA){
        printf("Expecting RC_RAWDATA container(%d), got %d\n", RC_RAWDATA, container.type);
    }

    rv = sg_file_get_payload(file, &container, &(set->data));
    if(!rv){
        printf("Couldn't get payload, bailing out\n");
        return false;
    }
    set->record_count = container.size/self->record_size;
    //printf("Container should have %d records\n", set->record_count);

    return true;
}

FGTape *fg_tape_init_from_file(FGTape *self, const char *filename)
{
    SGFile *file;
    SGContainer container;
    bool ret;
    FGTape *rv = NULL;

    file = sg_file_open(filename);
    if(!file){
        return NULL;
    }

    sg_file_get_container(file, 0, &container); //Skip first "header" type container

    fg_tape_read_duration(self, file);
    fg_tape_read_signals(self, file);

    /*Raw data*/
    for(int i = 0; i < NTERMS; i++){
        if(!fg_tape_read_records(self, &self->records[i], file))
            break;
    }

    rv = self;
out:
    sg_file_close(file);
    return rv;
}

FGTape *fg_tape_new_from_file(const char *filename)
{
    FGTape *rv;

    rv = calloc(1,sizeof(FGTape));
    if(!rv)
        return NULL;

    if(!fg_tape_init_from_file(rv, filename)){
        fg_tape_free(rv);
        return NULL;
    }
    return rv;
}

void fg_tape_free(FGTape *self)
{
    for(SignalType i = TDouble; i < NTypes; i++)
        fg_tape_signal_set_dispose(&self->signals[i]);

    for(int i = 0; i < NTERMS; i++){
        if(self->records[i].data)
            free(self->records[i].data);
    }
    free(self);
}

/**
 * Takes a NULL-terminated list of signal names and fill signals with it.
 * Signals must be a pointer to a location large enough to hold as many
 * as passed-in signals. Stops on the first not-found signal
 *
 * Returns: Number of read signals
 */
int fg_tape_get_signals(FGTape *self, FGTapeSignal *signals, ...)
{
    int rv = 0;
    va_list ap;
    char *signal_name;
    bool found;

    va_start(ap, signals);
    while((signal_name = va_arg(ap, char*))){
        found = fg_tape_get_signal(self, signal_name, &signals[rv]);
        if(!found){
            printf("Couldn't get signal %s\n", signal_name);
            break;
        }
        rv++;
    }
    va_end(ap);

    return rv;
}

/**
 * Searchs for a FGTapeSignal signal descriptor(that can be used on a record
 * to get its value) matching "name"
 *
 * Returns: true if the signal is found, false otherwise
 *
 */
bool fg_tape_get_signal(FGTape *self, const char *name, FGTapeSignal *signal)
{
    int i;
    FGTapeSignalSet *type_set;

    for(SignalType i = TDouble; i < NTypes; i++){
        type_set = &self->signals[i];
        for(int j = 0; j < type_set->count; j++){
            if(!strcmp(type_set->names[j], name)){
                signal->type = i;
                signal->idx = j;
                signal->interpolation = type_set->ipol_types[j];

                signal->offset = 0;
                for(int k = 0; k < i; k++)
                    signal->offset += types_sizes[k]*self->signals[k].count;
                if(i != TBool)
                    signal->offset += types_sizes[i]*signal->idx;
                //printf("signal %s type %s idx %d is offset %d\n",name, pretty_types[i], j, signal->offset);
                return true;
            }
        }
    }
    return false;
}

void *fg_tape_record_get_signal_value_ptr(FGTapeRecord *self, FGTapeSignal *signal)
{
    void *rv;
    static bool vtrue = true;
    static bool vfalse = false;

    rv = self->data + signal->offset;
    if(signal->type == TBool){
        int byte_idx = signal->idx/8;
        int local_bit_idx = signal->idx - byte_idx*8;
        bool value;

        rv += sizeof(unsigned char)*byte_idx;

        value = ((*(uint8_t*)rv) & (1<<(local_bit_idx)));
        if(value)
            rv =&vtrue;
        else
            rv = &vfalse;
    }

    return rv;
}

static inline bool fg_tape_time_within(FGTape *self, double time, uint8_t term)
{
    if(time <= fg_tape_last(self, term)->sim_time && time >= fg_tape_first(self, term)->sim_time)
        return true;
    return false;
}

static inline bool fg_tape_time_inbetween(FGTape *self, double time, uint8_t older_term, uint8_t recent_term )
{
    if(time < fg_tape_first(self, older_term)->sim_time && time > fg_tape_last(self, recent_term)->sim_time)
        return true;
    return false;
}


bool fg_tape_get_keyframes_from_term(FGTape *self, double time, uint8_t term, FGTapeRecord **k1, FGTapeRecord **k2)
{
    // sanity checking
    if( !self->records[term].record_count ){
        // handle empty list
        return false;
    }else if( self->records[term].record_count == 1 ){
        // handle list size == 1
        *k1 = fg_tape_first(self, term);
        *k2 = NULL;
        return true;
    }

    unsigned int last = self->records[term].record_count - 1;
    unsigned int first = 0;
    unsigned int mid = ( last + first ) / 2;

    bool done = false;
    while ( !done )
    {
        // printf("first <=> last\n");
        if ( last == first ) {
            done = true;
        } else if (fg_tape_get_record(self, term, mid)->sim_time < time && fg_tape_get_record(self, term, mid+1)->sim_time < time ) {
            // too low
            first = mid;
            mid = ( last + first ) / 2;
        } else if ( fg_tape_get_record(self, term, mid)->sim_time > time && fg_tape_get_record(self, term, mid+1)->sim_time > time ) {
            // too high
            last = mid;
            mid = ( last + first ) / 2;
        } else {
            done = true;
        }
    }

    *k1 = fg_tape_get_record(self, term, mid+1);
    *k2 = fg_tape_get_record(self, term, mid);
    return true;
}

bool fg_tape_get_keyframes_for(FGTape *self, double time, FGTapeRecord **k1, FGTapeRecord **k2)
{
    /* Short term is the last portion of the record regardless
     * of how long it is: If it's empty there is nothing to play
     */
    if(!self->records[SHORT_TERM].record_count)
        return false;

    /* Time is past the last frame of the record*/
    if(time > fg_tape_last(self, SHORT_TERM)->sim_time){
        *k1 = fg_tape_last(self, SHORT_TERM);
        *k2 = NULL;
        return true;
    }

    if ( fg_tape_time_within(self, time, SHORT_TERM)) { //Time is within short_term bounds
        return fg_tape_get_keyframes_from_term(self, time, SHORT_TERM, k1, k2);
    } else if (self->records[MEDIUM_TERM].record_count ) { //Time is NOT within short_term
        if ( fg_tape_time_inbetween(self, time, SHORT_TERM, MEDIUM_TERM)){ //Time is between medium_term last frame and short term first frame
            *k1 = fg_tape_last(self, MEDIUM_TERM);
            *k2 = fg_tape_first(self, SHORT_TERM);
            return true;
        } else { //Time is at least before medium_term last frame
            if (fg_tape_time_within(self, time, MEDIUM_TERM)) { //Time is within medium term bounds
                return fg_tape_get_keyframes_from_term(self, time, MEDIUM_TERM, k1, k2);
            } else if(self->records[LONG_TERM].record_count){ //Time is strictly before first frame of medium_term
                if (fg_tape_time_inbetween(self, time, MEDIUM_TERM, LONG_TERM)){ //Time is between first frame of medium_term and last frame of long_term
                    *k1 = fg_tape_last(self, LONG_TERM);
                    *k2 = fg_tape_first(self, MEDIUM_TERM);
                    return true;
                } else {
                    if (fg_tape_time_within(self, time, LONG_TERM)) { //Time is within long_term bounds
                        return fg_tape_get_keyframes_from_term(self, time, LONG_TERM, k1, k2);
                    } else { //Time is before the first frame of long_terms
                        *k1 = fg_tape_first(self, LONG_TERM);
                        *k2 = NULL;
                        return true;
                    }
                }
            } else { //Time is before the first frame of medium term AND there is no long term to search into
                *k1 = fg_tape_first(self, MEDIUM_TERM);
                *k2 = NULL;
                return true;
            }
        }
    } else { //Time is before the first frame of short term and there is no mid_term to continue looking for more ancient frames
        *k1 = fg_tape_first(self, SHORT_TERM);
        *k2 = NULL;
        return true;
    }

    return false;
}

static double weighting(IpolType interpolation, double ratio, double v1, double v2)
{

  //  printf("Doing %s interpolation between %f and %f\n", pretty_ipols[interpolation], v1, v2);
    switch(interpolation){
      case IPLinear:
          return v1 + ratio*(v2-v1);

      case IPAngularDeg:
      {
          // special handling of angular data
          double tmp = v2 - v1;
          if ( tmp > 180 )
              tmp -= 360;
          else if ( tmp < -180 )
              tmp += 360;
          return v1 + tmp * ratio;
      }

      case IPAngularRad:
      {
          // special handling of angular data
          double tmp = v2 - v1;
          if ( tmp > M_PI )
              tmp -= (M_PI * 2.0);
          else if ( tmp < -M_PI )
              tmp += (M_PI * 2.0);
          return v1 + tmp * ratio;
      }

      case IPDiscrete:
          // fall through
      default:
          return v2;
    }
}

/**
 * Will interpolate between leftbound and rightboud a number that is
 * greater than or equal leftbound and lower than or equal rightbound
 * depending on ratio. 0.0 being leftbound, 1.0 being rightbound, any
 * other value will be in between
 *
 */
void fg_tape_interpolate_values(SignalType type, IpolType itype, double ratio, void *leftbound, void *rightbound, void *destination)
{
//    printf("%s: type: %s, itype: %s\n",__FUNCTION__, pretty_kinds[type], pretty_ipols[itype]);
        switch(type){
            case TDouble:
                *((double*)(destination)) = leftbound ?
                    weighting(itype, ratio, *((double*)leftbound), *((double*)rightbound))
                    : *((double*)rightbound);
                break;
            case TFloat:
                *((float*)(destination)) = leftbound ?
                    weighting(itype, ratio, *((float*)leftbound), *((float*)rightbound))
                    : *((float*)rightbound);
                break;
            /*There is no real interpolation for anything other than floats/doubles
             * in original FG's code so leave it at that */
            case TInt:
                *((int *)destination) = *((int *)rightbound);
                break;
            case TInt16:
                *((short int *)destination) = *((short int *)rightbound);
                break;
            case TIn8:
                *((signed char *)destination) = *((signed char *)rightbound);
                break;
            case TBool:
                *((bool*)destination) = *((bool *)rightbound);
                break;
            case NTypes: //Fall through and make the compiler happy
            default:
                break;
        }
}

/* Get a cursor for a given time in the tape (between 0 and tape->duration)
 *
 * Returns -1 on error, 1 until the end of tape hasn't
 * been reached, 0 otherwise
 */
int fg_tape_get_cursor(FGTape *self, double time, FGTapeCursor *cursor)
{
    bool rv;

    /* k1: value just after time, k2, previous keyframe:
     * time->
     * ...k2...t...k1
     * */
    rv = fg_tape_get_keyframes_for(self, time, &cursor->k1, &cursor->k2);
    if(!rv){
        printf("Couldn't set cursor for time %f\n",time);
        return -1;
    }
    cursor->ratio = 1.0;
    if(cursor->k2){
        double NextSimTime = cursor->k1->sim_time;
        double LastSimTime = cursor->k2->sim_time;
        double Numerator = time - LastSimTime;
        double dt = NextSimTime - LastSimTime;
        // avoid divide by zero and other quirks
        if ((Numerator > 0.0)&&(dt != 0.0)){
              cursor->ratio = Numerator / dt;
              if (cursor->ratio > 1.0)
                  cursor->ratio = 1.0;
        }
    }
    if(time > self->duration)
        return 0;

    return 1;
}

bool fg_tape_cursor_get_signal_value(FGTapeCursor *self, size_t nsignals, FGTapeSignal *signals, void *buffer)
{
    void *buffer_cursor;
    void *v1, *v2;

    buffer_cursor = buffer;

    for(int i = 0; i < nsignals; i++){
        FGTapeSignal *signal = &signals[i];
        v1 = fg_tape_record_get_signal_value_ptr(self->k1, signal);
        if(self->k2)
            v2 = fg_tape_record_get_signal_value_ptr(self->k2, signal);
        else
            v2 = NULL;
        fg_tape_interpolate_values(signal->type, signal->interpolation , self->ratio, v2, v1, buffer_cursor);
        buffer_cursor += types_sizes[signal->type];
    }
    return true;
}


int fg_tape_get_data_at(FGTape *self, double time, size_t nsignals, FGTapeSignal *signals, void *buffer)
{
    int rv;
    FGTapeCursor cursor;

    rv = fg_tape_get_cursor(self, time, &cursor);
    if(rv >= 0)
        fg_tape_cursor_get_signal_value(&cursor, nsignals, signals, buffer);
    return rv;
}
