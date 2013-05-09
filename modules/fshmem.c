/**
   @author Faizal Zakaria
   sample code for my future referece.
   Shared memory and poll device driver.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "fshmem.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FAIZAL ZAKARIA");

#define DEVICE_NAME "fshmem"
#define MAX_USERS 1

/******************************************************************************/
/****************************** Typedef ***************************************/
/******************************************************************************/

typedef struct {
	struct cdev cdev;
	struct mutex lock;
	size_t usersCnt;
	void * pSMemP;
	void * pSMemK;
	size_t memSize;

	wait_queue_head_t readQueue;
	int dataAvailable;

	fshmem_context *evtQ;
} fshmem_cdev;

/******************************************************************************/
/******************************* Static Variables *****************************/
/******************************************************************************/

static fshmem_cdev * pDevInfo;
static dev_t stDev;
static volatile bool exitTask;

/******************************************************************************/
/******************************* Static functions *****************************/
/******************************************************************************/

static int fshmem_open(struct inode * inode, struct file * file);
static int fshmem_release(struct inode * inode, struct file * file);
static int fshmem_mmap(struct file *file, struct vm_area_struct *vma);
static unsigned int fshmem_poll(struct file *file, poll_table *wait);

/* fops */
static struct file_operations fileOps = {
   .owner          = THIS_MODULE,      /* Owner              */
   .open           = fshmem_open,       /* open method        */
   .release        = fshmem_release,     /* release method     */
   .mmap           = fshmem_mmap,        /* mmap method         */
   .poll           = fshmem_poll,        /* Poll method        */
};

/******************************************************************************/
/******************************************************************************/

static void fshmem_init_dev(fshmem_cdev *pDev) {

	mutex_init(&(pDevInfo->lock));

	pDev->pSMemP = NULL;
	pDev->pSMemK = NULL;
	pDev->memSize = 0;
	pDev->usersCnt = 0;

	init_waitqueue_head(&pDev->readQueue);
	pDev->dataAvailable = 0;

	pDev->evtQ = NULL;
}

/******************************************************************************/

static int fshmem_get_memory(fshmem_cdev *pDev, size_t iSize) {

	/* Allocate the memory if its not yet allocated */
	if (pDev->pSMemP == NULL) {
		unsigned long virt_addr;

		pDev->pSMemK = kmalloc(iSize + 2 * PAGE_SIZE, GFP_KERNEL);
		if (!pDev->pSMemK) {
			printk(KERN_CRIT "%s: Failed to allocate memory\n", __func__);
			return(-ENOMEM);
		}

		/* align memory */
		pDev->pSMemP = (void *)(((unsigned long) pDev->pSMemK + PAGE_SIZE - 1) & PAGE_MASK);
		pDev->memSize = iSize;

		/* mark page as reserved */
		for (virt_addr = (unsigned long) pDev->pSMemP;
			 virt_addr < (unsigned long) (pDev->pSMemP + iSize);
			 virt_addr += PAGE_SIZE) {
			SetPageReserved(virt_to_page((void *) virt_addr));
		}

		printk("shared memory unaligned : 0x%p\n", pDev->pSMemK);
		printk("shared memory aligned : 0x%p\n", pDev->pSMemP);

		memset(pDev->pSMemP, 0, iSize);
	}

	if (pDev->memSize < iSize) {
		printk(KERN_CRIT "%s: Mismatch request size (%d < %d)\n", __func__, pDev->memSize, iSize);
		return(-EFBIG);
	}

	return(0);
}

/******************************************************************************/

static void fshmem_free_memory(fshmem_cdev *pDev) {
	if (pDev->pSMemP) {
		unsigned long virt_addr;

		for (virt_addr = (unsigned long) pDev->pSMemP;
			 virt_addr < (unsigned long) (pDev->pSMemP + pDev->memSize);
			 virt_addr += PAGE_SIZE) {
			ClearPageReserved(virt_to_page((void *) virt_addr));
		}
		kfree(pDev->pSMemK);
		pDev->pSMemP = NULL;
		pDev->pSMemK = NULL;
		pDev->memSize = 0;
		pDev->evtQ = NULL;
	}
}

/******************************************************************************/

static int fshmem_open(struct inode * inode, struct file * file) {
	int ret = -ENODEV;
	fshmem_cdev * pDev = (fshmem_cdev *) container_of(inode->i_cdev, fshmem_cdev, cdev);
	
	mutex_lock(&(pDev->lock));
	if (pDev->usersCnt < MAX_USERS) {
		file->private_data = pDev;

		pDev->usersCnt++;
		ret = 0;
	}

	mutex_unlock(&(pDev->lock));
	return ret;
}

/******************************************************************************/

static int fshmem_release(struct inode * inode, struct file * file) {
	fshmem_cdev * pDev = (fshmem_cdev *) container_of(inode->i_cdev, fshmem_cdev, cdev);

	mutex_lock(&(pDev->lock));
	pDev->usersCnt--;
	mutex_unlock(&(pDev->lock));
	return 0;
}

/******************************************************************************/

static unsigned int fshmem_poll(struct file *file, poll_table *wait) {

	fshmem_cdev *pDev = file->private_data;
	unsigned int mask = 0;

	printk("%s : %d\n", __func__, __LINE__);

	mutex_lock(&(pDev->lock));

	if (pDev->dataAvailable) mask |= POLLIN | POLLRDNORM; /* readable */

	poll_wait(file, &pDev->readQueue, wait);

	if (pDev->dataAvailable > 0) pDev->dataAvailable--;

	mutex_unlock(&(pDev->lock));
	
	return mask;
}

/******************************************************************************/

static int fshmem_mmap(struct file *file, struct vm_area_struct *vma) {

	fshmem_cdev *pDev = file->private_data;
	size_t iSize = vma->vm_end - vma->vm_start;
	int ret;
	
	if (pDev->pSMemP) {
		fshmem_free_memory(pDev);
	}

	printk("%s : trying to map ... size : (%ld)\n", __func__, vma->vm_end - vma->vm_start);

	ret = fshmem_get_memory(pDev, iSize);
	
	if (ret != 0) {
		printk(KERN_CRIT "%s: Failed to allocate memory ... \n", __func__);
		return -1;
	}

	if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		printk(KERN_CRIT "%s: Memory is writable but not sharable ... \n", __func__);
		return(-EINVAL);
	}
	
	/* lock memory */
	vma->vm_flags |= VM_LOCKED;
	if (remap_pfn_range(vma, vma->vm_start, virt_to_phys((void *)(pDev->pSMemP)) >> PAGE_SHIFT, 
						iSize, PAGE_SHARED)) {
		printk(KERN_CRIT "%s: Failed to lock memory ... \n", __func__);
		return(-ENXIO);
	}

	/* got map memory */
	pDev->evtQ = (fshmem_context *) pDev->pSMemP;
	pDev->evtQ->mask = QUEUE_SIZE - 1;

	return 0;
}

/******************************************************************************/

static int fshmem_task(void* arg) {

	fshmem_cdev *pDev = (fshmem_cdev *) arg;
	int cnt = 0;

	while (!exitTask) {
		// do dummy stuff
		fshmem_node node;

		msleep(1000);
		
		if (pDev->evtQ != NULL) {
			// just dummy data
			node.dummy = cnt;
			cnt = (cnt + 1) & pDev->evtQ->mask;
			
			pDev->evtQ->queue[pDev->evtQ->producer] = node;
			pDev->evtQ->producer = (pDev->evtQ->producer + 1) & pDev->evtQ->mask;
			pDev->dataAvailable++;
			wake_up(&(pDev->readQueue));
		}
	}

	exitTask = false;
	return 0;
}

/******************************************************************************/

static int fshmem_init(void) {
	int ret;

	printk("%s : Initialising driver ... \n", __func__);

	printk("%08x %08x\n", PAGE_MASK, PAGE_SIZE);

	if (alloc_chrdev_region(&stDev, 0, 1, DEVICE_NAME) < 0) {
		printk(KERN_ERR "%s: Failed to register device %s\n", __func__, DEVICE_NAME);
		return -1;
	}

	pDevInfo = (fshmem_cdev *) kmalloc(sizeof(fshmem_cdev), GFP_KERNEL);
	if (pDevInfo) {
		fshmem_init_dev(pDevInfo);
	} else {
		printk(KERN_WARNING "%s: Failed to allocate memory for pDevInfo ...\n", __func__);
		return(-ENOMEM);
	}

	cdev_init(&(pDevInfo->cdev), &fileOps);
	pDevInfo->cdev.owner = THIS_MODULE;

	ret = cdev_add(&pDevInfo->cdev, stDev, 1);
	if (ret) {
		printk(KERN_WARNING "%s: Failed to add \n", __func__);
		return ret;
	}

	exitTask = false;
	kthread_run(fshmem_task, (void *) pDevInfo, "FSHMEM_DUMMY");

	return 0;
}

/******************************************************************************/

static void fshmem_exit(void) {

	printk("%s : Exiting driver ... \n", __func__);

	exitTask = true;
	// wait for task to exit
	msleep(100);

	mutex_destroy(&(pDevInfo->lock));
	pDevInfo->usersCnt = 0;

	cdev_del(&pDevInfo->cdev);
	unregister_chrdev_region(stDev, 1);
	fshmem_free_memory(pDevInfo);
}

/******************************************************************************/

module_init(fshmem_init);
module_exit(fshmem_exit);
