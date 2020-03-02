#include <pthread.h>
#include <semaphore.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "multi-lookup.h"

#include "util.h"

//#define DEBUG
//#define VERBOSE
//#define NO_INTERNET

#define MULT_FILE_THREADS // Just a little faster


int get_tid()
{
#ifdef SYS_gettid
    return syscall(SYS_gettid);
#else
    return -1;
#endif
}

int process_dn(char* dn, char* out)
{
#ifdef DEBUG
    printf("in %s with dn: %s\n", __FUNCTION__, dn);
#endif

#ifdef NO_INTERNET
    strncpy(out, "no.internet.com", 20);
    usleep(5000);
    int ret = UTIL_SUCCESS;
#else
    int ret = dnslookup(dn, out, BUFF_ENTRY_SIZE);
#endif

    if(ret != UTIL_SUCCESS)
    {
#ifdef DEBUG
        printf("in %s, failed\n", __FUNCTION__);
#endif
        fprintf(stderr, "%s is not a valid host name\n", dn);
    }
#ifdef DEBUG
    else
    {
        printf("in %s, ret: %d, with out_put: %s\n", __FUNCTION__, ret, out);
    }
#endif

    return ret == UTIL_SUCCESS;
}

/* start file read write monitor stuff */

FILE * open_file_sem(char * file_name, char * access_type, sem_t * mutex_p)
{
#ifdef DEBUG
    printf("in %s: %s, %s\n", __FUNCTION__, file_name, access_type);
#endif

    sem_wait(mutex_p); // Not really needed

    FILE * fp = fopen(file_name, access_type);

    sem_post(mutex_p);

    return fp;
}

bool close_file_sem(FILE * fp, sem_t * mutex_p)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif
    sem_wait(mutex_p);

    int ret = fclose(fp);

    sem_post(mutex_p);

    return ret;
}

bool read_single_dn_from_file_sem(FILE * fp, char* buf, int len, int *file_state_p, sem_t * mutex_p) // turn into a monitor or something
{
    //if(fp == NULL)
    //{
    //    return 0;
    //}
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif
    sem_wait(mutex_p);

    bool ret = fgets(buf, len, fp) != NULL;
    if (ret)
    {
        buf[strlen(buf) - 1] = (char)0; // Remove new line
    }
    else
    {
        *file_state_p = 0; // we are done reading
    }

    sem_post(mutex_p);

    return ret;
}

int pick_new_infile(file_data_t * file_data, int file_data_len, sem_t * mutex_p)
{
    // state: 0 fully read, 1 open (not threads reading), 2 being read by another thread

#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif

// pick new file file
    sem_wait(mutex_p);
    int ret = -1;

    for(int i = 0; i < file_data_len; i++)
    {
        if(file_data[i].state == 1)
        {
            ret = i;
            file_data[i].state = 2;
            break;
        }
    }

#ifdef MULT_FILE_THREADS
    // there are no unopen files, lets pick share one
    if(ret == -1)
    {
        for(int i = 0; i < file_data_len; i++)
        {
            if(file_data[i].state == 2)
            {
                ret = i;
                break;
            }
        }
    }
#endif

    sem_post(mutex_p);

    return ret;
}

int read_single_dn_from_file_data_list(file_data_t * file_data, int file_data_len, char* buf, int buf_len, sem_t * mutex_p, file_choice_data_t * file_choice_data)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif

    if(file_choice_data->file == -1)
    {
        // pick new file file
        file_choice_data->file = pick_new_infile(file_data, file_data_len, mutex_p);

        if(file_choice_data->file == -1) // issue
            return 0; // No new file found

        file_choice_data->files_served++;
#ifdef VERBOSE
        printf("in %s, %d started reading(1) filenum %d\n", __FUNCTION__, get_tid(), file_choice_data->file);
#endif
    }

    int ret = read_single_dn_from_file_sem(file_data[file_choice_data->file].file_ptr, buf, buf_len, &file_data[file_choice_data->file].state, file_data[file_choice_data->file].file_mutex);

    if(!ret) // We might need to file a new file
    {

        file_choice_data->file = pick_new_infile(file_data, file_data_len, mutex_p);
        if(file_choice_data->file == -1)
            return 0; // No new file found

        file_choice_data->files_served++;
#ifdef VERBOSE
        printf("in %s, %d started reading(2) filenum %d\n", __FUNCTION__, get_tid(), file_choice_data->file);
#endif

        // try again
        ret = read_single_dn_from_file_sem(file_data[file_choice_data->file].file_ptr, buf, buf_len, &file_data[file_choice_data->file].state, file_data[file_choice_data->file].file_mutex);
    }

    return ret;
}

bool writeln_data_to_file_sem(FILE * fp, char* data, sem_t * mutex_p)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif
    sem_wait(mutex_p);

#ifdef DEBUG
    printf("in %s: about to write: %s\n", __FUNCTION__, data);
#endif
    fprintf(fp, "%s\n", data);

    sem_post(mutex_p);
    return false;
}

/* End file read write monitor stuff */


sem_t * create_sem(int _pshared, int _value)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif
    sem_t * mutex_p = malloc(sizeof(sem_t));
    sem_init(mutex_p, _pshared, _value);
    return mutex_p;
}

void remove_sem(sem_t * mutex_p)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif
    sem_destroy(mutex_p);
    free(mutex_p);
}

void * requester_loop(requester_thread_input_t * input)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif

    char file_data[BUFF_ENTRY_SIZE];

    int domain_names_serviced = 0;

    file_choice_data_t file_choice_data = {-1,0};

    //while(read_single_dn_from_file_sem(input->in_file_data_p[0].file_ptr, file_data, BUFF_ENTRY_SIZE, input->in_file_data_p[0].file_mutex)) // TODO: change
    while(read_single_dn_from_file_data_list(input->in_file_data_p, input->in_file_len, file_data, BUFF_ENTRY_SIZE, input->in_file_io_mutex_p, &file_choice_data))
    {
        sem_wait(input->thread_input_p->shared_buf_space_avail_sem_p); // We are adding 1 if we can


        sem_wait(input->thread_input_p->shared_buf_sem_p);
        // logic
#ifdef DEBUG
        printf("in %s: logic: %s\n", __FUNCTION__, file_data);
#endif

        strncpy(input->thread_input_p->domain_name_request_buf.buf_data[input->thread_input_p->domain_name_request_buf.buf_len], file_data, BUFF_ENTRY_SIZE);
        input->thread_input_p->domain_name_request_buf.buf_len ++;

        sem_post(input->thread_input_p->shared_buf_sem_p);


        sem_post(input->thread_input_p->shared_buf_space_used_sem_p); // We finished adding one

        domain_names_serviced++;

        //sleep(2); // for testing
    }

    // TODO: change to normal
    // Write to log that we done
    // Im going to deviate from how the write up wants us to do thing and im going to log the number
    // of domains served not the number of files served
    char log_data[128];
    sprintf(log_data, "Thread %d serviced %d names from %d files.", get_tid(), domain_names_serviced, file_choice_data.files_served);
    writeln_data_to_file_sem(input->log_file_p, log_data, input->log_file_io_mutex_p);

    return NULL; // TODO: make return info
}

void * resolver_loop(resolver_thread_input_t * input)
{
    char buf_data[BUFF_ENTRY_SIZE];
    char resolver_data[BUFF_ENTRY_SIZE];
    char log_data[2*BUFF_ENTRY_SIZE+3];

#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif
    while(true) // TODO: change if there is a better way
    {
        sem_wait(input->thread_input_p->shared_buf_space_used_sem_p); // We are removing 1 if we can

        sem_wait(input->thread_input_p->shared_buf_sem_p);
        // logic
        if(input->thread_input_p->domain_name_request_buf.buf_len == 0) // Only if the requesters are done
        {
            sem_post(input->thread_input_p->shared_buf_space_used_sem_p); // allow next thread to see exit
            sem_post(input->thread_input_p->shared_buf_sem_p);
            break;
        }

        input->thread_input_p->domain_name_request_buf.buf_len --;
        strncpy(buf_data, input->thread_input_p->domain_name_request_buf.buf_data[input->thread_input_p->domain_name_request_buf.buf_len], BUFF_ENTRY_SIZE);

        sem_post(input->thread_input_p->shared_buf_sem_p);
        
#ifdef DEBUG
        printf("in %s: logic: %s\n", __FUNCTION__, buf_data);
#endif

        if(process_dn(buf_data, resolver_data))
        {
            // do something with resolver_data
#ifdef DEBUG
            printf("in %s: resolver_data: %s\n", __FUNCTION__, resolver_data);
#endif
            sprintf(log_data, "%s, %s", buf_data, resolver_data);
            writeln_data_to_file_sem(input->log_file_p, log_data, input->log_file_io_mutex_p);
        }
        else
        {
            // do something with resolver_data
#ifdef DEBUG
            printf("in %s: process_dn fail cond\n", __FUNCTION__);
#endif
            sprintf(log_data, "%s, ", buf_data);
            writeln_data_to_file_sem(input->log_file_p, log_data, input->log_file_io_mutex_p);
        }


        sem_post(input->thread_input_p->shared_buf_space_avail_sem_p); // We finished removing one

        //sleep(2);
    }

    return NULL;
}

int start_requesters(int num_requesters, requester_thread_input_t * requester_shared_input_p, pthread_t * thread_array)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif

    for (int req_num = 0; req_num < num_requesters; req_num++)
    {
#ifdef VERBOSE
        printf("in %s, starting requester thread %d\n", __FUNCTION__, req_num);
#endif
        pthread_create(&thread_array[req_num], NULL, (void * (*)(void *)) requester_loop, (void *)requester_shared_input_p);
    }

    return 0;
}

int start_resolvers(int num_resolvers, resolver_thread_input_t * resolver_shared_input_p, pthread_t * thread_array)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif

    for (int res_num = 0; res_num < num_resolvers; res_num++)
    {
#ifdef VERBOSE
        printf("in %s, starting resolver thread %d\n", __FUNCTION__, res_num);
#endif
        pthread_create(&thread_array[res_num], NULL, (void * (*)(void *)) resolver_loop, (void *)resolver_shared_input_p);
    }

    return 0;
}

int start_requester_resolver_loop(int num_requesters,
    int num_resolvers,
    char * log_requesters,
    char * log_resolvers,
    char **in_files, int num_infiles)
{
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif

    if(num_requesters > MAX_REQUESTERS || num_resolvers > MAX_RESOLVERS || num_requesters <= 0 || num_resolvers <= 0)
    {
        fprintf(stderr, "Oops, it looks like you meant to enter the correct number of threads\n");
        return -1;
    }

    if(num_infiles > MAX_INFILES || num_infiles <= 0)
    {
        fprintf(stderr, "Oops, it looks like you meant to enter the correct number of files\n");
        return -1;
    }

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

    sem_t * in_file_io_mutex_p = create_sem(0,1); // turn this into an array for each file
    sem_t * requester_log_file_io_mutex_p = create_sem(0,1);
    sem_t * resolver_log_file_io_mutex_p = create_sem(0,1);

    // start thread stuff
#ifdef DEBUG
    printf("in %s: start thread stuff\n", __FUNCTION__);
#endif
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
    requester_shared_input_p->in_file_io_mutex_p = in_file_io_mutex_p; // TODO: turn into array parallel to in_file_p
    requester_shared_input_p->log_file_io_mutex_p = requester_log_file_io_mutex_p;
    //requester_shared_input_p->in_file_p = malloc(MAX_INFILES * sizeof(FILE *));
    requester_shared_input_p->in_file_data_p = malloc(MAX_INFILES * sizeof(file_data_t));
    requester_shared_input_p->in_file_len = num_infiles;
    for(int i = 0; i < num_infiles; i++)
    {
        //requester_shared_input_p->in_file_p[i] = open_file_sem(in_files[i], "r", in_file_io_mutex_p); // TODO: change, do we care that it is on the stack

        requester_shared_input_p->in_file_data_p[i].file_mutex = create_sem(0,1);
        if((requester_shared_input_p->in_file_data_p[i].file_ptr = open_file_sem(in_files[i], "r", requester_shared_input_p->in_file_data_p[i].file_mutex)))
            requester_shared_input_p->in_file_data_p[i].state = 1;
        else
        {
            requester_shared_input_p->in_file_data_p[i].state = 0;

            fprintf(stderr, "%s is not a valid input file\n", in_files[i]);
        }

    }
    requester_shared_input_p->log_file_p = open_file_sem(log_requesters, "a", requester_log_file_io_mutex_p);

    resolver_thread_input_t * resolver_shared_input_p = malloc(sizeof(resolver_thread_input_t));
    resolver_shared_input_p->thread_input_p = shared_input_p;
    resolver_shared_input_p->log_file_io_mutex_p = resolver_log_file_io_mutex_p;
    resolver_shared_input_p->log_file_p = open_file_sem(log_resolvers, "w", resolver_log_file_io_mutex_p);

#ifdef DEBUG
    printf("in %s: start thread stuff part2\n", __FUNCTION__);
#endif

    start_resolvers(num_resolvers, resolver_shared_input_p, resolver_threads);

    start_requesters(num_requesters, requester_shared_input_p, requester_threads);

    // wait on threads 
    // clean up threads
    for(int i = 0; i < num_requesters + num_resolvers; i++)
    {
        if(i == num_requesters) // We are done with all the requesting
        {
#ifdef DEBUG
            printf("in %s: all requesters done\n", __FUNCTION__);
#endif
            sem_post(shared_buf_space_used_sem_p); // Tells the resolvers that requesters are done. TODO: find a better way
        }

        pthread_join(threads[i], NULL);
#ifdef VERBOSE
        printf("in %s: thread %d exit\n", __FUNCTION__, i);
#endif
    }

    free(shared_input_p);

    for(int i = 0; i < num_infiles; i++)
    {
        //requester_shared_input_p->in_file_p[i] = close_file_sem(in_files[i], in_file_io_mutex_p); // TODO: change, do we care that it is on the stack

        requester_shared_input_p->in_file_data_p[i].state = 0;
        if(requester_shared_input_p->in_file_data_p[i].file_ptr)
            close_file_sem(requester_shared_input_p->in_file_data_p[i].file_ptr, requester_shared_input_p->in_file_data_p[i].file_mutex);
        remove_sem(requester_shared_input_p->in_file_data_p[i].file_mutex);
    }
    close_file_sem(requester_shared_input_p->log_file_p, requester_log_file_io_mutex_p);
    close_file_sem(resolver_shared_input_p->log_file_p, resolver_log_file_io_mutex_p);

    //free(requester_shared_input_p->in_file_p);
    free(requester_shared_input_p->in_file_data_p);

    free(requester_shared_input_p);
    free(resolver_shared_input_p);

    // delete sems
    remove_sem(shared_buf_sem_p);
    remove_sem(shared_buf_space_avail_sem_p);
    remove_sem(shared_buf_space_used_sem_p);

    remove_sem(in_file_io_mutex_p);

    remove_sem(requester_log_file_io_mutex_p);
    remove_sem(resolver_log_file_io_mutex_p);

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
#ifdef DEBUG
    printf("in %s\n", __FUNCTION__);
#endif

    int num_requesters = 0;
    int num_resolvers = 0;
    char * log_requesters = NULL;
    char * log_resolvers = NULL;
    char *in_files[MAX_INFILES];
    int num_infiles = 0;
    
    if(argc < 6)
    {
        fprintf(stderr, "Expected 4 args and at least 1 input file.\nmulti-lookup <#requesters> <#resolvers> <#req_log> <#res_log> [<#infile>, ...]\n");
        return -1;
    }
    else if (argc > MAX_INFILES + 5)
    {
        fprintf(stderr, "only %d input files please.\n", MAX_INFILES);
        return -1;
    }
    else
    {
        // try to get inputs

        num_requesters = atoi(argv[1]); // TODO: do better
        num_resolvers = atoi(argv[2]);

        log_requesters = argv[3];
        log_resolvers = argv[4];

        /*
        FILE * fp;
        if(!(fp = fopen(log_requesters, "a"))) // I dont know why im doing this
        {
            fprintf(stderr, "It look like you meant to make requesters log be valid.\n");
            return -1;
        }
        fclose(fp);
        if(!(fp = fopen(log_resolvers, "r")))
        {
            fprintf(stderr, "It look like you meant to make resolvers log be valid.\n");
            return -1;
        }
        fclose(fp);
        */

        for (int i = 0; i < argc - 5; i++)
        {
            in_files[i] = argv[i + 5];
            num_infiles++;
        }
    }

    struct timeval t1;
    struct timeval t2;
    time_t elip_time;

    gettimeofday(&t1,0);

#ifdef VERBOSE
    printf("starting program\n");
#endif



    int ret = start_requester_resolver_loop(num_requesters, num_resolvers, log_requesters, log_resolvers, in_files, num_infiles);
#ifdef VERBOSE
    printf("done: %d\n", ret);
#endif

    gettimeofday(&t2,0);

    elip_time = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);

    printf("Took %ld micro secs (%f sec) to finish\n", elip_time, (float)elip_time / (float)1000000);

    return ret;
}

// TODO: more testing
// TODO: more error handling

// using: ./multi-lookup.o 5 10 test1.txt test2.txt input/names1.txt input/names2.txt input/name3.txt input/names4.txt input/names5.txt input/names3.txt 2> errlog.txt
// using: ./multi-lookup.o 5 10 test1.txt test2.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt 2> errlog.txt
