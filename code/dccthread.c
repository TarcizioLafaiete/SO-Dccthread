#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include "dccthread.h"
#include "dlist.h"


typedef struct dccthread{
    const char* name;
    ucontext_t context;
    int expected;
    int execution;
} dccthread_t;

typedef struct {
    ucontext_t manager;
    struct dlist* ready_queue;
    dccthread_t* current_thread;
    dccthread_t* waiter;
} managerThreads;

managerThreads central;

void managerCentral(){

    dccthread_t* thread = dlist_pop_left(central.ready_queue);
    central.current_thread = thread;
    setcontext(&thread->context);
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
    main->execution = 0;
    main->expected = 0;
    main->context.uc_link = &central.manager;
    makecontext(&(main->context),(void(*)(void))func,1,param);

    central.ready_queue = dlist_create();
    dlist_push_right(central.ready_queue,main);

    // central.waiterSignal = 0;


    setcontext(&central.manager);

    exit(EXIT_SUCCESS);

}

dccthread_t* dccthread_create(const char* name, void (*func)(int),int param){
    dccthread_t* newThread = (dccthread_t*)malloc(sizeof(dccthread_t));
    newThread->name = name;

    getcontext(&(newThread->context));
    newThread->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    newThread->context.uc_stack.ss_flags = 0;
    if((newThread->context.uc_stack.ss_sp = malloc(newThread->context.uc_stack.ss_size)) == NULL){
        perror("Failing allocationg space to new thread");
        return NULL;
    }
    newThread->expected = 0;
    newThread->execution = 0;
    newThread->context.uc_link = &central.manager;
    makecontext(&(newThread->context),(void(*)(void))func,1,param);
    
    dlist_push_right(central.ready_queue,newThread);

    // setcontext(&central.manager);

    return newThread;

}

void dccthread_yield(){
    
    // dlist_push_right(central.ready_queue,central.current_thread);
    getcontext(&central.current_thread->context);
    dlist_push_right(central.ready_queue,central.current_thread);
    swapcontext(&central.current_thread->context,&central.manager);
    

}

dccthread_t * dccthread_self(){
    return central.current_thread;
}

const char * dccthread_name(dccthread_t *tid){
    return tid->name;
}

void dccthread_wait(dccthread_t* tid){
    
}

void dccthread_exit(){

    if(!central.current_thread->expected){
        swapcontext(&central.current_thread->context,&central.manager);
    }
    else{
        setcontext(&central.waiter->context);
    }

}
