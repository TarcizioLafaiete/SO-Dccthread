#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include "dccthread.h"
#include "dlist.h"
#include <signal.h>
#include <string.h>

#define SIGNAL_BLOCK SIGUSR1

/*
================================================= STRUCT'S AND GLOBAL VARIABLES DECLARATION ====================================================================
 */

typedef struct dccthread{
    char name[DCCTHREAD_MAX_NAME_SIZE];
    ucontext_t context;
    int id;
    int blocking;
    int idWaited;
} dccthread_t;


typedef struct {
    ucontext_t manager;
    struct dlist* ready_queue;
    struct dlist* wait_queue;
    struct dlist* sleep_queue;
    dccthread_t* current_thread;
    dccthread_t* sleep_thread;
    int globalID;
    timer_t timer;
    timer_t sleeper;
} managerThreads;

//Global variable
managerThreads central;

/*
================================================= TIMER TRATEMENT FUNCTIONS ====================================================================
 */

void __expired(int signo){
        //Verify if function was called for correct signal
        if(signo == SIGNAL_BLOCK){
            getcontext(&central.current_thread->context);
            dlist_push_right(central.ready_queue,central.current_thread);
            swapcontext(&central.current_thread->context,&central.manager);
        }
}

void __sleep(){
        getcontext(&central.current_thread->context);
        dccthread_t* sleep_thread = dlist_pop_left(central.sleep_queue);
        dlist_push_right(central.ready_queue,sleep_thread);
        timer_delete(central.sleeper);
        swapcontext(&central.current_thread->context,&central.manager);
}

/*
================================================= TIMER CREATER'S ====================================================================
 */

void create_timer(int ms){
    // central.timer.event = { 0 };

    struct sigaction sa;
    struct sigevent event;
    timer_t timer_id;
    struct itimerspec its;

    timer_id = 0;
    //Configure sa struct
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = (void*)__expired;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGNAL_BLOCK, &sa, NULL);

    //Configure event
    event.sigev_notify = SIGEV_SIGNAL; //Vefiry documetation
    event.sigev_signo = SIGNAL_BLOCK;

    //Create timer
    timer_create(CLOCK_PROCESS_CPUTIME_ID,&event,&timer_id);


    //Define a timer in ms and set timer
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = ms * 1000000;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = ms * 1000000;
    timer_settime(timer_id,0,&its,NULL);

    //Save timer_id to use in block() function
    central.timer = timer_id;

}

void create_sleep(struct timespec ts){
    
    struct itimerspec timer_spec;
    struct sigevent sev;
    timer_t timer_id;
    
    // Configure event struct
    sev.sigev_notify = SIGEV_THREAD; //Verify documentation
    sev.sigev_notify_function = (void*)__sleep;
    sev.sigev_value.sival_ptr = &timer_id;
    timer_create(CLOCK_REALTIME, &sev, &timer_id);

    // Pass timescpec parameter to itimerspec and set timer
    timer_spec.it_value = ts;
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;
    timer_settime(timer_id, 0, &timer_spec, NULL);

    //Save timer_id to use in __sleep() function
    central.sleeper = timer_id;
   
}

/*
================================================= RACE CONDITION TREATAMENT FUNCTION ====================================================================
 */

void block(){
    if(!central.current_thread->blocking){
        central.current_thread->blocking = 1;
        timer_delete(central.timer);
    }
}

void unblock(){
    central.current_thread->blocking = 0;
    create_timer(10);
}

/*
================================================= AUXILIARY FUNCTION TO FIND A THREAD IN QUEUE ====================================================================
 */

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


/*
================================================= MANAGER THREAD FUNCTION ====================================================================
 */

void managerCentral(){

    dccthread_t* thread = dlist_pop_left(central.ready_queue);
    central.current_thread = thread;
    setcontext(&thread->context);

}

/*
================================================= DCCTHREAD_T FUNCTION'S ====================================================================
 */

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
    dccthread_t* firstThread = (dccthread_t*)malloc(sizeof(dccthread_t));
    getcontext(&(firstThread->context));
    firstThread->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    firstThread->context.uc_stack.ss_flags = 0;
    if((firstThread->context.uc_stack.ss_sp = malloc(firstThread->context.uc_stack.ss_size)) == NULL){
        perror("Failing allocating space to main thread");
        exit(1);
    }

    //Fill dcctthread_t variables
    strcpy(firstThread->name,"main"); //Must be this function
    firstThread->id = 0;
    firstThread->idWaited = -1;
    firstThread->context.uc_link = &central.manager;  //After the function end's return the context to manager
    makecontext(&(firstThread->context),(void(*)(void))func,1,param);


    //Creating the queue's
    central.ready_queue = dlist_create();
    central.wait_queue = dlist_create();
    central.sleep_queue = dlist_create();
    
    //Start ready_queue with main thread
    dlist_push_right(central.ready_queue,firstThread);

    //Start global ID
    central.globalID = 0;

    //Start preemption timer 
    create_timer(10);

    //Go to Manager thread
    setcontext(&central.manager);

    //Non - returnable function
    exit(EXIT_SUCCESS);

}



dccthread_t* dccthread_create(const char* name, void (*func)(int),int param){
    block();

    dccthread_t* newThread = (dccthread_t*)malloc(sizeof(dccthread_t));
    strcpy(newThread->name,name); // Must be this function

    newThread->id = central.globalID++;
    newThread->idWaited = -1;

    //Creation context
    getcontext(&(newThread->context));
    newThread->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    newThread->context.uc_stack.ss_flags = 0;
    if((newThread->context.uc_stack.ss_sp = malloc(newThread->context.uc_stack.ss_size)) == NULL){
        perror("Failing allocationg space to new thread");
        return NULL;
    }
    newThread->context.uc_link = &central.manager; // After the function end's return the context to manager
    
    makecontext(&(newThread->context),(void(*)(void))func,1,param);
    
    //Put in ready_queue
    dlist_push_right(central.ready_queue,newThread);

    unblock();

    return newThread;

}

void dccthread_yield(){

    block();
    //Save current context
    getcontext(&central.current_thread->context);
    //Return current_thread to ready_queue
    dlist_push_right(central.ready_queue,central.current_thread);
    //Go to manager to start another thread
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
    //Verify existence of thread
    if(dlist_find(central.ready_queue,tid) || dlist_find(central.sleep_queue,tid)){
        getcontext(&central.current_thread->context);
        tid->idWaited = central.current_thread->id;
        //current_thread now is waiting tid thread
        dlist_push_right(central.wait_queue,central.current_thread);
        //tid thread go to ready queue
        dlist_push_right(central.ready_queue,tid);
        swapcontext(&central.current_thread->context,&central.manager);
    }

    unblock();
}

void dccthread_exit(){
    block();
    //Verify if has a thread waiting it
    if(central.current_thread->idWaited != -1){
        dccthread_t* thread = dlist_pop_left(central.wait_queue);
        //Return the waiter thread to ready queue
        dlist_push_right(central.ready_queue,thread);
    }
    swapcontext(&central.current_thread->context,&central.manager);
    unblock();
}

void dccthread_sleep(struct timespec ts){
    
    block();
 
    getcontext(&central.current_thread->context);
    // Put thread in sleep_queue
    dlist_push_right(central.sleep_queue,central.current_thread);
    //Create sleep timer
    create_sleep(ts);
    ;
    swapcontext(&central.current_thread->context,&central.manager);

    unblock();
}