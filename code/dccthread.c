#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include "dccthread.h"


typedef struct dccthread{
    ucontext_t context;
    const char* name;
} dccthread_t;

typedef struct {
    ucontext_t manager;
    dccthread_t main;
    dccthread_t* secundary[100];
    int num_of_threads;
    int current_thread;
} managerThreads;

managerThreads central;

void managerCentral(){
    
    getcontext(&central.manager);
    swapcontext(&central.manager,&central.main.context);

    int i = 0;
    getcontext(&central.manager);
    printf("AAA\n");
    while(central.secundary[central.current_thread] != NULL){
        if(central.current_thread + 1 < central.num_of_threads){
            central.current_thread++;
        }
        swapcontext(&central.manager,&central.secundary[central.current_thread - 1]->context);
    }
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
    getcontext(&central.main.context);
    central.main.context.uc_stack.ss_size = THREAD_STACK_SIZE;
    central.main.context.uc_stack.ss_flags = 0;
    if((central.main.context.uc_stack.ss_sp = malloc(central.main.context.uc_stack.ss_size)) == NULL){
        perror("Failing allocating space to main thread");
        exit(1);
    }
    central.main.name = "main";
    central.main.context.uc_link = &central.manager;
    makecontext(&central.main.context,func,1,param);

    for(int i = 0;i < 100;i++){
        central.secundary[i] = (dccthread_t*) NULL;
    }
   
   central.num_of_threads = 0;
   central.current_thread = 0;

    setcontext(&central.manager);
    exit(EXIT_SUCCESS);

}

dccthread_t* dccthread_create(const char* name, void (*func)(int),int param){
    dccthread_t newThread;
    newThread.name = name;

    getcontext(&newThread.context);
    newThread.context.uc_stack.ss_size = THREAD_STACK_SIZE;
    newThread.context.uc_stack.ss_flags = 0;
    if((newThread.context.uc_stack.ss_sp = malloc(newThread.context.uc_stack.ss_size)) == NULL){
        perror("Failing allocationg space to new thread");
        return NULL;
    }
    newThread.context.uc_link = &central.manager;
    makecontext(&newThread.context,func,1,param);

    central.num_of_threads++;

    int i = -1;
    do{
        i++;
        if(central.secundary[i] == NULL){
            central.secundary[i] = &newThread;
        }
    } while(central.secundary[i] == NULL);

    return &newThread;

}