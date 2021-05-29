#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "main.h"

state_t state = MEDIUM_PREPARE;
int size, rank; /* nie trzeba zerować, bo zmienna globalna statyczna */
pthread_t comm_thread;

pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ts_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t shop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t medium_queue_mutex = PTHREAD_MUTEX_INITIALIZER; // maybe change to array
pthread_mutex_t in_tunnel_mutex = PTHREAD_MUTEX_INITIALIZER; // maybe change to array
pthread_mutex_t medium_usage_mutex = PTHREAD_MUTEX_INITIALIZER;


MPI_Datatype MPI_PACKET_T;

void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: chcemy %d. Co otrzymamy?\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
	    fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
	    MPI_Finalize();
	    exit(-1);
	    break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
        case MPI_THREAD_SERIALIZED: 
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n"); /* tego chcemy. Wszystkie inne powodują problemy */
	    break;
        default: printf("Nikt nic nie wie\n");
    }
}

int init(int* argc, char*** argv) {
    int provided;
    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);

    /* Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
    const int nitems=3; /* bo packet_t ma trzy pola */
    int       blocklengths[3] = {1,1,1};
    MPI_Datatype typy[3] = {MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint     offsets[3]; 
    offsets[0] = offsetof(process_s, id);
    offsets[1] = offsetof(process_s, ts);
    offsets[2] = offsetof(process_s, resource_id);
    
    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PACKET_T);
    MPI_Type_commit(&MPI_PACKET_T);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);

    // pthread_create( &comm_thread, NULL, start_comm_thread, 0);

    debug("jestem");

    medium_queue_table = malloc(M * sizeof(process_queue_node*));
    for(int i = 0; i < M; i++) {
        medium_queue_table[i] = NULL;
    }
    // shop_queue doesn't need init
    in_tunnel_queue = malloc(M * sizeof(process_queue_node*));

    medium_usage_table = malloc(M * sizeof(int));
    for(int i = 0; i < M; i++) {
        medium_usage_table[i] = T;
    }

    free_F = F;
    ts = 0;
}

void change_state(state_t new_state) {
    pthread_mutex_lock(&state_mutex);
    state = new_state;
    increase_timestamp(1);
    pthread_mutex_unlock(&state_mutex);
}

void send_packet(int resource_id, int destination, message_t tag) {
    process_s* pkt = malloc(sizeof(process_s));
    pkt->id = rank;
    pkt->resource_id = resource_id;
    pkt->ts = ts;
    MPI_Send( pkt, 1, MPI_PACKET_T, destination, tag, MPI_COMM_WORLD);
    increase_timestamp(1);
    free(pkt);
}

void send_packet_ts(int resource_id, int p_ts, int destination, message_t tag) {
    process_s* pkt = malloc(sizeof(process_s));
    pkt->id = rank;
    pkt->resource_id = resource_id;
    pkt->ts = p_ts;
    MPI_Send( pkt, 1, MPI_PACKET_T, destination, tag, MPI_COMM_WORLD);
    increase_timestamp(1);
    free(pkt);
}

void send_packet_to_everyone(int resource_id, int destination, message_t tag) {
    process_s* pkt = malloc(sizeof(process_s));
    pkt->id = rank;
    pkt->resource_id = resource_id;
    for(int i = 0; i < rank; i++) {
        if(i != rank) {
            pkt->ts = ts;
            MPI_Send(pkt, 1, MPI_PACKET_T, destination, tag, MPI_COMM_WORLD);
            increase_timestamp(1);
        }       
    }
    free(pkt);
}

void receive_packet(process_s* pkt, int tag, MPI_Status* status) {
    MPI_Recv(pkt, 1, MPI_PACKET_T, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, status);
    set_timestamp(pkt->ts, 1);
}

void increase_timestamp(int d) {
    pthread_mutex_lock(&ts_mutex);
    ts += d;
    pthread_mutex_unlock(&ts_mutex);
}

void set_timestamp(int packet_ts, int d) {
    pthread_mutex_lock(&ts_mutex);
    ts = (ts > packet_ts ? ts : packet_ts) + d;
    pthread_mutex_unlock(&ts_mutex);
}

int get_message_count(message_t type) {
    MPI_Status status;
    int msg_num = 0;
    int flag;

    MPI_Iprobe(MPI_ANY_SOURCE, type, MPI_COMM_WORLD, &flag, &status);
    if(flag) {
        MPI_Get_count(&status, MPI_PACKET_T, &msg_num);
    }
    return msg_num;
}

void main_loop()
{
    srandom(rank + 50);
    int ack_num = 0;
    int msg_num = 0;
    int flag = 0;
    MPI_Status status;

    process_s* response_packet = malloc(sizeof(response_packet));

    while (1) {
        // INIT

        int chosen_medium = choose_medium_index();

        println("wanna medium[%d]", chosen_medium);

        change_state(INIT);
        int msg_ts = ts; // all processes have to have request with same ts
        for(int i = 0; i < size; i++) {
            if(rank != i) {
                send_packet_ts(chosen_medium, msg_ts, i, MEDIUM_REQUEST);
            }
        }
        debug("sent MEDIUM_REQUEST to everyone");

        add_to_medium_queue(create_process_s(rank, msg_ts, chosen_medium), chosen_medium);
        debug("added myself to medium_queue[%d]", chosen_medium);

        change_state(WAITING_FOR_MEDIUM);
        println("switched state to WAITING_FOR_MEDIUM");

        // WAITING_FOR_MEDIUM
        while(ack_num != size - 1) {
            usleep((random() % 1000) * 1000 * 1);
            MPI_Iprobe(MPI_ANY_SOURCE, MEDIUM_ACK, MPI_COMM_WORLD, &flag, &status);
            if(flag) {
                MPI_Get_count(&status, MPI_PACKET_T, &msg_num);
                if(msg_num > 0) {
                    receive_packet(response_packet, MEDIUM_ACK, &status);

                    int senderSHOP_REQUEST MEDIUM_ACK from %d", sender);

                    // int wanted_r_id = response_packet->resource_id;
                    // if(wanted_r_id == -1) {
                    //     debug("%d and I don't have conflict", sender);
                    // } else {
                    //     debug("%d and I are in conflict", sender);
                    //     add_to_medium_queue(create_process_s(sender, response_packet->ts, wanted_r_id), wanted_r_id);
                    //     debug("added %d to medium_queue[%d]", sender, chosen_medium);
                    // }
                }
            }
            
            MPI_Iprobe(MPI_ANY_SOURCE, MEDIUM_REQUEST, MPI_COMM_WORLD, &flag, &status);
            if(flag) {
                MPI_Get_count(&status, MPI_PACKET_T, &msg_num);
                if(msg_num > 0) {
                    receive_packet(response_packet, MEDIUM_REQUEST, &status);
                    int sender = response_packet->id;
                    int wanted_r_id = response_packet->resource_id;

                    debug("got MEDIUM_REQUEST from %d, it wants medium[%d]", sender, wanted_r_id);

                    add_to_medium_queue(create_process_s(sender, response_packet->ts, wanted_r_id), wanted_r_id);
                    // debug("my queue is:");
                    // queue_print(medium_queue_table[chosen_medium]);
                    // TODO add medium usage
                    debug("added %d to medium_queue[%d]", sender, wanted_r_id);
                    debug("sending MEDIUM_ACK to %d", sender);
                    send_packet(chosen_medium, sender, MEDIUM_ACK);
                }
            }
        }

        ack_num = 0;

        while(queue_get_position(medium_queue_table[chosen_medium], rank) != 0) {
            println("i'm not first, waiting for medium to become free");
            receive_packet(response_packet, MEDIUM_RELEASE, &status);
            int sender = response_packet->id;
            int wanted_r_id = response_packet->resource_id;
            // debug("my queue is:");
            // queue_print(medium_queue_table[wanted_r_id]);
            queue_remove(&(medium_queue_table[wanted_r_id]), sender);
            debug("%d released medium[%d]", sender, wanted_r_id);
        }
        debug("medium[%d] is free", chosen_medium);

        // TODO add medium usage

        println("*** in critical section[%d] ***", chosen_medium);
        sleep(5);
        // println("*** out of critical section ***");
        // TODO move ^ to other place
        queue_remove(&(medium_queue_table[chosen_medium]), rank);

        for(int i = 0; i < size; i++) {
            if(i != rank) {
                send_packet(chosen_medium, i, MEDIUM_RELEASE);
            }
        } // TODO move this block to other place
        debug("sent MEDIUM_RELEASE to eveyone");


        println("switching state to SHOP_PREPARE");
        change_state(SHOP_PREPARE);

        msg_ts = ts;

        for(int i = 0; i < size; i++) {
            if(rank != i) {
                send_packet_ts(0, msg_ts, i, SHOP_REQUEST);
            }
        }
        debug("sent SHOP_REQUEST to everyone");

        queue_add(&shop_queue, create_process_s(rank, msg_ts, 0));
        debug("added myself to shop_queue");

        change_state()
    }
}

int choose_medium_index() {
    int max_T = -1;
    int max_T_index = -1;
    int i = rand() % M;
    for(; i < M; i++) {
        if(medium_usage_table[i] > max_T) {
            max_T = medium_usage_table[i];
            max_T_index = i;
        }
    }
    return max_T_index;
}

void add_to_medium_queue(process_s* p, int i) {
    pthread_mutex_lock(&medium_queue_mutex);
    queue_add(&(medium_queue_table[i]), p);
    pthread_mutex_unlock(&medium_queue_mutex);
}

int main(int argc, char** argv) {
    init(&argc, &argv);
    main_loop();
}