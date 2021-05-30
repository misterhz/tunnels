#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "main.h"

state_t state;
int size, rank; /* nie trzeba zerować, bo zmienna globalna statyczna */
pthread_t comm_thread;

pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ts_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t shop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t in_tunnel_mutex = PTHREAD_MUTEX_INITIALIZER; // maybe change to array
pthread_mutex_t medium_usage_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t msg_ack_num_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t main_loop_cond_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t recharge_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t medium_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t shop_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t tunnel_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t medium_mutex_table[M] = {PTHREAD_MUTEX_INITIALIZER};
pthread_mutex_t tunnel_mutex_table[M] = {PTHREAD_MUTEX_INITIALIZER};

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
    in_tunnel_queue_table = malloc(M * sizeof(process_queue_node*));
    for(int i = 0; i < M; i++) {
        in_tunnel_queue_table[i] = NULL;
    }

    medium_usage_table = malloc(M * sizeof(int));
    for(int i = 0; i < M; i++) {
        medium_usage_table[i] = T;
    }
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

void inc_ack_num() {
    pthread_mutex_lock(&msg_ack_num_mutex);
    ++ack_num;
    pthread_mutex_unlock(&msg_ack_num_mutex);
}

int get_ack_num() {
    pthread_mutex_lock(&msg_ack_num_mutex);
    int ret = ack_num;
    pthread_mutex_unlock(&msg_ack_num_mutex);
    return ret;
}

void zero_ack_num() {
    pthread_mutex_lock(&msg_ack_num_mutex);
    ack_num = 0;
    pthread_mutex_unlock(&msg_ack_num_mutex);
}

void main_loop() {

    while (1) {
        // INIT

        change_state(INIT);

        chosen_medium = choose_medium_index();
        int msg_ts = ts; // all processes have to have request with same ts

        println("wanna medium[%d]", chosen_medium);

        add_to_medium_queue(create_process_s(rank, msg_ts, chosen_medium), chosen_medium);
        debug("added myself to medium_queue[%d] with ts %d", chosen_medium, msg_ts);
        add_to_tunnel_queue(create_process_s(rank, msg_ts, chosen_medium), chosen_medium);
        debug("added myself to tunnel_queue[%d] with ts %d", chosen_medium, msg_ts);

        for(int i = 0; i < size; i++) {
            if(rank != i) {
                send_packet_ts(chosen_medium, msg_ts, i, MEDIUM_REQUEST);
            }
        }
        debug("sent MEDIUM_REQUEST[%d] to everyone", chosen_medium);

        // WAITING_FOR_MEDIUM

        change_state(WAITING_FOR_MEDIUM);
        println("switched state to WAITING_FOR_MEDIUM");

        pthread_mutex_lock(&main_loop_cond_mutex); // used to be msg...  mutex
        while(get_ack_num() < size - 1) {
            pthread_cond_wait(&medium_cond, &main_loop_cond_mutex);
        }
        pthread_mutex_unlock(&main_loop_cond_mutex);
        zero_ack_num();
        
        debug("got all MEDIUM_ACKs[%d]", chosen_medium);
        
        int position = get_index_in_medium_queue(rank, chosen_medium);
        if(position == 0) {
            debug("first to get medium[%d]", chosen_medium);
        } else {
            debug("have to wait to get medium[%d]", chosen_medium);
        }

        pthread_mutex_lock(&main_loop_cond_mutex); // used to be medium_mutex_table[chosen_medium]
        while(get_index_in_medium_queue(rank, chosen_medium) != 0) {
            pthread_cond_wait(&medium_cond, &main_loop_cond_mutex);
        }
        pthread_mutex_unlock(&main_loop_cond_mutex);

        int usage = get_medium_usage(chosen_medium);
        if(usage > 0) {
            debug("medium[%d] has enough usages left", chosen_medium);
        } else {
            debug("medium[%d] has to be recharged", chosen_medium);
        }

        pthread_mutex_lock(&main_loop_cond_mutex); // used to be medium_mutex_table[chosen_medium]
        while((usage = get_medium_usage(chosen_medium)) < 1) {
            pthread_cond_wait(&medium_cond, &main_loop_cond_mutex);
        }
        pthread_mutex_unlock(&main_loop_cond_mutex);

        increase_medium_usage(chosen_medium, -1);

        println("*** acquired medium[%d] ***", chosen_medium);

        // SHOP_PREPARE

        change_state(SHOP_PREPARE);
        println("switched state to SHOP_PREPARE");

        msg_ts = ts;
        
        println("wanna enter shop");
        add_to_shop_queue(create_process_s(rank, msg_ts, 1));
        debug("added myself to shop_queue with ts %d", msg_ts);

        for(int i = 0; i < size; i++) {
            if(rank != i) {
                send_packet_ts(1, msg_ts, i, SHOP_REQUEST);
            }
        }
        debug("sent SHOP_REQUEST to everyone");

        // WAITING_FOR_SHOP

        change_state(WAITING_FOR_SHOP);
        println("switched state to WAITING_FOR_SHOP");

        pthread_mutex_lock(&main_loop_cond_mutex); // used to be msg...  mutex
        while(get_ack_num() < size - 1) {
            pthread_cond_wait(&shop_cond, &main_loop_cond_mutex);
        }
        pthread_mutex_unlock(&main_loop_cond_mutex);
        zero_ack_num();

        debug("got all SHOP_ACKs");

        position = get_index_in_shop_queue(rank);
        if(position < F) {
            debug("shop has enough space");
        } else {
            debug("have to wait to enter shop");
        }

        pthread_mutex_lock(&main_loop_cond_mutex); // used to be medium_mutex_table[chosen_medium]
        while(get_index_in_shop_queue(rank) >= F) {
            pthread_cond_wait(&shop_cond, &main_loop_cond_mutex);
        }
        pthread_mutex_unlock(&main_loop_cond_mutex);

        println("*** entered shop ***");

        // IN_SHOP

        change_state(IN_SHOP);
        println("switched state to IN_SHOP");

        sleep(5); // TODO change to random
        println("*** left shop ***");

        remove_from_shop_queue(rank);
        debug("removed myself from shop_queue");

        for(int i = 0; i < size; i++) {
            if(i != rank) {
                send_packet(1, i, SHOP_RELEASE);
            }
        }

        debug("sent SHOP_RELEASE to everyone");
        
        // IN_TUNNEL
        change_state(IN_TUNNEL);
        println("switched state to IN_TUNNEL");

        println("*** released medium[%d] ***", chosen_medium);

        remove_from_medium_queue(rank, chosen_medium);
        debug("removed myself from medium_queue[%d]", chosen_medium);

        if(usage == 1) { // 1 was before me, so it is 0 after me and it has to be recharged
            pthread_cond_signal(&recharge_cond);
        }

        for(int i = 0; i < size; i++) {
            if(i != rank) {
                send_packet(chosen_medium, i, MEDIUM_RELEASE);
            }
        }
        debug("sent MEDIUM_RELEASE[%d] to everyone", chosen_medium);

        println("going through tunnel");
        sleep(5); // TODO change to random

        println("wanna exit tunnel");

        // WANNA_EXIT_TUNNEL
        change_state(WANNA_EXIT_TUNNEL);
        println("switched state to WANNA_EXIT_TUNNEL");

        pthread_mutex_lock(&main_loop_cond_mutex);
        while(get_index_in_tunnel_queue(rank, chosen_medium) > 0) {
            pthread_cond_wait(&tunnel_cond, &main_loop_cond_mutex);
        }
        pthread_mutex_unlock(&main_loop_cond_mutex);

        debug("tunnel[%d] exit is free", chosen_medium);

        remove_from_tunnel_queue(rank, chosen_medium);
        debug("removed myself from tunnel_queue[%d]", chosen_medium);

        for(int i = 0; i < size; i++) {
            if(i != rank) {
                send_packet(chosen_medium, i, LEFT_TUNNEL);
            }
        }
        debug("sent LEFT_TUNNEL[%d] to everyone", chosen_medium);

        println("exited tunnel[%d]", chosen_medium);

        // CHILL
        change_state(CHILL);
        println("switched state to CHILL");

        sleep(5); // TODO change to random
        
        println("gonna return");
    }
}

void* start_comm_thread(void* ptr) {
    MPI_Status status;
    process_s* response_packet = malloc(sizeof(response_packet));

    while(1) {
        receive_packet(response_packet, MPI_ANY_TAG, &status);
        int sender = response_packet->id;
        int wanted_r_id = response_packet->resource_id;
        switch(status.MPI_TAG) {
            case MEDIUM_REQUEST:
                debug("received MEDIUM_REQUEST[%d] from %d", wanted_r_id, sender);
                add_to_medium_queue(copy_process_s(response_packet), wanted_r_id);
                debug("added %d to medium_queue[%d]", sender, wanted_r_id);
                add_to_tunnel_queue(copy_process_s(response_packet), wanted_r_id);
                queue_print(in_tunnel_queue_table[wanted_r_id]);
                debug("added %d to tunnel_queue[%d]", sender, wanted_r_id);
                send_packet(wanted_r_id, sender, MEDIUM_ACK);
                debug("sent MEDIUM_ACK[%d] to %d", wanted_r_id, sender);
                break;

            case MEDIUM_ACK:
                debug("received MEDIUM_ACK[%d] from %d", wanted_r_id, sender);
                pthread_mutex_lock(&msg_ack_num_mutex);
                ++ack_num;
                pthread_mutex_unlock(&msg_ack_num_mutex);
                pthread_cond_signal(&medium_cond);
                break;
            
            case MEDIUM_RELEASE:
                debug("received MEDIUM_RELEASE[%d] from %d", wanted_r_id, sender);
                increase_medium_usage(wanted_r_id, -1); // medium usage is decreased by 1
                remove_from_medium_queue(sender, wanted_r_id);
                pthread_cond_signal(&medium_cond);
                debug("removed %d from medium_queue[%d]", sender, wanted_r_id);
                break;

            case MEDIUM_RESET:
                debug("received MEDIUM_RESET[%d]", wanted_r_id);
                increase_medium_usage(wanted_r_id, T);
                debug("medium[%d] has %d usages now", wanted_r_id, get_medium_usage(wanted_r_id));
                pthread_cond_signal(&medium_cond);
                break;

            case SHOP_REQUEST:
                debug("received SHOP_REQUEST from %d", sender);
                add_to_shop_queue(copy_process_s(response_packet));
                debug("added %d to shop_queue", sender);
                send_packet(1, sender, SHOP_ACK);
                debug("sent SHOP_ACK to %d", sender);
                break;

            case SHOP_ACK:
                debug("received SHOP_ACK from %d", sender);
                pthread_mutex_lock(&msg_ack_num_mutex);
                ++ack_num;
                pthread_mutex_unlock(&msg_ack_num_mutex);
                pthread_cond_signal(&shop_cond);
                break;
            
            case SHOP_RELEASE:
                debug("received SHOP_RELEASE from %d", sender);
                remove_from_shop_queue(sender);
                pthread_cond_signal(&shop_cond);
                debug("removed %d from shop_queue", sender);
                break;

            case LEFT_TUNNEL:
                debug("received LEFT_TUNNEL[%d] from %d", wanted_r_id, sender);
                remove_from_tunnel_queue(sender, wanted_r_id);
                pthread_cond_signal(&tunnel_cond);
                debug("removed %d from in_tunnel_queue[%d]", sender, wanted_r_id);
                break;
        }
    }
}

void* send_reset(void* ptr) {
    int* r_id_p = (int*) ptr;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    while(1) {
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&recharge_cond, &lock);
        int r_id = *r_id_p;

        debug("medium[%d] is being recharged", r_id);
        sleep(5); // TODO change to random
        for(int i = 0; i < size; i++) {
            if(rank != i) {
                send_packet(r_id, i, MEDIUM_RESET);
            }
        }
        debug("sent MEDIUM_RESET[%d] to everyone", chosen_medium);
        int res = increase_medium_usage(r_id, T);
        debug("recharged medium[%d] locally, it has %d usages now", r_id, get_medium_usage(r_id));

        pthread_mutex_unlock(&lock);
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

// medium queue functions
void add_to_medium_queue(process_s* p, int i) {
    pthread_mutex_lock(&medium_mutex_table[i]);
    queue_add(&(medium_queue_table[i]), p);
    increase_timestamp(1);
    pthread_mutex_unlock(&medium_mutex_table[i]);
}

void remove_from_medium_queue(int id, int medium_id) {
    pthread_mutex_lock(&medium_mutex_table[medium_id]);
    queue_remove(&(medium_queue_table[medium_id]), id);
    increase_timestamp(1);
    pthread_mutex_unlock(&medium_mutex_table[medium_id]);
}

int get_index_in_medium_queue(int id, int m_id) {
    pthread_mutex_lock(&medium_mutex_table[m_id]);
    int ret = queue_get_position(medium_queue_table[m_id], id);
    pthread_mutex_unlock(&medium_mutex_table[m_id]);
    return ret;
}

// shop queue functions
void add_to_shop_queue(process_s* p) {
    pthread_mutex_lock(&shop_mutex);
    queue_add(&shop_queue, p);
    increase_timestamp(1);
    pthread_mutex_unlock(&shop_mutex);
}

void remove_from_shop_queue(int id) {
    pthread_mutex_lock(&shop_mutex);
    queue_remove(&shop_queue, id);
    increase_timestamp(1);
    pthread_mutex_unlock(&shop_mutex);
}

int get_index_in_shop_queue(int id) {
    pthread_mutex_lock(&shop_mutex);
    int ret = queue_get_position(shop_queue, id);
    pthread_mutex_unlock(&shop_mutex);
    return ret;
}

// tunnel queue functions
void add_to_tunnel_queue(process_s* p, int i) {
    pthread_mutex_lock(&tunnel_mutex_table[i]);
    queue_add(&(in_tunnel_queue_table[i]), p);
    increase_timestamp(1);
    pthread_mutex_unlock(&tunnel_mutex_table[i]);
}

void remove_from_tunnel_queue(int id, int medium_id) {
    pthread_mutex_lock(&tunnel_mutex_table[medium_id]);
    queue_remove(&(in_tunnel_queue_table[medium_id]), id);
    increase_timestamp(1);
    pthread_mutex_unlock(&tunnel_mutex_table[medium_id]);
    pthread_cond_signal(&tunnel_cond);
}

int get_index_in_tunnel_queue(int id, int m_id) {
    pthread_mutex_lock(&tunnel_mutex_table[m_id]);
    int ret = queue_get_position(in_tunnel_queue_table[m_id], id);
    pthread_mutex_unlock(&tunnel_mutex_table[m_id]);
    return ret;
}

// int get_index_in_medium_queue(int id, int m_id) {
//     pthread_mutex_lock(&medium_mutex_table[m_id]);
//     int ret = queue_get_position(medium_queue_table[m_id], id);
//     pthread_mutex_unlock(&medium_mutex_table[m_id]);
//     return ret;
// }


// medium usage functions
int increase_medium_usage(int r_id, int num) {
    pthread_mutex_lock(&medium_mutex_table[r_id]);
    medium_usage_table[r_id] += num;
    int ret = medium_usage_table[r_id];
    pthread_mutex_unlock(&medium_mutex_table[r_id]);
    return ret;
}

int get_medium_usage(int r_id) {
    pthread_mutex_lock(&medium_mutex_table[r_id]);
    int ret = medium_usage_table[r_id];
    pthread_mutex_unlock(&medium_mutex_table[r_id]);
    return ret;
}

int main(int argc, char** argv) {
    init(&argc, &argv);
    
    pthread_create(&comm_thread, NULL, start_comm_thread, 0);
    pthread_create(&recharge_thread, NULL, send_reset, (void*) &chosen_medium);
    main_loop();
}