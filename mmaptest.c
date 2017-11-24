#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>

#define DEVICE_NAME "kw_adc"

struct cdev *my_cdev;
dev_t my_dev=0;
static struct class *kw_adc_class = NULL;


int kw_adc_open(struct inode *inode, struct file *filp)
{
  printk(KERN_ALERT "funkcja open zostala wywolana");
  return 0;
}


static void kw_adc_cleanup(void)
{
   printk("Sprzatam po sobie");
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
}

   const struct file_operations kw_adc_fops = {
	.owner		= THIS_MODULE,
	.open		= kw_adc_open,
};
   
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
  }
  
  /* Tworzymy klasę opisującą nasze urządzenie - aby móc współpracować z systemem udev */
  kw_adc_class = class_create(THIS_MODULE, DEVICE_NAME);
  if (IS_ERR(kw_adc_class)) {
    printk(KERN_ERR "Error creating kw_adc_class.\n");
    res=PTR_ERR(kw_adc_class);
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
  }
  
  device_create(kw_adc_class,NULL,my_dev,NULL,"%s%d", DEVICE_NAME, MINOR(my_dev));
  printk (KERN_ALERT "%s The major device number is %d.\n",
  "Registeration is a success.",
  MAJOR(my_dev));
  printk(KERN_ALERT "Zaladowano moj modul");
  return res;
  }
module_init(kw_adc_init);

static void kw_adc_exit(void)
{
	kw_adc_cleanup();
    printk(KERN_ALERT "Do widzenia");
}
module_exit(kw_adc_exit);


//MODULE_DEVICE_TABLE(of, of_match_ptr(spidev_dt_ids)); 
/* Information about this module */
MODULE_DESCRIPTION("Minimal SPI ADC driver");
MODULE_AUTHOR("Krzysztof Wasilewski");
MODULE_LICENSE("Dual BSD/GPL");
