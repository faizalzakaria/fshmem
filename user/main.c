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

#include "fshmem.h"

#define DEVNAME "/dev/fshmem0"

typedef struct {
	int fd;
	fshmem_context *pMem;
	int memSize;
} context;

static context gContext;

static int initialize(void) {

	printf("%s \n", __func__);
	
	memset(&gContext, 0, sizeof(gContext));
	gContext.fd = open(DEVNAME, O_RDWR, 0);
	if (gContext.fd < 0) {
		fprintf(stderr, "Failed to open %s \n", DEVNAME);
		return -1;
	}

	return 0;
}

static void deinitialize(void) {

	if (gContext.fd > 0)
		close(gContext.fd);
}

int getSharedMemory(void) {

	printf("%s \n", __func__);

	int size = sizeof(fshmem_context);
	gContext.pMem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, gContext.fd, 0);
	if (gContext.pMem == (void *) MAP_FAILED) {
		fprintf(stderr, "Failed to mmap memory\n");
		return -1;
	}

	fprintf(stdout, "Got shared memory %p size : %d \n", gContext.pMem, gContext.memSize);

	gContext.memSize = size;
	return 0;
}

void releaseSharedMemory(void) {
	
	printf("%s \n", __func__);
	
	if (gContext.pMem) {
		munmap(gContext.pMem, gContext.memSize);
	}
}

int main(int argc, char *argv[]) {

	if (initialize() < 0) {
		fprintf(stderr, "Failed to initialize ... \n");
		return -1;
	}

	if (getSharedMemory() < 0) {
		fprintf(stderr, "Failed to get shared memory ... \n");
		return -1;
	}

	// do something here
	sleep(2);

	deinitialize();
	releaseSharedMemory();
	return 0;
}

