#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/spinlock.h>
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


#define SPI_IOC_MAGIC			'k'
/* Read / Write of SPI mode (SPI_MODE_0..SPI_MODE_3) (limited to 8 bits) */
#define SPI_IOC_RD			_IOR(SPI_IOC_MAGIC, 1, __u8)
#define SPI_IOC_WR			_IOW(SPI_IOC_MAGIC, 1, __u8)
#define DEVICE_NAME "kw_adc"


struct kw_adc {

	struct spi_transfer	xfer[1];
	struct spi_message	msg_buf[1];
	spinlock_t		spi_lock;
	struct spi_device	*spi;
	struct device 		*spidev;
	struct list_head	device_entry;
	spinlock_t		adc_kfifo_lock;
	struct hrtimer	   	daq_timer;
	ktime_t		  	daq_sampling_period;
	struct kfifo 		*adc_kfifo;
	/* TX/RX buffers are NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	u8			tx[3];
	u8			rx[3];
};

static int kw_adc_open(struct inode *inode, struct file *file)
{       
  struct kw_adc *dev;
  int retval = 0;
  int subminor;
   
  subminor = iminor(inode); 
  
  printk(KERN_ALERT "wywolano funkcje kw_adc_open");
  dev = kmalloc(sizeof(*dev), GFP_KERNEL);
  if (!dev){
	retval = -ENODEV;
	goto exit;
  } 
   
/*  
 if (dev->adc_kfifo) {
    //Device is already opened
    return -EINVAL;
  }
  dev->adc_kfifo = kfifo_alloc(10000, GFP_KERNEL, &adc_kfifo_lock);
  if (!dev->adc_kfifo) {
    return -ENOMEM;
  }*/
 
 /* save our object in the file's private structure */
 file->private_data = dev;
  
exit:
	return retval;  
}

static int kw_adc_release(struct inode *inode, struct file *file)
{
  struct kw_adc *dev;
  dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;
  return 0;
}

static int kw_adc_probe(struct spi_device *spi)
{	
     printk(KERN_ALERT "Probe startuje!\n");
	struct kw_adc *dev;
	
	int err = 0;
	int retval = -ENOMEM;
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
	
	dev->spi = spi;
        dev->spi = &spi->dev;
	dev->rx[0] = 0;
	dev->rx[1] = 0;
	dev->rx[2] = 0;
	
	dev->tx[0] = 0x9f;
	dev->tx[1] = 0;
	dev->tx[2] = 0;
	
	dev->xfer[0].rx_buf = &dev->rx[0];
	dev->xfer[0].tx_buf = &dev->tx[0];
	dev->xfer[0].len = 3;
      
        dev_set_drvdata(&spi->dev, dev);
        dev_info(&spi->dev, "spi registered, item=0x%p\n", (void *)dev);
	
	spi_message_init(&dev->msg_buf[0]);
	spi_message_add_tail(&dev->xfer[0], &dev->msg_buf[0]);

	retval = spi_sync_transfer(dev, dev->msg_buf, 1);
	printk(KERN_ALERT "wykonano transfer!");
	printk("tx=%x %x %x\n", dev->tx[0], dev->tx[1], dev->tx[2]);
	printk("rx=%x %x %x\n", dev->rx[0], dev->rx[1], dev->rx[2]);
	
	struct spi_transfer *t = kmalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if(!t)
	   goto error;
	
	//tablica transferow
	struct spi_transfer *msg_buf = NULL;
	
	msg_buf = kmalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!msg_buf)
	    return -ENOMEM;
	
	
	/* save our data pointer in this interface device */
	spi_set_drvdata(spi, dev);
	dev->spi = spi;
	
	spi_message_add_tail(&t, &msg_buf);
	retval = spi_async(dev->spi, &dev->msg_buf);
	
	//wysylam do przestrzeni uzytkownika adres bufora
	retval = __copy_to_user((uint8_t __user *)arg, rx, 3);
	
	printk(KERN_ALERT "wykonano transfer!");
	printk("tx=%x %x %x\n",dev->tx[0], dev->tx[1], dev->tx[2]);
	printk("rx=%x %x %x\n",dev->rx[0], dev->rx[1], dev->rx[2]);

	printk(KERN_ALERT "wpisanie wartosci rx!");
	return 0;

	error:
	if (dev)
	  kfree(dev);
	if(t)
	  kfree(t);
	if(msg_buf)
	  kfree(msg_buf);
	
	return retval;
}


static int kw_adc_remove(struct spi_device *spi)
{
	dev_notice(&spi->dev, "remove() called\n");
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

static long kw_adc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
int retval = -ENOMEM;
int err = 0;
struct kw_adc *dev;
printk(KERN_ALERT "wywolano funkcje adc_ioctl()");

    
if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
	return -ENOTTY;
if (_IOC_DIR(cmd) & _IOC_READ)
	err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
else if (_IOC_DIR(cmd) & _IOC_WRITE)
	err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
if (err)
	return -EFAULT;

dev =file->private_data;
if(!dev) { 
    printk( KERN_NOTICE "Could not open device.");
    return -ENOTTY;
    }  
    
if (cmd == SPI_IOC_RD){
	/*
	msg_buf[0].tx_buf=tx;
	printk(KERN_ALERT "wpisanie wartosci tx");
	msg_buf[0].rx_buf=rx;
	printk(KERN_ALERT "wpisanie wartosci rx!");
	msg_buf[0].len = 3;
	printk(KERN_ALERT "wpisanie wartosci len!");
	
	retval = __copy_from_user(tx, (uint8_t __user *)arg, 3);
	spi_message_add_tail(&t, &msg_buf);
	retval = spi_async(dev->spi, &dev->msg_buf);
	printk(KERN_ALERT "wykonano transfer!");
	printk("tx=%x %x %x\n",tx[0], tx[1], tx[2]);
	printk("rx=%x %x %x\n",rx[0], rx[1], rx[2]);
	
	printk(KERN_ALERT "wpisanie wartosci rx!");
	
	//wysylam do przestrzeni uzytkownika adres bufora
	retval = __copy_to_user((uint8_t __user *)arg, rx, 3);
	
	//czyszcze pamiec
	  if(rx)
	    kfree(rx);
	  if(tx)
	    kfree(tx);
	  if(t)
	    kfree(t);
	  if(msg_buf)
	    kfree(msg_buf);
	
	
	if (retval < 0) {
	printk("spi_sync_transfer failed!\n");
	
	err:
	  if(rx)
	    kfree(rx);
	  if(tx)
	    kfree(tx);
	  if(t)
	    kfree(t);
	  if(msg_buf)
	    kfree(msg_buf);
	
	return -ENOMEM;
	}
	*/
        return retval;  
}

}

const struct file_operations kw_adc_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= kw_adc_ioctl,
	.open		= kw_adc_open,
	.release	= kw_adc_release,
};

static void kw_adc_cleanup(void)
{
  printk(KERN_ALERT "do widzenia");
}

static int kw_adc_init(void)
{
  int res=0;
  printk(KERN_ALERT "Init startuje!");
   res = spi_register_driver(&spidev_kw_adc_driver);
  return res;
}
module_init(kw_adc_init);

static void __exit kw_adc_exit(void)
{
	kw_adc_cleanup();
	spi_unregister_driver(&spidev_kw_adc_driver);
}
module_exit(kw_adc_exit);

/* Information about this module */
MODULE_DESCRIPTION("Minimal SPI ADC driver");
MODULE_AUTHOR("Krzysztof Wasilewski");
MODULE_LICENSE("GPL v2");

