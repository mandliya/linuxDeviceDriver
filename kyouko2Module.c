/*
 * ################################################################
   File: kyouko2Module.c 
   Purpose: Device driver for Kyouko2 GPU
   Team: Ben Whetstone, Ravi Mandliya
   ################################################################
*/


#include "defs.h"
#include "deviceStruct.h"


MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("RB");

//Initialize spinlock
DEFINE_SPINLOCK(k2_lock);

//Read int from register
unsigned int K_READ_REG(unsigned int reg){
  unsigned int value;
  rmb();
  value = *(k2.k_control_base+(reg>>2));
  return(value);
}

//Write int to register
void K_WRITE_REG(unsigned int reg, unsigned int val) {
  *(k2.k_control_base + (reg>>2)) = val;
}

//Write float to register
void K_WRITE_REG_F(unsigned int reg, float val) {
  *(k2.k_control_base + (reg>>2)) = *(unsigned int*) (&val);
}

//Sync devices
void K_SYNC(void) {
  while(K_READ_REG(FIFO_Depth));
}

//Open kyouko 2 device 
int kyouko2_open(struct inode *inode, struct file *fp){
  printk(KERN_ALERT "Kyouko2 opened\n");
  //Map control registers and RAM to kernel space
  k2.k_control_base = ioremap_nocache(k2.p_control_base,k2.controlLen);
  k2.k_ram_base = ioremap_nocache(k2.p_ram_base,k2.ramLen);	
  
  //Set default flag status
  k2.dma_mapped = 0;
  k2.graphics_on = 0;
  k2.buffsize=0;
  
  // Init DMA buffers processed count 
  draino=0;
  /*
   * Store fs user ID in order to determine 
   * if user can mmap control registers and RAM
   */
  k2.current_user=current_fsuid();
  
  return 0;
}
// Mmap into userspace
int kyouko2_mmap(struct file *fp, struct vm_area_struct *vma){
  int ret = -1;
  //Control register case (page offset = 0) 
 	if(((vma->vm_pgoff)<<PAGE_SHIFT) == 0) {
    //Checks if root user
    if(k2.current_user != 0) {
      printk(KERN_ALERT "Must be root to access control registers\n");
      return ret;
    }
    //Map kernel control register memory region into process address space
  	ret = io_remap_pfn_range(vma, vma->vm_start, k2.p_control_base>>PAGE_SHIFT, k2.controlLen, vma->vm_page_prot);
  }
  //RAM case (page offset = 0x8000 0000)
	else if (((vma->vm_pgoff)<<PAGE_SHIFT) == 0x80000000) {
    if(k2.current_user != 0) {
      printk(KERN_ALERT "Must be root to access framebuffer\n");
      return ret;
    }
    //Map kernel RAM memory region into process address space
   	ret = io_remap_pfn_range(vma, vma->vm_start, k2.p_ram_base>>PAGE_SHIFT, k2.ramLen, vma->vm_page_prot);
  }
  //DMA case (offset=physical bus address)
  else {
    //Map kernel DMA memory region into process address space
    ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, (vma->vm_end)-(vma->vm_start), vma->vm_page_prot);
  }
  
  return ret;
}

//Inits wait queue structure
DECLARE_WAIT_QUEUE_HEAD(dma_snooze);

//Helper function for starting DMA transfers
void init_transfer(void) {
  int notdone=1;
  //Get lock and disable interrupt (save interrupt config in flags)
  spin_lock_irqsave(&k2_lock, flags);

  //DMA buffers empty and not processing so can start immediately
  if(k2.fill==k2.drain) { 
    //Increment fill
    k2.fill=(k2.fill + 1) % NUM_BUFFER;
    //Release lock and restore interrupts
    spin_unlock_irqrestore(&k2_lock, flags);

    //Write to registers to start DMA
    tri++;
    K_WRITE_REG(Buffer_Address, buff_queue[k2.drain].p_dma_base);
    K_WRITE_REG(Buffer_Config, buff_queue[k2.drain].count);
    
    // Increment number of buffers processed
    draino++;
  }
  else {
    //Increment fill
    k2.fill=(k2.fill + 1) % NUM_BUFFER;
    
    //Set flag which indicates if buffers are full
    if(k2.fill==k2.drain) 
      k2_queue_full=1;
    
    //Loop to keep checking after waking up as queue may become full between awaking and returning from sleep
    while(notdone){
      //Buffer queue is not full, unset flag, and return
      if(k2_queue_full == 0) {
        notdone = 0;
      }
      //Buffer queue is full
      else{
        //Restore interrupts and release lock before sleeping (bad otherwise)
        spin_unlock_irqrestore(&k2_lock, flags);
        //Sleep until buffer no longer full
        wait_event_interruptible(dma_snooze, k2_queue_full == 0);
        //Get spinlock and disable interrupts before checking if queue is still not full
        spin_lock_irqsave(&k2_lock, flags);
      }
    }
    //Restore flags and release lock before returning
    spin_unlock_irqrestore(&k2_lock, flags);
  }
}

//DMA interrupt handler
irqreturn_t dma_intr(int irq, void *dev_id, struct pt_regs *regs) {
  unsigned int iflags;
  
  spin_lock_irqsave(&k2_lock, flags);
  //Save GPU interrupts
  iflags=K_READ_REG(Info_Status);

  //Clear interrupts
  K_WRITE_REG(Info_Status, 0xf);

  //If interrupt is not for DMA, release lock and return IRQ_NONE
  if((iflags & 0x02) == 0) {
    spin_unlock_irqrestore(&k2_lock, flags);
    return IRQ_NONE;
  }
  //Otherwise, interrupt is valid
  else {
    
    //Increment drain
    k2.drain=(k2.drain+1) % NUM_BUFFER;

    //If buffer ring is not empty, launch next buffer
    if(!(k2.fill == k2.drain && k2_queue_full==0)) {
      //Sync to make sure previous writes to regs have completed
      K_SYNC();
      
      //Write to reg to initiate DMA transfer
      K_WRITE_REG(Buffer_Address, buff_queue[k2.drain].p_dma_base);
      K_WRITE_REG(Buffer_Config, buff_queue[k2.drain].count);

      //Increment DMA buffers drawn counter
      draino++;
    }
    spin_unlock_irqrestore(&k2_lock, flags);
    //If user is waiting on not full then wake them up
    if(k2_queue_full) {
        k2_queue_full = 0;
        wake_up_interruptible(&dma_snooze);
    } 
    return IRQ_HANDLED;
  }
}

long kyouko2_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
  switch(cmd) {
    case VMODE:
    {
      if (arg == GRAPHICS_ON) {
        //Set X resolution
        K_WRITE_REG(Frame_Col, 1024);
        //Set Y resolution
        K_WRITE_REG(Frame_Row, 768);
        //Set frame pitch
        K_WRITE_REG(Frame_Pitch, 4096);
        //Set pixel type
        K_WRITE_REG(Frame_Pixel, 0xF888);
        //Set start of framebuffer
        K_WRITE_REG(Frame_Start, 0x0);
        
        //Set resolution for encoder
        K_WRITE_REG(Encoder_Width, 1024);
        K_WRITE_REG(Encoder_Height, 768);
        //Set offsets for encoder
        K_WRITE_REG(Encoder_OffX, 0);
        K_WRITE_REG(Encoder_OffY, 0);
        K_WRITE_REG(Encoder_Frame, 0);
        
        //Turn on 3D hardware acceleration
        K_WRITE_REG(Config_Accel, 0x40000000);
        
        K_SYNC();
        
        K_WRITE_REG(Config_ModeSet,0x0);
        
        //set clear color
        
        K_WRITE_REG_F(Clear_R, 0.5);
        K_WRITE_REG_F(Clear_G, 0.5);
        K_WRITE_REG_F(Clear_B, 0.4);
        
        //Flushes raster queue
        K_WRITE_REG(Raster_Flush, 0);
        
        K_SYNC();
        
        //Clears screen to clearcolor
        K_WRITE_REG(Raster_Clear, 1);

        //Set flag
        k2.graphics_on=1;
		  }
      //Graphics mode off
      else{
        k2.graphics_on=0;
        K_SYNC();
        K_WRITE_REG(Config_Reboot, 0);
      }
      
      break;
    }
    
    case SYNC:
    {
      K_SYNC();
      
      break;
    }
    
    case FLUSH:
    {
      K_WRITE_REG(Raster_Flush, 0);
      
      break;
    }
    
    case BIND_DMA:
    {
      int i;
      int result;

      //Set default fill and drain
      k2.fill = 0;
      k2.drain = 0;
      
      //Alloc room in kernelspace for DMA buffers and save bus address
      for(i = 0; i < NUM_BUFFER; ++i) {
        buff_queue[i].k_dma_base = pci_alloc_consistent(k2.dev, BUFFER_SIZE*1024, &(buff_queue[i].p_dma_base));
        buff_queue[i].count = 0;
      }
      
      //Mmap kernel DMA buffer to user space
      for(i = 0; i < NUM_BUFFER; ++i) {
        buff_queue[i].u_buffer_addr = do_mmap(fp, ((unsigned long)(buff_queue[i].p_dma_base)),BUFFER_SIZE*1024, PROT_READ|PROT_WRITE, MAP_SHARED, buff_queue[i].p_dma_base);
      }

      //Enable MSI capabilities on the card     
      pci_enable_msi(k2.dev);

      //Set interrupt handler for the card
      result = request_irq((k2.dev)->irq,(irq_handler_t) dma_intr, IRQF_SHARED|IRQF_DISABLED,"dma_intr",&k2);
      
      //Reset all interrupts that may have occurred before interrupts were configured
      K_WRITE_REG(Info_Status, 0xffffffff);
      //Configuring interrupt to occur when the buffer flushes
      if(result == 0){
        K_WRITE_REG(Config_Interrupt,2);
      }

   // *((unsigned long *)arg)=buff_queue[0].u_buffer_addr;
      
      //Copy back the first DMA buffer to user
      if( copy_to_user((int *) arg, &(buff_queue[0].u_buffer_addr), sizeof(unsigned int))){
        printk(KERN_ALERT "copy_to_user failed\n");
      }
      //Set flag to indicate that DMA has been mapped to kernel and should be freed in close
      k2.dma_mapped = 1;
      
      break;
		}
    
    case START_DMA:
    {
      
      //Get size of buffer to be processed
      if(copy_from_user(&(buff_queue[k2.fill].count), (int __user *)arg, sizeof(unsigned int))){
        printk(KERN_WARNING "Error in copy from user\n");
      }

      //Call processing function
      init_transfer();
      
      //*((unsigned long*)arg)=buff_queue[k2.fill].u_buffer_addr;

      //Copy back in arg the address of next buffer to be filled
      if(copy_to_user((unsigned int *)arg, &(buff_queue[k2.fill].u_buffer_addr), sizeof(unsigned int)))
        printk(KERN_ALERT "copy_to_user failed \n");
     
      break;
		}
    
    default:
    {
      break;
    }
	} 
	return 0;
}
struct pci_device_id kyouko2_dev_ids[] = {
  {
    PCI_DEVICE(PCI_VENDOR_ID_CCORSI, PCI_DEVICE_ID_KYOUKO2)
  },
  {0}
};

int kyouko2_probe(struct pci_dev *pci_dev, const struct pci_device_id  *pci_id){
  //Getting physical address of control registers and RAM
  k2.p_control_base = pci_resource_start(pci_dev,1);
  k2.p_ram_base = pci_resource_start(pci_dev,2);

  //Getting length of control registers and RAM
  k2.controlLen = pci_resource_len(pci_dev, 1);
  k2.ramLen = pci_resource_len(pci_dev, 2);

  //Storing pci_dev struct
  k2.dev = pci_dev;

  //Enable Kyouko2 card
  if(pci_enable_device(pci_dev)){
    printk(KERN_WARNING "Error in enabling PCI_DEVICE\n");
  }

  //Sets Kyouko2 as DMA master device
  pci_set_master(pci_dev);

	
  return 0;
}

struct pci_driver kyouko2_pci_dev ={
  .name = "kyouko2",
  .id_table = kyouko2_dev_ids,
  .probe = kyouko2_probe,
};

int kyouko2_release(struct inode *inode, struct file *fp){
	int i;
  int intr;

  //Print buffers drawn  
  printk(KERN_ALERT "Buffers drawn:%d\n", draino);

  //Print interrupt status on exit
  intr = K_READ_REG(Info_Status);
  printk(KERN_ALERT "Interrupt on exit: %x\n", intr);

  //Disable interrupt handler
  free_irq(k2.dev->irq,&k2);

  //Turn off MSI interrupts
  pci_disable_msi(k2.dev);

  //Free kernel DMA buffers
  if(k2.dma_mapped) {
    for(i=0;i<NUM_BUFFER;++i) {
      pci_free_consistent(k2.dev, BUFFER_SIZE*1024, buff_queue[i].k_dma_base, buff_queue[i].p_dma_base);
    }
  }
  
  //Free kernel RAM and control register address
  iounmap(k2.k_ram_base);		
  iounmap(k2.k_control_base);
  
  //Disable DMA master
  pci_clear_master(k2.dev);

  //Disable Kyouko2 pci device
  pci_disable_device(k2.dev);
  printk(KERN_ALERT "BUUH BYE\n");
  
  return 0;
}

struct file_operations kyouko2_fops  = {
  .open = kyouko2_open,
  .release = kyouko2_release,
  .unlocked_ioctl = kyouko2_ioctl,
  .mmap = kyouko2_mmap,
  .owner  =  THIS_MODULE
};

int initf(void){
  //register the character device
  cdev_init(&kyouko2_cdev, &kyouko2_fops);
  cdev_add(&kyouko2_cdev,MKDEV(500,127),1);
  
  //scans the pci bus for the kyouko2 card
  if(pci_register_driver(&kyouko2_pci_dev)){
	  printk(KERN_WARNING "Error in registering device\n");
  }

  printk(KERN_ALERT "kyoko2 Initialized\n");
  return 0;
}

void exitf(void){
  pci_unregister_driver(&kyouko2_pci_dev);

  cdev_del(&kyouko2_cdev);
  printk(KERN_ALERT "Kyouko2 exited\n");
}

module_init(initf);
module_exit(exitf);
