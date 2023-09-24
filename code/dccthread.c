#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include "dccthread.h"


typedef struct {
    ucontext_t manager;
    ucontext_t main;
} managerThreads;

managerThreads central;

void managerCentral(){
    setcontext(&central.main);
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
    getcontext(&central.main);
    central.main.uc_stack.ss_size = THREAD_STACK_SIZE;
    central.main.uc_stack.ss_flags = 0;
    if((central.main.uc_stack.ss_sp = malloc(central.main.uc_stack.ss_size)) == NULL){
        perror("Failing allocating space to main thread");
        exit(1);
    }
    makecontext(&central.main,func,1,param);
}