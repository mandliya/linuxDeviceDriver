#ifndef DEVICE_STRUCT_H
#define DEVICE_STRUCT_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/cred.h>

#define PCI_VENDOR_ID_CCORSI 0x1234
#define PCI_DEVICE_ID_KYOUKO2 0x1113

#define NUM_BUFFER 8

struct cdev kyouko2_cdev;

/*
 *This struct stores all the information and flags 
 *we need to store for the driver
 */
struct kyouko2{
  unsigned long  p_control_base;
  unsigned long p_ram_base;		
  unsigned int* k_control_base;
  unsigned int* k_ram_base;
   
  unsigned long controlLen;
  unsigned long ramLen;
  struct pci_dev *dev;

  unsigned long buffsize;

  unsigned int graphics_on;
  unsigned int dma_mapped;
  unsigned int fill;
  unsigned int drain;

  uid_t current_user;
}k2;

/*
 * This struct holds the informations/addresses for each buffer 
 *
 */
 
struct dma_buff {
  unsigned int* k_dma_base;
  dma_addr_t p_dma_base;
  unsigned int u_buffer_addr;
  int count;
}buff_queue[NUM_BUFFER];

int k2_queue_full = 0;
unsigned long flags;
int tri=0;





int draino;







#endif
