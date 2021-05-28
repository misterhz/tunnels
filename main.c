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

    pthread_create( &comm_thread, NULL, start_comm_thread, 0);

    debug("jestem");

    medium_queue_table = malloc(M * sizeof(process_queue_node*));
    // shop_queue doesn't need init
    in_tunnel_queue = malloc(M * sizeof(process_queue_node*));

    medium_usage_table = malloc(M * sizeof(int));
    for(int i = 0; i < M; i++) {
        medium_usage_table[i] = T;
    }

    free_F = F;
    ts = 0;
}

void* start_comm_thread(void *ptr) {
    MPI_Status status;
    int is_message = FALSE;
    process_s packet;
    /* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( state != 50 ) {
	debug("czekam na recv");
    MPI_Recv( &packet, 1, MPI_PACKET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    increase_timestamp(1);

    switch(status.MPI_TAG) {
        case MEDIUM_REQUEST:
            if(state == WAITING_FOR_MEDIUM) {
                add_to_medium_queue(copy_process_s(&packet), packet.resource_id);
            }
    }

        // switch ( status.MPI_TAG ) {
	    // case FINISH: 
        //         changeState(InFinish);
	    // break;
	    // case TALLOWTRANSPORT: &argv
        //         sendPacket(&pakiet, ROOT, STATE);
        //         debug("Wysyłam mój stan do monitora: %d funtów łoju na składzie!", tallow);
	    // break;
        //     case STATE:
        //         numberReceived++;
        //         globalState += pakiet.data;
        //         if (numberReceived > size-1) {
        //             debug("W magazynach mamy %d funtów łoju.", globalState);
        //         } 
        //     break;
	    // case INMONITOR: 
        //         changeState( InMonitor );
        //         debug("Od tej chwili czekam na polecenia od monitora");
	    // break;
	    // case INRUN: 
        //         changeState( InRun );
        //         debug("Od tej chwili decyzję podejmuję autonomicznie i losowo");
	    // break;
	    // default:
	    // break;
        // }
    }
}

void change_state(state_t new_state) {
    pthread_mutex_lock(&state_mutex);
    state = new_state;
    increase_timestamp(1);
    pthread_mutex_unlock(&state_mutex);
}

void send_packet(process_s *pkt, int destination, int tag) {
    MPI_Send( pkt, 1, MPI_PACKET_T, destination, tag, MPI_COMM_WORLD);
    // TODO is send an event?
}

void increase_timestamp(int d) {
    pthread_mutex_lock(&ts_mutex);
    ts += d;
    pthread_mutex_unlock(&ts_mutex);
}

void set_timestamp(int new_ts, int d) {
    pthread_mutex_lock(&ts_mutex);
    ts = (ts > new_ts ? ts : new_ts) + d;
    pthread_mutex_unlock(&ts_mutex);
}

void main_loop()
{
    srandom(rank);
    while (state != 50) {
        // int perc = random()%100; 

        // if (perc<STATE_CHANGE_PROB) {
        //     if (stan==InRun) {
		// debug("Zmieniam stan na wysyłanie");
		// changeState( InSend );
		// packet_t *pkt = malloc(sizeof(packet_t));
		// pkt->data = perc;
        //         changeTallow( -perc);
        //         sleep( SEC_IN_STATE); // to nam zasymuluje, że wiadomość trochę leci w kanale
        //                               // bez tego algorytm formalnie błędny za każdym razem dawałby poprawny wynik
		send_packet( 0, (rank+1) % size, MEDIUM_PREPARE);
		// changeState( InRun );
		// debug("Skończyłem wysyłać");
        //     } else {
        //     }
        // }
        // sleep(SEC_IN_STATE);
    }
}

int get_maximum_free_T_index() {
    int max_T = -1;
    int max_T_index = -1;
    for(int i = 0; i < T; i++) {
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
    printf("test\n");
}
