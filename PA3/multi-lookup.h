#ifndef _MULTI_LOOKUP_H
#define _MULTI_LOOKUP_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "util.h"

#define BUFF_SIZE 1024
#define BUFF_ENTRY_SIZE 1025

#define MAX_INFILES 10

#define MAX_REQUESTERS 5
#define MAX_RESOLVERS 10

/* Start shared data structs */

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
    FILE * file_ptr;
    int state;
    sem_t * file_mutex;
} file_data_t;

typedef struct
{
    thread_input_t * thread_input_p;
    sem_t * in_file_io_mutex_p;
    sem_t * log_file_io_mutex_p;
    //FILE **in_file_p;
    file_data_t * in_file_data_p;
    int in_file_len;
    FILE * log_file_p;
} requester_thread_input_t;

typedef struct
{
    thread_input_t * thread_input_p;
    sem_t * log_file_io_mutex_p;
    FILE * log_file_p;

} resolver_thread_input_t;

/* End shared data structs */



#endif
