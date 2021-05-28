//
// Created by mister_hz on 2021-05-16.
//


#ifndef TUNNELS_STRUCTS_H
#define TUNNELS_STRUCTS_H

typedef struct proc_queue_n process_queue_node;
typedef struct proc process_s;

struct proc {
    int id;
    int ts;
    int resource_id;
};

struct proc_queue_n {
    process_s* proc;
    process_queue_node* prev;
    process_queue_node* next;
};

int is_less(process_s*, process_s*);
process_s* create_process_s(int, int, int);
process_s* copy_process_s(process_s*);

#endif //TUNNELS_STRUCTS_H
