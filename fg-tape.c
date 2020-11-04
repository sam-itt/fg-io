#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#include "fg-tape.h"
#include "sg-file.h"


static uint8_t kinds[NKINDS] = {KDOUBLE,KFLOAT,KINT,KINT16,KINT8,KBOOL};
static char *pretty_kinds[NKINDS] = {"double","float","int","int16","int8","bool"};
static size_t kind_sizes[NKINDS] = {
    sizeof(double), sizeof(float), sizeof(int),
    sizeof(short int), sizeof(signed char), sizeof(unsigned char)
};

static uint8_t ipols[NIPOLS] = {IPOL_LINEAR, IPOL_ANGULAR_DEG, IPOL_ANGULAR_RAD};
static char *pretty_ipols[NIPOLS] = {"linear", "angular-deg", "angular-rad"};

static char *pretty_terms[NTERMS] = {"short","mid","long"};

typedef struct{
    char *str;
    size_t len;
}XString;

bool _get_node_value(const char *xml, const char *node_name, XString *str);

const char *fg_tape_signal_ipol_pretty(uint8_t ipol_type)
{
    return ipol_type < NIPOLS ? pretty_ipols[ipol_type]: "unknown";
}


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
    for(uint8_t i = 0; i < NIPOLS; i++){
        if(!strncmp(ipol_type.str, pretty_ipols[i], ipol_type.len)){
            tmpipol = ipols[i];
            break;
        }
    }
    if( tmpipol < 0){
        printf("Unknown interpolation type: %.*s, bailing out\n", ipol_type.len, ipol_type.str);
        return false;
    }


    FGTapeSignalKind *kind = NULL;
    for(uint8_t i = 0; i < NKINDS; i++){
        if(!strncmp(tmptype.str, pretty_kinds[i], tmptype.len)){
            kind = &self->signals[kinds[i]];
        }
    }
    if(!kind){
        printf("Couldn't find a matching signal kind for read value %.*s",tmptype.len, tmptype.str);
        return false;
    }

    kind->names[kind->count] =  strndup(prop.str, prop.len); //TODO: free me
    kind->ipol_types[kind->count] = tmpipol;
    kind->count++;

    *cursor = end;
    return true;
}

/* *
 * Use data from the xml to allocate enough space to hold
 * signals data of a given kind (double, float, ...)
 *
 * self->count is set to 0 but names and ipol_types are allocated
 * to fit as many signals as declared in the file <type></type>
 * node
 *
 * returns: Number of allocated signals or -1 on failure
 */
int fg_tape_signal_kind_init(FGTapeSignalKind *self, const char *type, const char *xml)
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

void fg_tape_signal_kind_dispose(FGTapeSignalKind *self)
{
    for(int i = 0; i < self->count; i++){
        free(self->names[i]);
    }
    free(self->names);
    free(self->ipol_types);
}

void fg_tape_signal_kind_dump(FGTapeSignalKind *self, uint8_t level)
{
    int i;

    for(i = 0; i < self->count; i++){
        for(int j = 0; j < level; j++) putchar('\t');
        printf("%s - %s\n",
            self->names[i],
            fg_tape_signal_ipol_pretty(self->ipol_types[i])
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
    for(int i = 0; i < NKINDS; i++){
        printf("\t\t%d %s\n",
            self->signals[kinds[i]].count,
            pretty_kinds[i]
        );
    }

    for(int i = 0; i < NKINDS; i++){
        if(self->signals[kinds[i]].count > 0){
            printf("\t%s signals:\n", pretty_kinds[i]);
            fg_tape_signal_kind_dump(&(self->signals[kinds[i]]), 2);
        }else{
            printf("\t%s signals: None\n", pretty_kinds[i]);
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

    int count[NKINDS];
    for(int i = 0; i < NKINDS; i++){
        count[i] = fg_tape_signal_kind_init(
            &(self->signals[kinds[i]]), /*Might as well use 'i' direcly?*/
            pretty_kinds[i],
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
    printf("Container should have %d records\n", set->record_count);

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

#if 0

    FGTapeRecord *first, *last;
    first = fg_tape_get_record(self, 0);
    last = fg_tape_get_record(self, self->record_count-1);
    double sim_duration = last->sim_time - first->sim_time;
//    self->duration //seconds
    printf("first->sim_time: %f, last->sim_time: %f\n", first->sim_time, last->sim_time);
    printf("sim_duration: %f, duration(seconds): %f\n",sim_duration,self->duration);
    self->sec2sim = sim_duration / self->duration;
    self->first_stamp = first->sim_time;

#endif
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
    for(int i = 0; i < NKINDS; i++)
        fg_tape_signal_kind_dispose(&self->signals[i]);

    for(int i = 0; i < NTERMS; i++){
        if(self->records[i].data)
            free(self->records[i].data);
    }
    free(self);
}

/**
 * Searchs for a FGTapeSignal signal descriptor(array + index in array) matching
 * "name"
 *
 *
 */
bool fg_tape_get_signal(FGTape *self, const char *name, FGTapeSignal *signal)
{
    int i;
    FGTapeSignalKind *kind;

    for(int i = 0; i < NKINDS; i++){
        kind = &self->signals[kinds[i]];
        for(int j = 0; j < kind->count; j++){
            if(!strcmp(kind->names[j], name)){
                signal->type = kinds[i];
                signal->idx = j;
                return true;
            }
        }
    }
    return false;
}

void *fg_tape_get_value_ptr(FGTape *self, FGTapeRecord *record, uint8_t kind, size_t idx)
{
    void *rv;
    static bool _bstore; //Not thread safe

    rv = record->data;
    for(int i = 0; i < kind; i++)
        rv += kind_sizes[i]*self->signals[kinds[i]].count;
    if(kind != KBOOL){
        rv += kind_sizes[kind]*idx;
    }else{
        int byte_idx = idx/8;
        int local_bit_idx = idx - byte_idx*8;

        rv += sizeof(unsigned char)*byte_idx;

        _bstore = ((*(uint8_t*)rv) & (1<<(local_bit_idx)));
        rv = &_bstore;
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
    } else if (self->records[MID_TERM].record_count ) { //Time is NOT within short_term
        if ( fg_tape_time_inbetween(self, time, SHORT_TERM, MID_TERM)){ //Time is between medium_term last frame and short term first frame
            *k1 = fg_tape_last(self, MID_TERM);
            *k2 = fg_tape_first(self, SHORT_TERM);
            return true;
        } else { //Time is at least before medium_term last frame
            if (fg_tape_time_within(self, time, MID_TERM)) { //Time is within medium term bounds
                return fg_tape_get_keyframes_from_term(self, time, MID_TERM, k1, k2);
            } else if(self->records[LONG_TERM].record_count){ //Time is strictly before first frame of medium_term
                if (fg_tape_time_inbetween(self, time, MID_TERM, LONG_TERM)){ //Time is between first frame of medium_term and last frame of long_term
                    *k1 = fg_tape_last(self, LONG_TERM);
                    *k2 = fg_tape_first(self, MID_TERM);
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
                *k1 = fg_tape_first(self, MID_TERM);
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


