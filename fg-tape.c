#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "fg-tape.h"
#include "sg-file.h"


char *_get_node_value(const char *xml, const char *node_name, size_t *olen);

bool fg_tape_read_duration(FGTape *self, SGFile *file)
{
    SGContainer container;
    char *xml;
    char *cursor;
    char *end;

    sg_file_read_next(file, &container);
    if(container.type != 1){
        printf("Second container isn't of expected type(%d), bailing out\n",1);
        return NULL;
    }

    xml = NULL;
    sg_file_get_payload(file, &container, (uint8_t**)&xml);

    cursor =  _get_node_value(xml, "tape-duration", NULL);
    if(!cursor){
        printf("Couldn't find tape-duration's value in file xml descriptor, bailing out\n");
        return false;
    }
    self->duration = atof(cursor);

    return true;
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
char *_get_node_value(const char *xml, const char *node_name, size_t *olen)
{
    static size_t current_size = 0;
    static char *look_for = NULL;

    int len;

    if(!node_name && look_for){
        current_size = 0;
        free(look_for);
        return NULL;
    }

    len = strlen(node_name);
    if(len+2 > current_size){ //We need one mor char for '<' and one more byte for '\0'
        char *tmp;
        tmp = realloc(look_for, len + 2);
        if(!tmp)
            return NULL;
        look_for = tmp;
        current_size = len + 2;
    }

    *look_for = '<';
    strncpy(look_for+1, node_name, len);
    *(look_for + 1 + len) = '\0';

    char *tmp;
    char *cursor;

    cursor = strstr(xml, look_for); // <node_name
    if(!cursor){
        printf("Couldn't find %s> tag in given xml string\n", look_for);
        return NULL;
    }
    cursor = strchr(cursor, '>');
    if(!cursor){
        printf("Couldn't find %s's GT\n", look_for);
        return NULL;
    }

    if(olen){
        //<type bar="foo">data</type> whe are at '>'
        //count distance to closing </type> to read what's in between
        tmp = strchr(cursor, '<');
        if(!tmp){
            printf("Couldn't find </%s> closing tag\n",node_name);
            return false;
        }
        *olen = (tmp-1) - cursor;
    }

    return cursor+1;
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

    size_t tlen;
    char *tmptype = _get_node_value(begin, "type", &tlen);
    //printf("Read signal type: %.*s\n", tlen, tmptype);

    char *prop = _get_node_value(begin, "property", &tlen);
    if(!prop){
        printf("Couldn't find property for current %s signal, bailing out\n", tmptype);
        return false;
    }

    size_t ilen;
    char *ipol_type = _get_node_value(begin, "interpolation", &ilen);
    uint8_t tmpipol;

    if(ipol_type){
        if(!strncmp(ipol_type, "linear", ilen)){
            tmpipol = IPOL_LINEAR;
        }else if(!strncmp(ipol_type, "angular-deg", ilen)){
            tmpipol = IPOL_ANGULAR_DEG;
        }else if(!strncmp(ipol_type, "angular-rad", ilen)){
            tmpipol = IPOL_ANGULAR_RAD;
        }else{
            printf("Unknown interpolation type: %.*s, bailing out\n", ilen, ipol_type);
            return false;
        }
    }else{
        printf("Couldn't find interpolation type for current %s signal, assuming linear\n", tmptype);
        tmpipol = IPOL_LINEAR;
    }

    switch(tmptype[0]){
        case 'd': //double
            self->signals.doubles.names[self->signals.doubles.count] =  strndup(prop, tlen);
            self->signals.doubles.ipol_types[self->signals.doubles.count] = tmpipol;
            self->signals.doubles.count++;
            break;
        case 'f': //float
            self->signals.floats.names[self->signals.floats.count] =  strndup(prop, tlen);
            self->signals.floats.ipol_types[self->signals.floats.count] = tmpipol;
            self->signals.floats.count++;
            break;
        case 'i': //ints
            if(!strcmp(tmptype,"int")){
                self->signals.ints.names[self->signals.ints.count] =  strndup(prop, tlen);
                self->signals.ints.ipol_types[self->signals.ints.count] = tmpipol;
                self->signals.ints.count++;
            }else if(!strcmp(tmptype,"int16")){
                self->signals.int16s.names[self->signals.int16s.count] =  strndup(prop, tlen);
                self->signals.int16s.ipol_types[self->signals.int16s.count] = tmpipol;
                self->signals.int16s.count++;
            }else if(!strcmp(tmptype,"int8")){
                self->signals.int8s.names[self->signals.int8s.count] =  strndup(prop, tlen);
                self->signals.int8s.ipol_types[self->signals.int8s.count] = tmpipol;
                self->signals.int8s.count++;
            }
            break;
        case 'b': //bool
            self->signals.bools.names[self->signals.bools.count] =  strndup(prop, tlen);
            self->signals.bools.ipol_types[self->signals.bools.count] = tmpipol;
            self->signals.bools.count++;
            break;
    };
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
    char *cursor;

    cursor = _get_node_value(xml, type, NULL);
    if(!cursor){
        printf("Couldn't find count of '%s' signals, bailing out\n", type);
        return -1;
    }
    n_signals = atoi(cursor);
    self->names = calloc(sizeof(char*), n_signals);
    self->ipol_types = calloc(sizeof(uint8_t), n_signals);

    return n_signals;
}

const char *fg_tape_signal_ipol_pretty(int ipol_type)
{
    switch(ipol_type){
        case 0: return "linear";
        case 1: return "angular-deg";
        case 2: return "angular-rad";
    };
    return "unknown";
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
//    printf("\tNumber of records: %d\n",self->record_count);
    printf("\tEach record has:\n"
        "\t\t%d doubles\n"
        "\t\t%d floats\n"
        "\t\t%d ints\n"
        "\t\t%d int16s\n"
        "\t\t%d int8s\n"
        "\t\t%d bools\n",
        self->signals.doubles.count,
        self->signals.floats.count,
        self->signals.ints.count,
        self->signals.int16s.count,
        self->signals.int8s.count,
        self->signals.bools.count
    );
    printf("\tDouble signals:\n");
    fg_tape_signal_kind_dump(&(self->signals.doubles), 2);
    printf("\tFloat signals:\n");
    fg_tape_signal_kind_dump(&(self->signals.floats), 2);
    printf("\tInt signals:\n");
    fg_tape_signal_kind_dump(&(self->signals.ints), 2);
    printf("\tInt16 signals:\n");
    fg_tape_signal_kind_dump(&(self->signals.int16s), 2);
    printf("\tInt8 signals:\n");
    fg_tape_signal_kind_dump(&(self->signals.int8s), 2);
    printf("\tBool signals:\n");
    fg_tape_signal_kind_dump(&(self->signals.bools), 2);
}

bool fg_tape_read_signals(FGTape *self, SGFile *file)
{
    SGContainer container;
    char *xml;
    char *cursor;
    char *end;

    sg_file_read_next(file, &container); /*TODO: Move that up to sg_file_get_container(type)*/
    if(container.type != 2){
        printf("Second container isn't of expected type(%d), bailing out\n",2);
        return NULL;
    }

    xml = NULL;
    sg_file_get_payload(file, &container, (uint8_t**)&xml);
    end = xml + container.size;

    int count[6];
    count[0] = fg_tape_signal_kind_init(&(self->signals.doubles), "double", xml);
    count[1] = fg_tape_signal_kind_init(&(self->signals.floats), "float", xml);
    count[2] = fg_tape_signal_kind_init(&(self->signals.ints), "int", xml);
    count[3] = fg_tape_signal_kind_init(&(self->signals.int16s), "int16", xml);
    count[4] = fg_tape_signal_kind_init(&(self->signals.int8s), "int8", xml);
    count[5] = fg_tape_signal_kind_init(&(self->signals.bools), "bool", xml);

    for(int i = 0; i < 6; i++){
        self->signals.total_count += count[i] < 0 ? 0 : count[i];
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

    for(int i = 0; i < self->signals.total_count; i++)
        fg_tape_read_signal(self, &cursor);
#if 0
    fg_tape_dump(self);
    exit(0);
#endif
    return true;
}

FGTape *fg_tape_open(const char *filename)
{
    FGTape *rv;
    SGContainer container;
    char *xml;

    rv = calloc(1,sizeof(FGTape));
    if(!rv)
        return NULL;

    rv->file = sg_file_open(filename);
    if(!rv->file){
        free(rv);
        return NULL;
    }

    sg_file_read_next(rv->file, &container); //Skip first "header" type container


    fg_tape_read_duration(rv, rv->file);

    fg_tape_read_signals(rv, rv->file);

    _get_node_value(NULL, NULL, NULL); //clear internal buffer


    sg_file_read_next(rv->file, &container); /*TODO: Move that up to sg_file_get_container(type)*/
    if(container.type != 3){
        printf("Second container isn't of expected type(%d), bailing out\n",1);
        return NULL;
    }

    bool ret;
    ret = sg_file_get_payload(rv->file, &container, &(rv->data));
    if(!ret){
        printf("Couldn't get payload, bailing out\n");
        free(rv);
        return NULL;
    }

    return rv;
}

bool fg_tape_get_signal(FGTape *self, const char *name, FGTapeSignal *signal)
{
    int i;
    for(i = 0; i < self->signals.doubles.count; i++){
        if(!strcmp(self->signals.doubles.names[i], name)){
            signal->idx = i;
            signal->type = TDOUBLE;
            return true;
        }
    }

    for(i = 0; i < self->signals.floats.count; i++){
        if(!strcmp(self->signals.floats.names[i], name)){
            signal->idx = i;
            signal->type = TFLOAT;
            return true;
        }
    }

    for(i = 0; i < self->signals.ints.count; i++){
        if(!strcmp(self->signals.ints.names[i], name)){
            signal->idx = i;
            signal->type = TINT;
            return true;
        }
    }

    for(i = 0; i < self->signals.int16s.count; i++){
        if(!strcmp(self->signals.int16s.names[i], name)){
            signal->idx = i;
            signal->type = TINT16;
            return true;
        }
    }

    for(i = 0; i < self->signals.int8s.count; i++){
        if(!strcmp(self->signals.int8s.names[i], name)){
            signal->idx = i;
            signal->type = TINT8;
            return true;
        }
    }

    for(i = 0; i < self->signals.bools.count; i++){
        if(!strcmp(self->signals.bools.names[i], name)){
            signal->idx = i;
            signal->type = TBOOL;
            return true;
        }
    }
    return false;
}

double fg_tape_get_record_double_value(FGTape *self, FGTapeRecord *record, size_t idx)
{
    double rv;

    rv = *(double*)(record->data + sizeof(double)*idx);

    return rv;
}

float fg_tape_get_record_float_value(FGTape *self, FGTapeRecord *record, size_t idx)
{
    float rv;

    rv = *(float *)(record->data +
        sizeof(double)*self->signals.doubles.count +
        sizeof(float)*idx
    );

    return rv;
}

int fg_tape_get_record_int_value(FGTape *self, FGTapeRecord *record, size_t idx)
{
    float rv;

    rv = *(int *)(record->data +
        sizeof(double)*self->signals.doubles.count +
        sizeof(float)*self->signals.floats.count +
        sizeof(int) * idx
    );
    return rv;
}

short int fg_tape_get_record_int16_value(FGTape *self, FGTapeRecord *record, size_t idx)
{
    short int rv;

    rv = *(short int*)(record->data +
        sizeof(double)*self->signals.doubles.count +
        sizeof(float)*self->signals.floats.count +
        sizeof(int)*self->signals.ints.count +
        sizeof(short int) * idx
    );
    return rv;
}

signed char fg_tape_get_record_int8_value(FGTape *self, FGTapeRecord *record, size_t idx)
{
    signed char rv;

    rv = *(signed char*)(record->data +
        sizeof(double)*self->signals.doubles.count +
        sizeof(float)*self->signals.floats.count +
        sizeof(int)*self->signals.ints.count +
        sizeof(short int) * self->signals.ints.count +
        sizeof(signed char) * idx
    );
    return rv;
}

#if 0
//Unimplemented
bool fg_tape_get_record_bool_value(FGTape *self, FGTapeRecord *record, size_t idx)
{
    bool rv;

    rv = *(record->data + sizeof(double) +
        sizeof(double)*self->signals.doubles.count +
        sizeof(float)*self->signals.floats.count +
        sizeof(int)*self->signals.ints.count +
        sizeof(short int) * self->signals.ints.count +
        sizeof(signed char) * self->signals.int8s.count
    );
    return rv;
}
#endif


double fg_tape_get_record_signal_double_value(FGTape *self, FGTapeRecord *record, FGTapeSignal *signal)
{
    return fg_tape_get_record_double_value(self, record, signal->idx);
}

float fg_tape_get_record_signal_float_value(FGTape *self, FGTapeRecord *record, FGTapeSignal *signal)
{
    return fg_tape_get_record_float_value(self, record, signal->idx);
}
