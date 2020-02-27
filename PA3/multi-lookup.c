#include <pthread.h>
#include <semaphore.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFF_SIZE 1024
#define BUFF_ENTRY_SIZE 100

#define MAX_INFILES 5

typedef struct
{
    char ** buf_data;
    int buf_len;
} domain_name_request_buf_t;


typedef struct
{
    sem_t * shared_buf_sem_p;
    sem_t * shared_buf_space_avail_sem_p;
    sem_t * shared_buf_space_used_sem_p;
    domain_name_request_buf_t domain_name_request_buf;
} thread_input_t;

typedef struct
{
    thread_input_t * thread_input_p;
    sem_t * in_file_io_mutex_p;
    FILE **in_file_p;
    FILE * log_file_p;
} requester_thread_input_t;

typedef struct
{
    thread_input_t * thread_input_p;
    FILE * log_file_p;

} resolver_thread_input_t;


int process_dn(char* dn)
{
    printf("in %s with dn: %s\n", __FUNCTION__, dn);
    return 0;
}

/* start file read write monitor stuff */

FILE * open_file_sem(char * file_name, bool read, bool write, sem_t * mutex_p)
{
    printf("in %s: %s\n", __FUNCTION__, file_name);
    char access_type[3];

    if (read && write)
        strncpy(access_type, "rw", 3);
    else if (read && !write)
        strncpy(access_type, "r", 3);
    else
        strncpy(access_type, "w", 3);


    sem_wait(mutex_p); // Not really needed

    FILE * fp = fopen(file_name, access_type);

    sem_post(mutex_p);

    return fp;
}

bool close_file_sem(FILE * fp, sem_t * mutex_p)
{
    printf("in %s\n", __FUNCTION__);
    sem_wait(mutex_p);

    int ret = fclose(fp);

    sem_post(mutex_p);

    return ret;
}

bool read_single_dn_from_file_sem(FILE * fp, char* buf, int len, sem_t * mutex_p) // turn into a monitor or something
{
    printf("in %s\n", __FUNCTION__);
    sem_wait(mutex_p);

    bool ret = fgets(buf, len, fp) != NULL;
    if (ret)
        buf[strlen(buf) - 1] = 0; // Remove new line

    sem_post(mutex_p);

    return ret;
}

bool write_file_idk_sem(sem_t * mutex_p)
{
    sem_wait(mutex_p);
    sem_post(mutex_p);
    return false;
}

/* End file read write monitor stuff */


sem_t * create_sem(int _pshared, int _value)
{
    printf("in %s\n", __FUNCTION__);
    sem_t * mutex_p = malloc(sizeof(sem_t));
    sem_init(mutex_p, _pshared, _value);
    return mutex_p;
}

void remove_sem(sem_t * mutex_p)
{
    printf("in %s\n", __FUNCTION__);
    sem_destroy(mutex_p);
    free(mutex_p);
}

void * requester_loop(requester_thread_input_t * input)
{
    printf("in %s\n", __FUNCTION__);

    char file_data[BUFF_ENTRY_SIZE];

    while(read_single_dn_from_file_sem(input->in_file_p[0], file_data, BUFF_ENTRY_SIZE, input->in_file_io_mutex_p)) // TODO: change
    {
        sem_wait(input->thread_input_p->shared_buf_space_avail_sem_p); // We are adding 1 if we can


        sem_wait(input->thread_input_p->shared_buf_sem_p);
        // TODO: logic
        printf("in %s: logic: %s\n", __FUNCTION__, file_data);

        strncpy(input->thread_input_p->domain_name_request_buf.buf_data[input->thread_input_p->domain_name_request_buf.buf_len], file_data, BUFF_ENTRY_SIZE);
        input->thread_input_p->domain_name_request_buf.buf_len ++;

        sem_post(input->thread_input_p->shared_buf_sem_p);


        sem_post(input->thread_input_p->shared_buf_space_used_sem_p); // We finished adding one

        //sleep(2);
    }

    return NULL; // TODO: make return info
}

void * resolver_loop(resolver_thread_input_t * input)
{
    char buf_data[BUFF_ENTRY_SIZE];

    printf("in %s\n", __FUNCTION__);
    while(true) // TODO: change
    {
        sem_wait(input->thread_input_p->shared_buf_space_used_sem_p); // We are removing 1 if we can

        sem_wait(input->thread_input_p->shared_buf_sem_p);
        // TODO: logic
        if(input->thread_input_p->domain_name_request_buf.buf_len == 0) // Only if the requesters are done
        {
            sem_post(input->thread_input_p->shared_buf_sem_p);
            break;
        }

        input->thread_input_p->domain_name_request_buf.buf_len --;
        strncpy(buf_data, input->thread_input_p->domain_name_request_buf.buf_data[input->thread_input_p->domain_name_request_buf.buf_len], BUFF_ENTRY_SIZE);

        printf("in %s: logic: %s\n", __FUNCTION__, buf_data);

        sem_post(input->thread_input_p->shared_buf_sem_p);


        sem_post(input->thread_input_p->shared_buf_space_avail_sem_p); // We finished removing one

        //sleep(2);
    }

    return NULL;
}

int start_requesters(int num_requesters, requester_thread_input_t * requester_shared_input_p, pthread_t * thread_array)
{
    printf("in %s\n", __FUNCTION__);

    for (int req_num = 0; req_num < num_requesters; req_num++)
    {
        pthread_create(&thread_array[req_num], NULL, (void * (*)(void *)) requester_loop, (void *)requester_shared_input_p);
    }

    return 0;
}

int start_resolvers(int num_resolvers, resolver_thread_input_t * resolver_shared_input_p, pthread_t * thread_array)
{
    printf("in %s\n", __FUNCTION__);

    for (int res_num = 0; res_num < num_resolvers; res_num++)
    {
        pthread_create(&thread_array[res_num], NULL, (void * (*)(void *)) resolver_loop, (void *)resolver_shared_input_p);
    }

    return 0;
}

int start_requester_resolver_loop(int num_requesters,
    int num_resolvers,
    char * log_requesters,
    char * log_resolvers,
    char *in_files[5])
{
    printf("in %s\n", __FUNCTION__);

    if(num_requesters != 1 || num_resolvers != 1)
        return -1; // only one of each thread

    // Alloc shared buf
    char **domain_name_request_buf = malloc(BUFF_SIZE * sizeof(char*));
    for(int i = 0; i < BUFF_SIZE; i++)
    {
        domain_name_request_buf[i] = malloc(BUFF_ENTRY_SIZE);
    }

    // set up sems
    sem_t * shared_buf_sem_p = create_sem(0,1);
    sem_t * shared_buf_space_avail_sem_p = create_sem(0,BUFF_SIZE);
    sem_t * shared_buf_space_used_sem_p = create_sem(0,0);

    sem_t * in_file_io_mutex_p = create_sem(0,1); // are these the right inputs

    // start thread stuff
    printf("in %s: start thread stuff\n", __FUNCTION__);
    pthread_t threads[num_requesters + num_resolvers];
    pthread_t * requester_threads = &threads[0];
    pthread_t * resolver_threads = &threads[num_requesters];

    thread_input_t * shared_input_p = malloc(sizeof(thread_input_t));
    shared_input_p->domain_name_request_buf.buf_data = domain_name_request_buf;
    shared_input_p->domain_name_request_buf.buf_len = 0;
    shared_input_p->shared_buf_space_avail_sem_p = shared_buf_space_avail_sem_p;
    shared_input_p->shared_buf_space_used_sem_p = shared_buf_space_used_sem_p;
    shared_input_p->shared_buf_sem_p = shared_buf_sem_p;

    requester_thread_input_t * requester_shared_input_p = malloc(sizeof(requester_thread_input_t));
    requester_shared_input_p->thread_input_p = shared_input_p;
    requester_shared_input_p->in_file_io_mutex_p = in_file_io_mutex_p;
    requester_shared_input_p->in_file_p = malloc(MAX_INFILES * sizeof(FILE *));
    requester_shared_input_p->in_file_p[0] = open_file_sem(in_files[0], true, false, in_file_io_mutex_p); // TODO: change, do we care that it is on the stack
    requester_shared_input_p->log_file_p = open_file_sem(log_requesters, false, true, in_file_io_mutex_p); // TODO: change sim

    resolver_thread_input_t * resolver_shared_input_p = malloc(sizeof(resolver_thread_input_t));
    resolver_shared_input_p->thread_input_p = shared_input_p;
    resolver_shared_input_p->log_file_p = open_file_sem(log_resolvers, false, true, in_file_io_mutex_p); // TODO: change sim


    printf("in %s: start thread stuff part2\n", __FUNCTION__);

    start_requesters(num_requesters, requester_shared_input_p, requester_threads);

    start_resolvers(num_resolvers, resolver_shared_input_p, resolver_threads);

    // wait on threads 
    // clean up threads
    for(int i = 0; i < num_requesters + num_resolvers; i++)
    {
        if(i == num_requesters) // We are done with all the requesting
        {
            printf("in %s: all requesters done\n", __FUNCTION__);
            sem_post(shared_buf_space_used_sem_p); // Tells the resolvers that requesters are done. TODO: find a better way
        }

        pthread_join(threads[i], NULL);
        printf("in %s: thread %d exit\n", __FUNCTION__, i);
    }

    free(shared_input_p);
    close_file_sem(requester_shared_input_p->in_file_p[0], in_file_io_mutex_p);
    free(requester_shared_input_p);
    free(resolver_shared_input_p);

    // delete sems
    remove_sem(shared_buf_sem_p);
    remove_sem(shared_buf_space_avail_sem_p);
    remove_sem(shared_buf_space_used_sem_p);

    remove_sem(in_file_io_mutex_p); // TODO: move, or ???

    // remove buf
    for(int i = 0; i < BUFF_SIZE; i++)
    {
        free(domain_name_request_buf[i]);
    }
    free(domain_name_request_buf);

    return 0;
}

int main(int argc, char *argv[])
{
    printf("in %s\n", __FUNCTION__);
    int num_requesters = 0;
    int num_resolvers = 0;
    char * log_requesters = NULL;
    char * log_resolvers = NULL;
    char *in_files[5];
    
    if(argc < 6)
    {
        printf("expected 4 args and at least 1 input file.\n");
        return -1;
    }
    else if (argc > 10)
    {
        printf("only 5 input files please.\n");
        return -1;
    }
    else
    {
        // try to get inputs

        num_requesters = atoi(argv[1]); // TODO: do better
        num_resolvers = atoi(argv[2]);

        log_requesters = argv[3];
        log_resolvers = argv[4];

        // set in_files
        if(argc - 5 > 1)
            return -1; // only 1 file for now

        for (int i = 0; i < argc - 5; i++)
        {
            in_files[i] = argv[i + 5];
        }
    }

    return start_requester_resolver_loop(num_requesters, num_resolvers, log_requesters, log_resolvers, in_files);
}
