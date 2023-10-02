#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include "dccthread.h"
#include "dlist.h"
#include <signal.h>


typedef struct dccthread{
    const char* name;
    ucontext_t context;
    int id;
    int blocking;
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
    dccthread_t* current_thread;
    int globalID;
    timerControl timer;
} managerThreads;

// #define block() sigprocmask( SIG_BLOCK, &central.block_timer, NULL)
// #define unblock() sigprocmask( SIG_UNBLOCK, &central.block_timer, NULL)
// #define block() {central.control.signal = 0;}
// #define unblock() {central.control.signal = 1;}
#define SIGNAL SIGALRM

managerThreads central;

void expired(int signo){
        if(signo == SIGNAL){
            getcontext(&central.current_thread->context);
            dlist_push_right(central.ready_queue,central.current_thread);
            swapcontext(&central.current_thread->context,&central.manager);
        }
}

void create_timer(int ms){
    // central.timer.event = { 0 };

    central.timer.timer_id = 0;
    central.timer.sa.sa_flags = SA_SIGINFO;
    central.timer.sa.sa_sigaction = (void*)expired;
    sigemptyset(&central.timer.sa.sa_mask);
    sigaction(SIGNAL, &central.timer.sa, NULL);

    central.timer.event.sigev_notify = SIGEV_SIGNAL;
    // event.sigev_value.sival_ptr = &timer_id;
    central.timer.event.sigev_signo = SIGNAL;

    timer_create(CLOCK_PROCESS_CPUTIME_ID,&central.timer.event,&central.timer.timer_id);


    central.timer.its.it_value.tv_sec = 0;
    central.timer.its.it_value.tv_nsec = ms * 1000000;
    central.timer.its.it_interval.tv_sec = 0;
    central.timer.its.it_interval.tv_nsec = ms * 1000000;
    timer_settime(central.timer.timer_id,0,&central.timer.its,NULL);

    // sigemptyset(&central.old_timer);
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

void managerCentral(){

    // block();
    dccthread_t* thread = dlist_pop_left(central.ready_queue);
    central.current_thread = thread;
    setcontext(&thread->context);
    // unblock();
}

void dccthread_init(void (*func)(int),int param){
    
    //Create a Manager Thread
    getcontext(&central.manager);
    central.manager.uc_stack.ss_size = THREAD_STACK_SIZE;
    central.manager.uc_stack.ss_flags = 0;
    if((central.manager.uc_stack.ss_sp = malloc(central.manager.uc_stack.ss_size)) == NULL){
        perror("Failing allocating space to manager thread");
        exit(1);
    }
    makecontext(&central.manager,managerCentral,0);
    
    //Create a main Thread
    dccthread_t* main = (dccthread_t*)malloc(sizeof(dccthread_t));
    getcontext(&(main->context));
    main->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    main->context.uc_stack.ss_flags = 0;
    if((main->context.uc_stack.ss_sp = malloc(main->context.uc_stack.ss_size)) == NULL){
        perror("Failing allocating space to main thread");
        exit(1);
    }
    main->name = "main";
    main->id = 0;
    main->context.uc_link = &central.manager;
    makecontext(&(main->context),(void(*)(void))func,1,param);

    central.ready_queue = dlist_create();
    dlist_push_right(central.ready_queue,main);

    central.globalID = 0;

    create_timer(10);

    setcontext(&central.manager);

    exit(EXIT_SUCCESS);

}

dccthread_t* dccthread_create(const char* name, void (*func)(int),int param){
    block();
    dccthread_t* newThread = (dccthread_t*)malloc(sizeof(dccthread_t));
    newThread->name = name;
    newThread->id = central.globalID++;

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
    if(tid->id >= central.current_thread->id){
        getcontext(&central.current_thread->context);
        struct dlist* tmp = dlist_create();
        dlist_push_right(tmp,tid);
        dlist_push_right(tmp,central.current_thread);
        dccthread_t* thread;
        while(!dlist_empty(central.ready_queue)){
            thread = dlist_pop_left(central.ready_queue);
            dlist_push_right(tmp,thread);
        }
        central.ready_queue = tmp;
        swapcontext(&central.current_thread->context,&central.manager);
    }
    unblock();
}

void dccthread_exit(){
    block();
    swapcontext(&central.current_thread->context,&central.manager);
    unblock();
}
