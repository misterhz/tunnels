//
// Created by mister_hz on 2021-05-16.
//
#include <stdlib.h>
#include "queue.h"
#include <stddef.h>
#include <stdio.h>

void queue_add(process_queue_node** head, process_s* p) {
    process_queue_node* node = malloc(sizeof(process_queue_node));

    node->proc = p;
    node->next = NULL;
    node->prev = NULL;

    if(*head == NULL) {
        *head = node;
    } else if(is_less(node->proc, (*head)->proc)){
        process_queue_node* former_head = *head;
        *head = node;

        node->next = former_head;
        former_head->prev = node;
    } else {
        process_queue_node* insert_before = *head;
        int broke = 0;
        int cont = 1;
        while(cont) {
            if(insert_before->next != NULL) {
                insert_before = insert_before->next;
                cont = !is_less(node->proc, insert_before->proc);
            } else {
                process_queue_node* last_node = insert_before; // |->|->|->null -> |->|->|->node->null
                broke = 1;

                last_node->next = node;
                node->prev = last_node;
                break;
            }
        }
        if(!broke) {
            process_queue_node* prev = insert_before->prev;

            prev->next = node;
            node->prev = prev;

            node->next = insert_before;
            insert_before->prev = node;
        }
    }
}

void queue_remove(process_queue_node** head, int id) {
    if(*head == NULL) {
        return;
    }
    process_queue_node* to_be_removed = *head;
    while(to_be_removed != NULL) {
        if(to_be_removed->proc->id != id) {
            to_be_removed = to_be_removed->next;
        } else {
            break;
        }
    }
    if(to_be_removed == NULL) {
        return;
    } else {
        process_queue_node* prev = to_be_removed->prev;
        process_queue_node* next = to_be_removed->next;

        if(prev != NULL) {
            prev->next = next;
        }
        if(next != NULL) {
            next->prev = prev;
        }

        if(to_be_removed == *head) {
            *head = next;
        }

        free(to_be_removed->proc);
        free(to_be_removed);
    }
}

void queue_clear(process_queue_node** head){
    if(*head == NULL)
        return;
    process_queue_node* to_be_removed = *head;
    while(to_be_removed != NULL) {
        process_queue_node* temp = to_be_removed->next;
        free(to_be_removed->proc);
        free(to_be_removed);
        to_be_removed = temp;
    }
    *head = NULL;
}

process_queue_node* queue_copy(process_queue_node* src) {
    process_queue_node* current_src = src;

    process_queue_node* current_dst = NULL;
    process_queue_node* prev_dst = NULL;

    process_queue_node* dst_head = NULL;
    int head_created = 0;
    while(current_src != NULL) {
        prev_dst = current_dst;
        current_dst = malloc(sizeof(process_queue_node));

        if(!head_created) {
            dst_head = current_dst;
            head_created = 1;
        }
        
        process_s* src_process_s = current_src->proc;
        process_s* dst_process_s = copy_process_s(src_process_s);

        current_dst->proc = dst_process_s;

        current_dst->prev = prev_dst;
        if(prev_dst != NULL) {
            prev_dst->next = current_dst;
        }
        current_src = current_src->next;
    }
    return dst_head;
}

int queue_get_position(process_queue_node* head, int id) {
    process_queue_node* current_node = head;
    int index = 0;
    while(current_node->proc->id != id) {
        printf("next %d", index);
        current_node = current_node->next;
        ++index;
    }
    return index;
}

void queue_print(process_queue_node* head) {
    process_queue_node* current_node = head;
    while(current_node != NULL) {
        printf("id %d, ts %d\n", current_node->proc->id, current_node->proc->ts);
        current_node = current_node->next;
    }
}

