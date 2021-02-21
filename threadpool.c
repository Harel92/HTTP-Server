#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>



threadpool* create_threadpool(int num_threads_in_pool){

    if(num_threads_in_pool>MAXT_IN_POOL || num_threads_in_pool<=0){
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        return NULL;
    }

    threadpool* pool=(threadpool*)malloc(sizeof(threadpool));
    if(pool==NULL){
        perror("Cannot allocate memory");
        return NULL;
    }

    pool->num_threads=num_threads_in_pool;
    pool->qsize=0;
    pool->qhead=NULL;
    pool->qtail=NULL;
    pool->shutdown=0;
    pool->dont_accept=0;
    pthread_mutex_init(&pool->qlock,NULL);
    pthread_cond_init(&pool->q_not_empty,NULL);
    pthread_cond_init(&pool->q_empty,NULL);
    pool->threads=(pthread_t*)malloc(sizeof(pthread_t)*num_threads_in_pool);

    int status;
    for(int i=0 ; i<num_threads_in_pool ; i++){
        status = pthread_create(&pool->threads[i], NULL, do_work,pool); 
        if(status){
            perror("Cannot create thread");
            return NULL;
        }
    }

    return pool;
}



void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){

    if(from_me->dont_accept==1){
        printf("Cant accept any new tasks\n");
        return;
    }

    work_t* work=(work_t*)malloc(sizeof(work_t));
    if(work==NULL)
        {
          perror("Cannot allocate memory");
          return;  
        }

    work->routine=dispatch_to_here;
    work->arg=arg;
    work->next=NULL;
    pthread_mutex_lock(&from_me->qlock);

    if(from_me->qhead==NULL){
        from_me->qhead=work;
        from_me->qtail=work;
    }

    else{
        from_me->qtail->next=work;
        from_me->qtail=from_me->qtail->next;
    }
    from_me->qsize++;
    pthread_mutex_unlock(&from_me->qlock);
    pthread_cond_signal(&from_me->q_not_empty);

}

void* do_work(void* p){

    threadpool* temp=(threadpool*)p;
   
    while(1){
        pthread_mutex_lock(&temp->qlock);
        if(temp->shutdown==1){
            pthread_mutex_unlock(&temp->qlock);
            return NULL;
        }
        if(temp->qsize==0)
            pthread_cond_wait(&temp->q_not_empty,&temp->qlock);

        if(temp->shutdown==1){
            pthread_mutex_unlock(&temp->qlock);
            return NULL;
        }

        work_t* cur=temp->qhead;
        temp->qsize--;
        if(temp->qsize==0){
            temp->qhead=NULL;
            temp->qtail=NULL;  

        if(temp->dont_accept==1)
            pthread_cond_signal(&temp->q_empty);    
        }    
        else
            temp->qhead=temp->qhead->next;
        
        pthread_mutex_unlock(&temp->qlock);
        cur->routine(cur->arg);
        free(cur);
                    
    }

}


void destroy_threadpool(threadpool* destroyme){

    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept=1;
    if(destroyme->qsize!=0)
        pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock);

    destroyme->shutdown=1;
    pthread_mutex_unlock(&destroyme->qlock);
    pthread_cond_broadcast(&destroyme->q_not_empty);

    int status;
    for(int i=0;i< destroyme->num_threads; i++) { 
	    status = pthread_join(destroyme->threads[i],NULL);
	    if (status) { 
		    perror("Error join process");
	     } 	
    }

    free(destroyme->threads);
    free(destroyme);    

}