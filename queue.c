//
// Created by mister_hz on 2021-05-16.
//
#include <stdlib.h>
#include "queue.h"
#include <stddef.h>

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

int queue_is_head(process_queue_node* head, int id) {
}

