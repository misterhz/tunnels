#ifndef GLOBALH
#define GLOBALH

#define _GNU_SOURCE
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "queue.h"
/* odkomentować, jeżeli się chce DEBUGI */
#define DEBUG 

#define M 10
#define F 5
#define T 1

#define IN_SHOP_MIN_MS 0
#define IN_SHOP_MAX_MS 10000

#define OPENING_TUNNEL_MIN_MS 0
#define OPENING_TUNNEL_MAX_MS 10000

#define IN_TUNNEL_MIN_MS 0
#define IN_TUNNEL_MAX_MS 10000

#define CHILL_MIN_MS 0
#define CHILL_MAX_MS 10000

#define RECHARGE_MIN_MS 0
#define RECHARGE_MAX_MS 10000

/* stany procesu */
typedef enum { MEDIUM_PREPARE, WAITING_FOR_MEDIUM, SHOP_PREPARE, WAITING_FOR_SHOP, IN_SHOP, IN_TUNNEL, WANNA_EXIT_TUNNEL, CHILL } state_t;
typedef enum { MEDIUM_REQUEST, MEDIUM_ACK, MEDIUM_RELEASE, MEDIUM_RESET, SHOP_REQUEST, SHOP_ACK, SHOP_RELEASE, LEFT_TUNNEL } message_t;

state_t state;
int rank;
int size;
int ts;
int chosen_medium;
int ack_num;

process_queue_node* shop_queue;

process_queue_node** medium_queue_table;
process_queue_node** in_tunnel_queue_table;

int* medium_usage_table;

pthread_t comm_thread;
pthread_t recharge_thread;

void* start_comm_thread(void* ptr);
void* send_reset(void*);

void change_state(state_t);
void send_packet(int resource_id, int destination, message_t tag);
void send_packet_ts(int resource_id, int p_ts, int destination, message_t tag);
void send_packet_to_everyone(int resource_id, int destination, message_t tag);
void increase_timestamp(int);
void set_timestamp(int, int);
int choose_medium_index();

void add_to_medium_queue(process_s* p, int i);
void remove_from_medium_queue(int id, int medium_id);
int get_index_in_medium_queue(int id, int m_id);

void add_to_shop_queue(process_s* p);
void remove_from_shop_queue(int id);
int get_index_in_shop_queue(int id);

void add_to_tunnel_queue(process_s* p, int i);
void remove_from_tunnel_queue(int id, int medium_id);
int get_index_in_tunnel_queue(int id, int m_id);

void main_loop();

void inc_ack_num();
void zero_ack_num();
int get_ack_num();

int increase_medium_usage(int r_id, int num);
int get_medium_usage(int r_id);

int get_random_time_ms(int lb, int ub);

/* Typy wiadomości */

/* macro debug - działa jak printf, kiedy zdefiniowano
   DEBUG, kiedy DEBUG niezdefiniowane działa jak instrukcja pusta 
   
   używa się dokładnie jak printfa, tyle, że dodaje kolorków i automatycznie
   wyświetla rank

   w związku z tym, zmienna "rank" musi istnieć.

   w printfie: definicja znaku specjalnego "%c[%d;%dm [%d]" escape[styl bold/normal;kolor [RANK]
                                           FORMAT:argumenty doklejone z wywołania debug poprzez __VA_ARGS__
					   "%c[%d;%dm"       wyczyszczenie atrybutów    27,0,37
                                            UWAGA:
                                                27 == kod ascii escape. 
                                                Pierwsze %c[%d;%dm ( np 27[1;10m ) definiuje styl i kolor literek
                                                Drugie   %c[%d;%dm czyli 27[0;37m przywraca domyślne kolory i brak pogrubienia (bolda)
                                                ...  w definicji makra oznacza, że ma zmienną liczbę parametrów
                                            
*/
#ifdef DEBUG
#define debug(FORMAT,...) printf("%c[%d;%dm [%d] [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, ts, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#endif

#define P_WHITE printf("%c[%d;%dm",27,1,37);
#define P_BLACK printf("%c[%d;%dm",27,1,30);
#define P_RED printf("%c[%d;%dm",27,1,31);
#define P_GREEN printf("%c[%d;%dm",27,1,33);
#define P_BLUE printf("%c[%d;%dm",27,1,34);
#define P_MAGENTA printf("%c[%d;%dm",27,1,35);
#define P_CYAN printf("%c[%d;%d;%dm",27,1,36);
#define P_SET(X) printf("%c[%d;%dm",27,1,31+(6+X)%7);
#define P_CLR printf("%c[%d;%dm",27,0,37);

/* printf ale z kolorkami i automatycznym wyświetlaniem RANK. Patrz debug wyżej po szczegóły, jak działa ustawianie kolorków */
#define println(FORMAT, ...) printf("%c[%d;%dm [%d] [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, ts, ##__VA_ARGS__, 27,0,37);

#endif