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
#include <linux/spi/spidev.h>


#define SPI_IOC_MAGIC			'k'
/* Read / Write of SPI mode (SPI_MODE_0..SPI_MODE_3) (limited to 8 bits) */
#define SPI_IOC_RD			_IOR(SPI_IOC_MAGIC, 1, __u8)
#define SPI_IOC_WR			_IOW(SPI_IOC_MAGIC, 1, __u8)
#define DEVICE_NAME "kw_adc"

dev_t my_dev=0;
struct cdev * my_cdev = NULL;
#define PROC_BLOCK_SIZE (3*1024)
static struct class *kw_adc_class = NULL;
static float lsb = 0.001;

struct spi_device *spi_adc_dev = NULL;
// struct spi_transfer * adc_message = NULL;
// EXPORT_SYMBOL(adc_message);
EXPORT_SYMBOL(spi_adc_dev);

static int kw_adc_open(struct inode *inode, struct file *file)
{
  printk(KERN_ALERT "wywolano funkcje kw_adc_open");
  return 0;
}

static int kw_adc_release(struct inode *inode, struct file *file)
{
  printk(KERN_ALERT "no to na razie");
	return 0;
}

static int kw_adc_probe(struct spi_device *spi)
{	
	// w funkcji probe musimy umiec rozroznic urzadzenia ktore sa podlaczone
	// beda one mialy inne numer minor
	
  
	printk(KERN_ALERT "Probe startuje!");
	int retval = -ENOMEM;
	
	spi->max_speed_hz = 2000000;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	
	retval = spi_setup(spi);
	if (retval < 0) {
	    printk(KERN_ALERT "<1> Nie udalo sie ustawic parametrow urzadzenia!\n");
	    return retval;
	}
	
	/* Store the reference to the spi_device */
	spi_adc_dev = spi;
	return 0;
}


static int kw_adc_remove(struct spi_device *spi)
{
	spi_adc_dev = NULL;
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
printk(KERN_ALERT "wywolano funkcje adc_ioctl()");
int retval = -ENOMEM;
int err = 0;

if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
	return -ENOTTY;
if (_IOC_DIR(cmd) & _IOC_READ)
	err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
else if (_IOC_DIR(cmd) & _IOC_WRITE)
	err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
if (err)
	return -EFAULT;
	
	
if (cmd == SPI_IOC_RD){
	  
	uint8_t* tx = kmalloc(3, GFP_KERNEL);
	if(!tx)
	   goto err;

	printk(KERN_ALERT "alokacja tx!");
	
	uint8_t* rx = kmalloc(3, GFP_KERNEL);
	if(!rx)
	   goto err; 
	
	printk(KERN_ALERT "alokacja rx!");
	
	tx[0] = 143;
	tx[1] = 0x00;
	tx[2] = 0x00;
	printk(KERN_ALERT "wpisano wartosci do bufora tx!");
	
	rx[0] = 0x00;
	rx[1] = 0x00;
	rx[2] = 0x00;
	printk(KERN_ALERT "wpisano wartosci do bufora rx!");
	
	//definicja struktury spi_transfer
	struct spi_transfer *t = kmalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if(!t)
	   goto err;
	
	printk(KERN_ALERT "alokacja transferu!");
	
	//tablica transferow
	struct spi_transfer *msg_buf = NULL;
	
	msg_buf = kmalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!msg_buf)
	    return -ENOMEM;
	
	printk(KERN_ALERT "alokacja tablicy transferow!");
		
	msg_buf[0].tx_buf=tx;
	printk(KERN_ALERT "wpisanie wartosci tx");
	msg_buf[0].rx_buf=rx;
	printk(KERN_ALERT "wpisanie wartosci rx!");
	msg_buf[0].len = 3;
	printk(KERN_ALERT "wpisanie wartosci len!");
	
	
	retval = spi_sync_transfer(spi_adc_dev, msg_buf, 1);
	printk(KERN_ALERT "wykonano transfer!");
	printk("rx=%x %x %x\n",rx[0], rx[1], rx[2]);
	
	printk(KERN_ALERT "wpisanie wartosci rx!");
	
	//wysylam do przestrzeni uzytkownika adres bufora
	retval = __copy_to_user((uint8_t __user *)arg, rx, 3);
	
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
        return retval;  
}
   
	//mutex_unlock(&spiadc->buf_lock);
	//spi_dev_put(spi);
	
}

const struct file_operations kw_adc_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= kw_adc_ioctl,
	.open		= kw_adc_open,
	.release	= kw_adc_release,
};


static void kw_adc_cleanup(void)
{
  /* Usuwamy urządzenie z klasy */
  if(my_dev && kw_adc_class) {
    device_destroy(kw_adc_class,my_dev);
  }
  /* Usuwamy strukturę urządzenia z systemu*/
  if(my_cdev) { 
    cdev_del(my_cdev);
    my_cdev=NULL;
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

}

static int kw_adc_init(void)
{
  printk(KERN_ALERT "Init startuje!");
  int res=0;

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
  my_cdev = cdev_alloc();
  my_cdev->ops = &kw_adc_fops;
  my_cdev->owner = THIS_MODULE;
  
  /* Dodajemy urządzenie znakowe do systemu */
  res=cdev_add(my_cdev, my_dev, 1);
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
  return res;
 err1:
  kw_adc_cleanup();
  return res;
  
}
module_init(kw_adc_init);

static void __exit kw_adc_exit(void)
{
	kw_adc_cleanup();
	spi_unregister_driver(&spidev_kw_adc_driver);
}
module_exit(kw_adc_exit);


//MODULE_DEVICE_TABLE(of, of_match_ptr(spidev_dt_ids)); 
/* Information about this module */
MODULE_DESCRIPTION("Minimal SPI ADC driver");
MODULE_AUTHOR("Krzysztof Wasilewski");
MODULE_LICENSE("GPL v2");

















