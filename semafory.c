//
//  semafory.c
//  
//
//  Created by Mateusz Sledz on 06/04/2019.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <time.h>

#define BUFFKEY   2544
#define EMPTYKEY  2545
#define FULLKEY   2546
#define MUTSYSKEY 2548

static struct sembuf buf;

struct Queue {
    int head, tail, length, buffer_size;
    int buf[];
};

struct Queue * createQueue(struct Queue *q, int buffer_size)
{
    q = malloc(sizeof(*q) + sizeof(int)*(buffer_size+4));
    q ->tail = 0;
    q ->head = 0;
    q ->length = 0;
    q ->buffer_size = buffer_size;
    return q;
}

void putToBuf(struct Queue *q, int val) {
    q->buf[q->tail] = val;
    q->tail = (q->tail+1)%q->buffer_size;
    q->length++;
    printf("Wsadzam %d, size: %d\n", val, q->length);
}

void getFromBuf(struct Queue *q){
    int ret = q->buf[q->head];
    q->buf[q->head] = 0;
    q->head = (q->head+1)%q->buffer_size;
    q->length--;
    printf("Wyjmuje %d\n", ret);
}


void upS(int semid){
    buf.sem_num = 0;
    buf.sem_op = 1;
    buf.sem_flg = 0;
    if (semop(semid, &buf, 1) == (void*)-1){
        perror("Podnoszenie semafora upS");
         exit(1);
    }
    
}
void downS(int semid){
    buf.sem_num = 0;
    buf.sem_op = -1;
    buf.sem_flg = 0;
    if (semop(semid, &buf, 1) == (void*)-1){
        perror("Opuszczenie semafora downS");
        exit(1);
    }
}

int produce_item()
{
    int i;
    i = rand()%1000;
    return i;
}



void Producent(int nr, int buff_size,int liczba_towaru){
    srand(time(NULL)+nr*123);
    int buffid = shmget(BUFFKEY, sizeof(int)*(buff_size+4), 0600);
    if(buffid == (void*)-1) perror("shget w producer: ");
    
    struct Queue *buffer = (struct Queue*)shmat(buffid, NULL, 0);
    
    if(buffer == (void*)-1) perror("shmat w producent: ");
    int mutex = semget(MUTSYSKEY, 1, 0600);
    
    if(mutex == (void*)-1) perror("semget mutex prod: ");
    int emptyid = semget(EMPTYKEY, 1, 0600);
    
    if(emptyid == (void*)-1) perror("semget empty prod: ");
    int fullid = semget(FULLKEY, 1, 0600);
    
    int k=0;
    int item;
    
    while(k < liczba_towaru){
        sleep(rand()%4);
        downS( emptyid );
        downS( mutex );
            item = produce_item();
            printf("*****Producent %d\n", nr);
            putToBuf( buffer, item);
        upS( mutex );
        upS( fullid );
        k++;
    }
}

void Consumer(int liczba_prod ,int buff_size, int liczba_towaru){
    int buffid = shmget(BUFFKEY, sizeof(int)*(buff_size+4), 0600);
    if(buffid == (void*)-1) perror("shget w consumer: ");
    struct Queue *buffer = (struct Queue*)shmat(buffid, NULL, 0);
    int mutex = semget(MUTSYSKEY, 1, 0600);
    int emptyid = semget(EMPTYKEY, 1, 0600);
    int fullid = semget(FULLKEY, 1, 0600);
    int k = 0;
    while(k < liczba_prod*liczba_towaru){
        sleep(rand()%6);
        downS( fullid);
        downS( mutex );
            printf("=====Consumer \n");
            getFromBuf(buffer);
        upS( mutex );
        upS( emptyid );
        k++;
    }
}

int main(int argc, char * argv[]) {

    int liczba_prod = strtol(argv[1], NULL, 10);
    int buff_size = strtol(argv[2], NULL, 10);
    int liczba_towaru = strtol(argv[3], NULL, 10);
    
    int buffid = shmget(BUFFKEY, (sizeof(int)*(buff_size+4)), IPC_CREAT|0600); //utworzenie segmentu pamieci wspoldzielonej
    if(buffid == (void*)-1) perror("1shmget: ");
    
    struct Queue *buffer = createQueue(buffer, buff_size);
    
    buffer = (struct Queue*)shmat(buffid, NULL, 0);
    
    if(buffer == (void*)-1) perror("2shmat: ");
  
    int mutex = semget(MUTSYSKEY, 1, IPC_CREAT|IPC_EXCL|0600); //mutex - blokuje dostep do kolejki


    if(mutex==-1){
        mutex = semget(MUTSYSKEY, 1, 0600);
        if(mutex==-1) perror("BLAD SEMID");
    }
        semctl(mutex, 0, SETVAL, (int)1);
    
    int emptyid = semget(EMPTYKEY, 1, IPC_CREAT|IPC_EXCL|0600); //ile jest elemtnow empty w danej kolejce
    if(emptyid==-1){
        emptyid = semget(EMPTYKEY, 1, 0600);
        if(emptyid==-1) perror("BLAD EMPTYID");
    }
    
    
    semctl(emptyid, 0, SETVAL, (int)buff_size);
    
    int fullid = semget(FULLKEY, 1, IPC_CREAT|IPC_EXCL|0600);
    if(fullid==-1){
        fullid = semget(FULLKEY, 1, 0600);
        if(fullid==-1) perror("BLAD FULLID");
    }
    
    semctl(fullid, 0, SETVAL, (int)0);
    
    buffer->length=0;
    buffer->head=0;
    buffer->tail=0;
    buffer->buffer_size = buff_size;
    
    pid_t pid[liczba_prod+1];
    
    pid[liczba_prod] = fork();
    if(pid[liczba_prod]==0)
    {
        Consumer(liczba_prod, buff_size, liczba_towaru);
        printf("konsument RIP\n");
        return 0;
    }

    for(int i=0; i<liczba_prod; i++){
        pid[i]=fork();
        if(pid[i]==0){ Producent(i, buff_size, liczba_towaru);
            printf("               I'm done, goodbye ***Prod: %d\n", i);
        }
        else break;

    }
    
    sleep(100);
    shmdt(buffer);
    return 0;
}


# Semafory-C-
