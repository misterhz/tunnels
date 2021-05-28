//
// Created by mister_hz on 2021-05-16.
//

#include <stdlib.h>
#include "structs.h"

int is_less(process_s* p1, process_s* p2) {
    return (p1->ts < p2->ts) || ((p1->ts == p2->ts) && (p1->id < p2->id));
}

process_s* create_process_s(int id, int ts, int r_id) {
    process_s* p = malloc(sizeof(process_s));
    p->id = id;
    p->ts = ts;
    p->resource_id = r_id;
    return p;
}

process_s* copy_process_s(process_s* src) {
    process_s* dst = malloc(sizeof(process_s));
    dst->id = src->id;
    dst->resource_id = src->resource_id;
    dst->ts = src->ts;
    return dst;
}
