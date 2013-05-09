/**
   @author Faizal Zakaria
   sample code for my future referece.
   Userspace test application
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
#include <pthread.h>
#include <sys/time.h>
#include "fshmem.h"

typedef int bool;
enum { false, true };

#define DEVNAME "/dev/fshmem0"

typedef struct {
    int fd;
    fshmem_context *pMem;
    int memSize;
    bool runPoll;
} context;

static context gContext;

static int initialize(void) {

    memset(&gContext, 0, sizeof(gContext));
    gContext.fd = open(DEVNAME, O_RDWR, 0);
    if (gContext.fd < 0) {
        fprintf(stderr, "Failed to open %s \n", DEVNAME);
        return -1;
    }

    return 0;
}

static void msleep (unsigned int ms) {
    int microsecs;
    struct timeval tv;
    microsecs = ms * 1000;
    tv.tv_sec  = microsecs / 1000000;
    tv.tv_usec = microsecs % 1000000;
    select (0, NULL, NULL, NULL, &tv);
}

static void deinitialize(void) {

    if (gContext.fd > 0)
        close(gContext.fd);
}

static int getSharedMemory(void) {

    printf("%s \n", __func__);

    int size = sizeof(fshmem_context);
    gContext.pMem = (fshmem_context *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, gContext.fd, 0);
    if (gContext.pMem == (void *) MAP_FAILED) {
        fprintf(stderr, "Failed to mmap memory\n");
        return -1;
    }

    fprintf(stdout, "Got shared memory %p size : %d \n", gContext.pMem, gContext.memSize);

    gContext.memSize = size;
    return 0;
}

static void releaseSharedMemory(void) {
    if (gContext.pMem) {
        munmap(gContext.pMem, gContext.memSize);
    }
}

static void *pollThread(void *arg) {

    int retval;
    struct pollfd pfds[1];
    pfds[0].fd = gContext.fd;
    pfds[0].events = POLLIN | POLLRDNORM;

    while (gContext.runPoll) {
        retval = poll(pfds, (unsigned long) 1, -1);

        if (retval < 0) {
            fprintf(stderr, "Failed to poll\n");
            gContext.runPoll = false;
            break;
        }

        if (((pfds[0].revents & POLLIN) == POLLIN) &&
            (pfds[0].revents & POLLRDNORM) == POLLRDNORM) {

            /* read the shared memory */
            int consumer = gContext.pMem->consumer;
            fshmem_node node = gContext.pMem->queue[consumer];
            printf("Get from shared memory : %d\n", node.dummy);
            gContext.pMem->consumer = (gContext.pMem->consumer + 1) & gContext.pMem->mask;
        }
    }

    gContext.runPoll = false;
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char *argv[]) {

    pthread_t pollT;

    if (initialize() < 0) {
        fprintf(stderr, "Failed to initialize ... \n");
        return -1;
    }

    if (getSharedMemory() < 0) {
        fprintf(stderr, "Failed to get shared memory ... \n");
        return -1;
    }

    gContext.runPoll = true;

    // do something here
    if (pthread_create(&pollT, NULL, pollThread, NULL)) {
        printf ("Failed to create pollThread\n");
        goto exit;
    }

    sleep(2);

    while (1) {
        int cmd;
        scanf("%d", &cmd);

        if (cmd == 0) break;

        msleep(100);
    }

    pthread_cancel(pollT);
    pthread_join(pollT, NULL);

exit:

    deinitialize();
    releaseSharedMemory();
    return 0;
}

