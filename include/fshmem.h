/**
   @author Faizal Zakaria
   sample code for my future referece.
   Shared memory and poll device driver.
*/

#ifndef __FSHMEM_H__
#define __FSHMEM_H__

/*
  need to be power of 2 since I'm using "& (size - 1)" instead of "% size"
  for the performance reason.
*/
#define QUEUE_SIZE 8

typedef struct {
    int dummy;
} fshmem_node;

typedef struct {

    /* QUEUE (circular buffer) */
    int producer; // increment by user
    int consumer; // increment by driver
    fshmem_node queue[QUEUE_SIZE];
    unsigned int mask;

} fshmem_context;

#endif
