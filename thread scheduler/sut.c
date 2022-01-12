#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>      
#include <stdbool.h>     
#include <sys/queue.h>
#include <pthread.h>
#include <stddef.h>                                                              
#include "sut.h"
#include "queue.h"

static pthread_t *cexec;
static struct queue ready_queue; 
static pthread_t *iexec;
static struct queue wait_queue;

void *fun_cexec(){
    printf("C-EXEC kernel thread running\n"); //bugfix line
}

void *fun_iexec(){
    printf("I-EXEC kernel thread running\n"); //bugfix line
}

void sut_init(){

    //Create kernel thread C-EXEC
    cexec = (pthread_t*)malloc(sizeof(pthread_t));
    if(pthread_create(cexec, NULL, fun_cexec, NULL) != 0)
    printf("Failed to create C-EXEC"); //bugfix line

    //Create ready queue
    ready_queue = queue_create();
    queue_init(&ready_queue);

    //Create kernel thread I-EXEC
    iexec = (pthread_t*)malloc(sizeof(pthread_t));
    if(pthread_create(iexec, NULL, fun_iexec, NULL) != 0)
    printf("Failed to create I-EXEC"); //bugfix line

    //Create wait queue
    wait_queue = queue_create();
    queue_init(&wait_queue);

}

bool sut_create(sut_task_f fn);
void sut_yield();
void sut_exit();
int sut_open(char *dest);
void sut_write(int fd, char *buf, int size);
void sut_close(int fd);
char *sut_read(int fd, char *buf, int size);

void sut_shutdown(){
    pthread_join(*cexec, NULL);
    pthread_join(*iexec, NULL);
    printf("C-EXEC and I-EXEC joined\n"); //bugfix line
}