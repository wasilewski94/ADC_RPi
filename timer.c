#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/hrtimer.h>
#include <linux/poll.h>
#include <asm/atomic.h>
#include <linux/kfifo.h>


#define IOCTL_TYPE (253)
#define ADC_SET		_IOW(IOCTL_TYPE, 1, unsigned long)
#define ADC_START	_IOR(IOCTL_TYPE, 2, __u8)
#define ADC_STOP	_IO(IOCTL_TYPE,3)

#define DEVICE_NAME "kw_adc"

/* fifo size in elements (bytes) */
#define FIFO_SIZE	32

DECLARE_WAIT_QUEUE_HEAD (read_queue);

//ADC buffer kfifo
struct kfifo adc_kfifo;
DEFINE_SPINLOCK(adc_kfifo_lock);
atomic_t daq_flag = ATOMIC_INIT(0);
unsigned char *kfifo_buffer; 

struct max1202 {
  struct spi_transfer	   adc1_xfer[1];
  struct spi_message	   adc1_msg[1];
  struct cdev              *my_cdev;
  u8			   tx[3];
  u8			   rx[3];
  struct hrtimer 	   adc_timer;
  struct spi_device 	   *adc_dev;
  ktime_t  		   adc_sampling_period;
//   struct kfifo 		   adc_kfifo; //It seems, that the buffer must be kmalloc allocated.
};
#define to_max1202_dev(d) container_of(d, struct max1202, dev)

struct max1202 *dev; // dev pointer

dev_t my_dev=0;
#define PROC_BLOCK_SIZE (3*1024)
static struct class *kw_adc_class = NULL;

int kw_adc_open(struct inode *inode, struct file *filp)
{
 printk("funkcja open zostala wywolana");
  
 //todo sprawdzic czy nie byl juz otwarty
//   printk("alokowanie bufora kfifo");
//     printk("udalo sie zaalokowac kfifo");
  return 0;
}

// SPI completion routines
void adc_complete(void * context)
{
  if(atomic_dec_and_test(&daq_flag)) {
    //If the result is zero, then both SPI messages are serviced
    //We may copy the results
    //I do not clean up the bits returned by MCP3201
    //The user application has to do it...
    //adc_buf[0] |= 0x80;
    if(dev->rx[0] != 0)
      printk("MCP3201 error - first byte of transfer is not 0, ignoring results.");
    else {
      kfifo_in(adc_kfifo, (dev->rx+1), 2);
    //We should check for the free space... I'll correct it later...
      wake_up_interruptible(&read_queue);
    }
  }
}

//HRtimer interrupt routine
enum hrtimer_restart adc_timer_proc(struct hrtimer *my_timer)
{
  printk(KERN_ALERT "obsluga przerwania timera");  
  
  struct max1202 *dev = container_of(my_timer, struct max1202, adc_timer);
  if (dev == NULL)
     return 0;
  
  //initialization of the spi_messages
  spi_message_init(dev->adc1_msg);
  memset(dev->adc1_xfer, 0, sizeof(dev->adc1_xfer));
  dev->adc1_xfer->tx_buf  = dev->tx;
  dev->adc1_xfer->rx_buf  = dev->rx;
  dev->adc1_xfer->len = 3;
  spi_message_add_tail(dev->adc1_xfer, dev->adc1_msg);
  dev->adc1_msg->complete = adc_complete;
  //submission of the messages
  spi_async(dev->adc_dev, dev->adc1_msg);
  //mark the fact, that messages are submited
  atomic_set(&daq_flag,1);
  //rearming of the timer
  hrtimer_forward(my_timer, my_timer->_softexpires, dev->adc_sampling_period);
  return HRTIMER_RESTART;
} 


int kw_adc_release(struct inode *inode, struct file *filp)
{
  //Make sure, that daq_timer is already switched off
  hrtimer_try_to_cancel(&dev->adc_timer);
  //Make sure that the spi_messages are serviced
  while (atomic_read(&daq_flag)) {};
  //Now we can be sure, that the spi_messages are serviced
//   kfifo_free(&adc_kfifo);
  return 0;
}


static int kw_adc_probe(struct spi_device *spi)
{	
	printk(KERN_ALERT "Probe startuje!");
	int retval = -ENOMEM;
	struct max1202 *dev;
	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		goto error;

	spi->max_speed_hz = 2000000;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	
	retval = spi_setup(spi);
	if (retval < 0) {
	    printk(KERN_ALERT "<1> Nie udalo sie ustawic parametrow urzadzenia!\n");
	    return retval;
	}
	dev->adc_dev = spi;
	return 0;
error:
	return retval;
}

static int kw_adc_remove(struct spi_device *spi)
{
	return 0;
}

static const struct of_device_id spidev_dt_ids[] = {           
  
	{ .compatible = "kw_adc" },
        {}
};

static struct spi_driver spidev_kw_adc_driver = {
	.driver	= {
		.name		= "kw_adc",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(spidev_dt_ids),
	},
	.probe		= kw_adc_probe,
	.remove		= kw_adc_remove,
};

//module_spi_driver(spidev_kw_adc_driver);

static long kw_adc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
printk(KERN_ALERT "wywolano funkcje adc_ioctl()");

//struct max1202 *dev;
//dev = file->private_data;  
if (dev == NULL)
    return -ENODEV;


int retval=0;
int err = 0;	


if (_IOC_TYPE(cmd) != IOCTL_TYPE)
	return -ENOTTY;
if (_IOC_DIR(cmd) & _IOC_READ)
	err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
else if (_IOC_DIR(cmd) & _IOC_WRITE)
	err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
if (err)
	return -EFAULT;

switch(cmd){
 
  case ADC_SET: //Set sampling frequency
     dev->adc_sampling_period =  ktime_set(0, arg);
     hrtimer_init(&dev->adc_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
     dev->adc_timer.function = adc_timer_proc;
     printk(KERN_ALERT "ustawiono okres probkowania: %lu", arg);
     //file->private_data = dev;
     return 0; 
  
  case ADC_START:
    hrtimer_start(&dev->adc_timer, dev->adc_sampling_period, HRTIMER_MODE_REL);
    printk(KERN_ALERT "rozpoczecie konwersji");
    //file->private_data = dev;
    return 0;
    
   case ADC_STOP: //Stop acquisition
   hrtimer_try_to_cancel(&dev->adc_timer);
   printk(KERN_ALERT "koniec konwersji!");
   //file->private_data = dev;
   return 0;
   
        return retval;  
    }
    
return 0; 
}

ssize_t kw_adc_read (struct file * filp, char __user * buff, size_t count, loff_t * offp)
{/*
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
    
    kfifo_out_spinlocked(adc_kfifo, tmp, 4, &adc_kfifo_lock);
    copy_to_user(buff+copied,tmp,4);
    //copy_to_user return copied bytes count
    copied+=4;
  }
  
  return copied;
*/
  return 0;
}

const struct file_operations kw_adc_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= kw_adc_ioctl,
	.read           = kw_adc_read,
	.open		= kw_adc_open,
	.release	= kw_adc_release,
};

static void kw_adc_cleanup(void)
{
   printk("Sprzatam po sobie bom zabrudzil");
   /* Usuwamy urządzenie z klasy */
   if(my_dev && kw_adc_class) {
     device_destroy(kw_adc_class,my_dev);
   }
   /* Usuwamy strukturę urządzenia z systemu*/
   if(dev->my_cdev) { 
     cdev_del(dev->my_cdev);
     dev->my_cdev=NULL;
   }
   /* Zwalniamy numer urządzenia */
   if(my_dev) {
     unregister_chrdev_region(my_dev, 1);
   }
   /* Wyrejestrowujemy klasę */
   if(kw_adc_class) {
     class_destroy(kw_adc_class);
     kw_adc_class=NULL;
   }
   printk(KERN_ALERT "do widzenia");
   kfifo_free(&adc_kfifo);  
   if(dev) {
     kfree(dev);
     dev = NULL;
   }
}

static int kw_adc_init(void)
{
  printk(KERN_ALERT "Init startuje!");
  int res=0; 
  dev = kmalloc(sizeof(*dev), GFP_KERNEL | __GFP_ZERO);
  if (!dev)
    return 0;  
  
  dev->tx[0] = 0x9f;
  dev->tx[1] = 0;
  dev->tx[2] = 0;
   
  /* Alokujemy numer urządzenia */
  res=alloc_chrdev_region(&my_dev, 0, 1, DEVICE_NAME);
  if(res) {
    printk (KERN_ALERT "Alocation of the device number for %s failed\n",
            DEVICE_NAME);
    return res; 
  };
  
  /* Tworzymy klasę opisującą nasze urządzenie - aby móc współpracować z systemem udev */
  kw_adc_class = class_create(THIS_MODULE, DEVICE_NAME);
  if (IS_ERR(kw_adc_class)) {
    printk(KERN_ERR "Error creating kw_adc_class.\n");
    res=PTR_ERR(kw_adc_class);
    goto err1;
  }
  
 /* Alokujemy strukturę opisującą urządzenie znakowe */
  dev->my_cdev = cdev_alloc();
  dev->my_cdev->ops = &kw_adc_fops;
  dev->my_cdev->owner = THIS_MODULE;
  
  /* Dodajemy urządzenie znakowe do systemu */
  res=cdev_add(dev->my_cdev, my_dev, 1);
  
  if(res) {
    printk (KERN_ALERT "Registration of the device number for %s failed\n",
            DEVICE_NAME);
    res=-EFAULT;
    goto err1;
  };
  
  device_create(kw_adc_class,NULL,my_dev,NULL,"%s%d", DEVICE_NAME, MINOR(my_dev));
  printk (KERN_ALERT "%s The major device number is %d.\n",
 "Registeration is a success.",
    MAJOR(my_dev));
   printk(KERN_ALERT "Zaladowano moj modul");
   res = spi_register_driver(&spidev_kw_adc_driver);
  
    //todo sprawdzic czy nie byl juz otwarty
  printk("alokowanie bufora kfifo");
  int ret = kfifo_alloc(&adc_kfifo, FIFO_SIZE, GFP_KERNEL);//todo czy  nie wyciek pamieci
  
  if (ret) {
    printk("nie udalo sie zaalokowac kfifo");
    return ret;
  }
   
   return res;
 err1:
  kw_adc_cleanup();
  return res;
  
}
module_init(kw_adc_init);

static void kw_adc_exit(void)
{
	kw_adc_cleanup();
	spi_unregister_driver(&spidev_kw_adc_driver);
}
module_exit(kw_adc_exit);


//MODULE_DEVICE_TABLE(of, of_match_ptr(spidev_dt_ids)); 
/* Information about this module */
MODULE_DESCRIPTION("Minimal SPI ADC driver");
MODULE_AUTHOR("Krzysztof Wasilewski");
// MODULE_LICENSE("GPL v2");
MODULE_LICENSE("Dual BSD/GPL");
