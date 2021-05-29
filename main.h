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

typedef struct proc_queue_n process_queue_node;
typedef struct proc process_s;

/* boolean */
#define TRUE 1
#define FALSE 0

/* używane w wątku głównym, determinuje jak często i na jak długo zmieniają się stany */
#define STATE_CHANGE_PROB 50
#define SEC_IN_STATE 2

#define ROOT 0

#define M 10
#define F 5
#define T 10

/* stany procesu */
typedef enum {INIT, MEDIUM_PREPARE, WAITING_FOR_MEDIUM, WAITING_FOR_RESET, SHOP_PREPARE, WAITING_FOR_SHOP, IN_SHOP, IN_TUNNEL, WANNA_EXIT_TUNNEL, CHILL} state_t;
typedef enum {MEDIUM_REQUEST, MEDIUM_ACK, MEDIUM_RELEASE, MEDIUM_RESET, SHOP_REQUEST, SHOP_ACK, SHOP_RELEASE} message_t;

state_t state;
int rank;
int size;
int medium_i;
int ts;
int free_F;

process_queue_node* shop_queue;

process_queue_node** medium_queue_table;
process_queue_node** in_tunnel_queue;

int* medium_usage_table;

/* ilu już odpowiedziało na GIVEMESTATE */
extern int number_received;

void* start_comm_thread(void* ptr);

void change_state(state_t);
void send_packet(int resource_id, int destination, message_t tag);
void send_packet_ts(int resource_id, int p_ts, int destination, message_t tag);
void send_packet_to_everyone(int resource_id, int destination, message_t tag);
void increase_timestamp(int);
void set_timestamp(int, int);
int choose_medium_index();

void add_to_medium_queue(process_s* p, int i);

void main_loop();

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