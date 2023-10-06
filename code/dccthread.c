#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include "dccthread.h"
#include "dlist.h"
#include <signal.h>
#include <string.h>


typedef struct dccthread{
    char name[DCCTHREAD_MAX_NAME_SIZE];
    ucontext_t context;
    int id;
    int blocking;
    int idWaited;
    dccthread_t* waiting;
} dccthread_t;

typedef struct{
    struct sigaction sa;
    struct sigevent event;
    timer_t timer_id;
    struct itimerspec its;
} timerControl;

typedef struct {
    ucontext_t manager;
    struct dlist* ready_queue;
    struct dlist* wait_queue;
    struct dlist* sleep_queue;
    dccthread_t* current_thread;
    dccthread_t* sleep_thread;
    int globalID;
    int wait;
    timerControl timer;
    timerControl sleeper;
    sigset_t mask;
} managerThreads;

// #define block() sigprocmask( SIG_BLOCK, &central.mask, NULL)
// #define unblock() sigprocmask( SIG_UNBLOCK, &central.mask, NULL)
// #define block() {central.control.signal = 0;}
// #define unblock() {central.control.signal = 1;}
#define SIGNAL_BLOCK SIGUSR1
#define SIGNAL_SLEEP SIGALRM

managerThreads central;

void __expired(int signo){
        if(signo == SIGNAL_BLOCK){
            getcontext(&central.current_thread->context);
            dlist_push_right(central.ready_queue,central.current_thread);
            swapcontext(&central.current_thread->context,&central.manager);
            // setcontext(&central.current_thread->context);
        }
}

void __sleep(){
        // if(signo == SIGNAL_SLEEP){
            getcontext(&central.current_thread->context);
            dccthread_t* sleep_thread = dlist_pop_left(central.sleep_queue);
            dlist_push_right(central.ready_queue,sleep_thread);
            timer_delete(central.sleeper.timer_id);
            swapcontext(&central.current_thread->context,&central.manager);
            // setcontext(&central.current_thread->context);
        // }
}

void create_timer(int ms){
    // central.timer.event = { 0 };

    central.timer.timer_id = 0;
    central.timer.sa.sa_flags = SA_SIGINFO;
    central.timer.sa.sa_sigaction = (void*)__expired;
    sigemptyset(&central.timer.sa.sa_mask);
    sigaction(SIGNAL_BLOCK, &central.timer.sa, NULL);

    central.timer.event.sigev_notify = SIGEV_SIGNAL;
    // event.sigev_value.sival_ptr = &timer_id;
    central.timer.event.sigev_signo = SIGNAL_BLOCK;

    timer_create(CLOCK_PROCESS_CPUTIME_ID,&central.timer.event,&central.timer.timer_id);


    central.timer.its.it_value.tv_sec = 0;
    central.timer.its.it_value.tv_nsec = ms * 1000000;
    central.timer.its.it_interval.tv_sec = 0;
    central.timer.its.it_interval.tv_nsec = ms * 1000000;
    timer_settime(central.timer.timer_id,0,&central.timer.its,NULL);

    // sigemptyset(&central.old_timer);
}

void create_sleep(struct timespec ts){
    
    struct itimerspec timer_spec;
    struct sigevent sev;
    timer_t timer_id;
    // Crie o temporizador
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = (void*)__sleep;
    sev.sigev_value.sival_ptr = &timer_id;
    if (timer_create(CLOCK_REALTIME, &sev, &timer_id) == -1) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    // Configurar a especificação do temporizador
    timer_spec.it_value = ts;
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;

    // Ative o temporizador
    if (timer_settime(timer_id, 0, &timer_spec, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    central.sleeper.timer_id = timer_id;
    // printf("Sleeper criado \n");
   
}

void block(){
    if(!central.current_thread->blocking){
        central.current_thread->blocking = 1;
        timer_delete(central.timer.timer_id);
    }
}

void unblock(){
    central.current_thread->blocking = 0;
    create_timer(10);
}

int dlist_find(struct dlist* queue, dccthread_t* thread){
    if(dlist_empty(queue)){
        return 0;
    }
    struct dnode *curr = queue->head;
    while(curr != NULL && (dccthread_t*)curr->data != thread){
        curr = curr->next;
    }
    if(curr != NULL){
        return 1;
    }
    else{
        return 0;
    }
}

void managerCentral(){

    dccthread_t* thread = dlist_pop_left(central.ready_queue);
    central.current_thread = thread;
    setcontext(&thread->context);

}

void dccthread_init(void (*func)(int),int param){

    //Create a Manager Thread
    getcontext(&central.manager);

    // sigemptyset(&central.mask);
    // sigaddset(&central.mask,SIGNAL_BLOCK);
    // sigaddset(&central.mask,SIGNAL_SLEEP);
    // sigprocmask(SIG_SETMASK,&central.mask,NULL);

    // central.manager.uc_sigmask = central.mask;

    central.manager.uc_stack.ss_size = THREAD_STACK_SIZE;
    central.manager.uc_stack.ss_flags = 0;
    if((central.manager.uc_stack.ss_sp = malloc(central.manager.uc_stack.ss_size)) == NULL){
        perror("Failing allocating space to manager thread");
        exit(1);
    }
    makecontext(&central.manager,managerCentral,0);
    
    //Create a main Thread
    dccthread_t* firstThread = (dccthread_t*)malloc(sizeof(dccthread_t));
    getcontext(&(firstThread->context));
    firstThread->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    firstThread->context.uc_stack.ss_flags = 0;
    if((firstThread->context.uc_stack.ss_sp = malloc(firstThread->context.uc_stack.ss_size)) == NULL){
        perror("Failing allocating space to main thread");
        exit(1);
    }
    // main->name = "main";
    strcpy(firstThread->name,"main");
    firstThread->id = 0;
    firstThread->idWaited = -1;
    firstThread->context.uc_link = &central.manager;
    makecontext(&(firstThread->context),(void(*)(void))func,1,param);

    central.ready_queue = dlist_create();
    central.wait_queue = dlist_create();
    central.sleep_queue = dlist_create();
    dlist_push_right(central.ready_queue,firstThread);

    central.globalID = 0;
    central.wait = 0;
    // central.sleep_thread->id = 0;

    create_timer(10);

    // managerCentral();
    setcontext(&central.manager);

    exit(EXIT_SUCCESS);

}

dccthread_t* dccthread_create(const char* name, void (*func)(int),int param){
    block();
    dccthread_t* newThread = (dccthread_t*)malloc(sizeof(dccthread_t));
    // newThread->name = name;
    strcpy(newThread->name,name);

    newThread->id = central.globalID++;
    newThread->idWaited = -1;

    getcontext(&(newThread->context));
    newThread->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    newThread->context.uc_stack.ss_flags = 0;
    if((newThread->context.uc_stack.ss_sp = malloc(newThread->context.uc_stack.ss_size)) == NULL){
        perror("Failing allocationg space to new thread");
        return NULL;
    }
    newThread->context.uc_link = &central.manager;
    makecontext(&(newThread->context),(void(*)(void))func,1,param);
    dlist_push_right(central.ready_queue,newThread);

    unblock();
    return newThread;

}

void dccthread_yield(){

    block();
    getcontext(&central.current_thread->context);
    dlist_push_right(central.ready_queue,central.current_thread);
    swapcontext(&central.current_thread->context,&central.manager);
    unblock();
}

dccthread_t * dccthread_self(){
    return central.current_thread;
}

const char * dccthread_name(dccthread_t *tid){
    return tid->name;
}

void dccthread_wait(dccthread_t* tid){
    
    block();
    // printf("thread %s \n",tid->name);
    // printf("sleep %s \n",central.sleep_thread->name);
    if(dlist_find(central.ready_queue,tid) || dlist_find(central.sleep_queue,tid)){
        getcontext(&central.current_thread->context);
        tid->idWaited = central.current_thread->id;
        // printf("%d waited \n",tid->idWaited);
        dlist_push_right(central.wait_queue,central.current_thread);
        dlist_push_right(central.ready_queue,tid);
        // printf("size %d \n",central.ready_queue->count);
        swapcontext(&central.current_thread->context,&central.manager);
    }

    unblock();
}

void dccthread_exit(){
    block();
    // printf("current: %s ready size %d \n", central.current_thread->name,central.ready_queue->count);
    if(central.current_thread->idWaited != -1){
        dccthread_t* thread = dlist_pop_left(central.wait_queue);
        dlist_push_right(central.ready_queue,thread);
    }
    swapcontext(&central.current_thread->context,&central.manager);
    // setcontext(&central.manager);
    unblock();
}

void dccthread_sleep(struct timespec ts){
    
    block();
 
    getcontext(&central.current_thread->context);
    dlist_push_right(central.sleep_queue,central.current_thread);
    create_sleep(ts);
    
    // setcontext(&central.manager);
    swapcontext(&central.current_thread->context,&central.manager);

    unblock();
}