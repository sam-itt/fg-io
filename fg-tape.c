#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
    static size_t current_size = 0;
    static char *look_for = NULL; //TODO: Remove this and search for < and then node_name

    int len;

    if(!node_name && look_for){
        current_size = 0;
        free(look_for);
        return false;
    }

    str->str = NULL;
    str->len = 0;

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
        return false;
    }
    cursor = strchr(cursor, '>');
    if(!cursor){
        printf("Couldn't find %s's GT\n", look_for);
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
//
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
    bool ret;

    rv = calloc(1,sizeof(FGTape));
    if(!rv)
        return NULL;

    rv->file = sg_file_open(filename);
    if(!rv->file){
        free(rv);
        return NULL;
    }

    sg_file_get_container(rv->file, 0, &container); //Skip first "header" type container


    fg_tape_read_duration(rv, rv->file);

    fg_tape_read_signals(rv, rv->file);

    _get_node_value(NULL, NULL, NULL); //clear internal buffer


    ret = sg_file_get_container(rv->file, 3, &container);
    if(!ret){
        printf("Couldn't find payload container, bailing out\n");
        return NULL;
    }

    ret = sg_file_get_payload(rv->file, &container, &(rv->data));
    if(!ret){
        printf("Couldn't get payload, bailing out\n");
        free(rv);
        return NULL;
    }

    return rv;
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

