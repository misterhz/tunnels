#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include "structs.h"
#include "queue.h"

int main() {
    process_queue_node* head = NULL;

    queue_add(&head, create_process_s(1, 10));
    queue_add(&head, create_process_s(2, 8));
    queue_add(&head, create_process_s(3, 7));
    queue_add(&head, create_process_s(4, 10));
    queue_add(&head, create_process_s(5, 15));
    queue_add(&head, create_process_s(6, 13));
    queue_add(&head, create_process_s(7, 20));
    queue_add(&head, create_process_s(8, 10));

    queue_remove(&head, 3);
    queue_remove(&head, 7);
    queue_remove(&head, 4);
    queue_remove(&head, 20);

    int a = 0;
}
