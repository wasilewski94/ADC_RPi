/*
  This is a simple, quick&dirty driver working with two MCP3201 converters connected to the SPI bus
  Written by Wojciech M. Zabolotny (wzab@ise.pw.edu.pl).
  The code was significantly based on many different examples of Linux device drivers...
*/

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/kfifo.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/io.h>Wyświetla wyniki dla %2.
#include <asm/arch/at32ap700x.h>
#include <asm/arch/board.h>
#include <asm/arch/portmux.h>

#define DEVICE_NAME "wzdaq"
//Static variables
ktime_t daq_sampling_period;
struct spi_transfer     adc1_xfer, adc2_xfer;
struct spi_message      adc1_msg, adc2_msg;
unsigned char *adc_buf;
unsigned char button;
//ADC buffer kfifo
struct kfifo *adc_kfifo = NULL; //It seems, that the buffer mast be kmalloc allocated.
spinlock_t adc_kfifo_lock = SPIN_LOCK_UNLOCKED;
atomic_t daq_flag = ATOMIC_INIT(0);
//Imported from the wzspi module
struct spi_device extern * spi_daq_devs[4];
extern void at32_select_gpio(unsigned int pin, unsigned long flags);
extern int at32_select_gpio_pins(unsigned int port, u32 pins, u32 oe_mask);
// HR timer
struct hrtimer daq_timer;

DECLARE_WAIT_QUEUE_HEAD (read_queue);

// SPI completion routines
void adc_complete(void * context)
{ 
  if(atomic_dec_and_test(&daq_flag)) {
    //If the result is zero, then both SPI messages are serviced
    //We may copy the results
    if (0) printk("<1>status: %d %d %2.2x %2.2x %2.2x %2.2x\n",
		  adc1_msg.status,adc2_msg.status,
		  (int) adc_buf[0], (int) adc_buf[1],
		  (int) adc_buf[2], (int) adc_buf[3] );
    //I do not clean up the bits returned by MCP3201
    //The user application has to do it...
    if(button)
      adc_buf[0] |= 0x80;
    else
      adc_buf[0] &= ~0x80;
    kfifo_put(adc_kfifo,adc_buf,4); //We should check for the free space... I'll correct it later...
    wake_up_interruptible(&read_queue);
  }
}

//HRtimer interrupt routine
enum hrtimer_restart daq_timer_proc(struct hrtimer *my_timer)
{
  //initialization of the spi_messages
  spi_message_init(&adc1_msg);
  adc1_msg.is_dma_mapped = 0;
  spi_message_init(&adc2_msg);
  adc2_msg.is_dma_mapped = 0;
  memset(&adc1_xfer, 0, sizeof(adc1_xfer));
  memset(&adc2_xfer, 0, sizeof(adc2_xfer));
  adc1_xfer.tx_buf  = NULL;
  adc1_xfer.rx_buf  = &(adc_buf[0]);
  adc1_xfer.len = 2;
  adc2_xfer.tx_buf  = NULL;
  adc2_xfer.rx_buf  = &(adc_buf[2]);
  adc2_xfer.len = 2;
  spi_message_add_tail(&adc1_xfer, &adc1_msg);
  spi_message_add_tail(&adc2_xfer, &adc2_msg);
  adc1_msg.complete = adc_complete;
  adc2_msg.complete = adc_complete;
  //decode the state of the button
  button = gpio_get_value(GPIO_PIN_PE(12));
  //submission of the messages
  spi_async(spi_daq_devs[0], &adc1_msg);
  spi_async(spi_daq_devs[1], &adc2_msg);
  //mark the fact, that messages are submited
  atomic_set(&daq_flag,2); 
  //rearming of the timer
  hrtimer_forward(my_timer, my_timer->expires, daq_sampling_period);
  return HRTIMER_RESTART;
}  

int wzdaq_open(struct inode *inode, struct file *filp)
{
  if (adc_kfifo) {
    //Device is already opened
    return -EINVAL;
  }
  adc_kfifo = kfifo_alloc(10000,GFP_KERNEL,&adc_kfifo_lock);
  if (!adc_kfifo) {
    return -ENOMEM;
  }
  return 0;
}

int wzdaq_release(struct inode *inode, struct file *filp)
{
  //Make sure, that daq_timer is already switched off
  hrtimer_cancel(&daq_timer);
  //Make sure that the spi_messages are serviced
  while (atomic_read(&daq_flag)) {};
  //Now we can be sure, that the spi_messages are serviced
  kfifo_free(adc_kfifo);
  adc_kfifo=NULL;
  return 0;
}

ssize_t wzdaq_read (struct file * filp, char __user * buff, size_t count, loff_t * offp)
{
  char tmp[4];
  int copied=0;
  int res;
  //Unfortunately we do not have the userspace access to the kfifo data
  //I hope one day we will have kfifo_put_user in the mainstream kernel... ;-)
  //Therefore we have to loop through the kfifo :-(
  if (count % 4) return -EINVAL; //only 4-byte access
  while(count>0) {
    res=wait_event_interruptible(read_queue,kfifo_len(adc_kfifo) >= 4);
    if(res) return res; //We have received a signal...
    count -= 4;
    kfifo_get(adc_kfifo,tmp,4);
    copy_to_user(buff+copied,tmp,4);
    copied+=4;
  }
  return copied;
}

/* The ioctl commands should be assigned according to LDD3 chapter 6
   however for this "quick&dirty" example I have decided just
   tu use 3 arbitrary selected values */

int wzdaq_ioctl (struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
  switch(cmd) {
  case 0xa530: //Set sampling frequency
    daq_sampling_period =  ktime_set(0,arg);
    return 0;
  case 0xa531: //Start acquisition
    //If acquisition is on - give up
    // Create the timer
    hrtimer_init(&daq_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    daq_timer.function = daq_timer_proc;
    hrtimer_start(&daq_timer, daq_sampling_period,HRTIMER_MODE_REL);
    return 0;
  case 0xa532: //Stop acquisition
    hrtimer_cancel(&daq_timer);
    return 0;
  }
  return -EINVAL;
}

unsigned int wzdaq_poll(struct file *filp,poll_table *wait)
{
  unsigned int mask =0;
  poll_wait(filp,&read_queue,wait);
  if(kfifo_len(adc_kfifo)>=4) mask |= POLLIN |POLLRDNORM;
  return mask;
}


struct file_operations daq_fops = {
  .owner=THIS_MODULE,
  .read=wzdaq_read,
  .open=wzdaq_open,
  .ioctl=wzdaq_ioctl,
  .poll=wzdaq_poll,
  .release=wzdaq_release,
};

dev_t my_dev=0;
struct cdev * my_cdev = NULL;
static struct class *class_wzdaq_priv = NULL;

void  clean_up(void)
{
  gpio_free(GPIO_PIN_PE(12));
  kfree(adc_buf);
  if(my_dev && class_wzdaq_priv) {
    class_device_destroy(class_wzdaq_priv,my_dev);
  }
  if(my_cdev) { 
    cdev_del(my_cdev);
    my_cdev=NULL;
  }
  if(my_dev) {
    unregister_chrdev_region(my_dev, 1);
  }
  if(class_wzdaq_priv) {
    class_destroy(class_wzdaq_priv);
    class_wzdaq_priv=NULL;
  }
}

static int __init wzdaq_init(void)
{
  int res=0;
  adc_buf=kmalloc(4,GFP_KERNEL);
  //Check if the spi devices are registred
  if (spi_daq_devs[0]==NULL)
    return -EINVAL;
  if (spi_daq_devs[1]==NULL)
    return -EINVAL;
  //Register the device
  res=alloc_chrdev_region(&my_dev, 0, 1, DEVICE_NAME);
  if(res) {
    printk ("<1>Alocation of the device number for %s failed\n",
            DEVICE_NAME);
    return res; 
  };
  class_wzdaq_priv = class_create(THIS_MODULE, "wzdaq_class");
  if (IS_ERR(class_wzdaq_priv)) {
    printk(KERN_ERR "Error creating rs_class.\n");
    res=PTR_ERR(class_wzdaq_priv);
    goto err1;
  }
  my_cdev = cdev_alloc( );
  my_cdev->ops = &daq_fops;
  my_cdev->owner = THIS_MODULE;
  res=cdev_add(my_cdev, my_dev, 1);
  if(res) {
    printk ("<1>Registration of the device %s failed\n",
            DEVICE_NAME);
    res=-EFAULT;
    goto err1;
  };
  class_device_create(class_wzdaq_priv,NULL,my_dev,NULL,"wzdaq_%d",MINOR(my_dev));
  printk ("<1>%s The major device number is %d.\n",
	  "Registration is a success.",
	  MAJOR(my_dev));
  //Na koniec ustawiamy pin E12 w tryb GPIO z pull_up i filtrem
  if ((res=gpio_request(GPIO_PIN_PE(12),"wzdaq"))) {
    printk("<1> I couldn't reserve the E12 pin! error code: %d",res);
    goto err1;
  }
  //at32_select_gpio(GPIO_PIN_PE(12), AT32_GPIOF_PULLUP | AT32_GPIOF_DEGLITCH);
  //at32_select_gpio_pins(4, 1<<12, 1<<12);
  return res;
 err1:
  clean_up();
  return res;
}
module_init(wzdaq_init);

static void __exit wzdaq_exit(void)
{
  clean_up();
}
module_exit(wzdaq_exit);

/* Information about this module */
MODULE_DESCRIPTION("DAQ system driver working with 2 MCP3201 connected to the SPI bus 1.0 and 1.1");
MODULE_AUTHOR("Wojciech M. Zabolotny, Institute of Electronic Systems");
MODULE_LICENSE("Dual BSD/GPL");
//MODULE_LICENSE("GPL");