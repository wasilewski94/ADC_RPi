    #include <linux/kernel.h>
    #include <linux/module.h>
    #include <linux/init.h>
    #include <linux/workqueue.h>
    #include <linux/spi/spi.h>
     
    struct kw_adc {
        struct delayed_work work;
        struct device *dev;
        struct spi_device *spidev;
        unsigned char bufor;
        //unsigned char direction;
    };
     
    struct workqueue_struct *workqueue;
     
    static void kw_adc_loop( struct delayed_work *ptr )
    {
        struct kw_adc *item;
        item = (struct kw_adc *)ptr;
     
	    item->bufor = 0x9f;
//             item->direction = 'd';
//         else if (item->bufor == 1)
//             item->direction = 'u';
//      
//         if (item->direction == 'u')
//             item->bufor <<=1;
//         else if (item->direction == 'd')
//             item->bufor >>=1;
     
        spi_write(item->spidev,&(item->bufor),1);
     
        queue_delayed_work(workqueue,ptr,10);
    }
     
    static int __init kw_adc_probe(struct spi_device *dev)
    {
        int ret = 0;
        struct kw_adc *item;
     
        printk("kw_adc probe started\n");
     
        item = kzalloc(sizeof(struct kw_adc), GFP_KERNEL);
        if (!item) {
            dev_err(&dev->dev,
                    "%s: unable to kzalloc for kw_adc\n", __func__);
            ret = -ENOMEM;
            goto out;
        }
     
        item->spidev = dev;
        item->dev = &dev->dev;
        //item->direction ='u';
        //item->bufor = 1;
        dev_set_drvdata(&dev->dev, item);
        dev_info(&dev->dev, "spi registered, item=0x%p\n", (void *)item);
     
        INIT_DELAYED_WORK(&(item->work),kw_adc_loop);
        queue_delayed_work(workqueue,&(item->work),10);
     
    out:
        return ret;
    }
     
    static int kw_adc_remove(struct spi_device *spi)
    {
        /*should be implemented but I'm lazy and this is just a test */
        return 0;
    }
    /* 
    static int kw_adc_suspend(struct spi_device *spi, pm_message_t state)
    {
        should be implemented but I'm lazy and this is just a test 
        return 0;
    }
     
    static int kw_adc_resume(struct spi_device *spi)
    {
        should be implemented but I'm lazy and this is just a test 
        return 0;
    }
    */
    
    
    static const struct of_device_id spidev_dt_ids[] = {                                                                                       
        { .compatible = "kw_adc" },
        {}
};
    
    static struct spi_driver kw_adc_driver = {
        .driver = {
            .name   = "kw_adc",
            .bus    = &spi_bus_type,
            .owner  = THIS_MODULE,
        },
        .probe = kw_adc_probe,
        .remove = kw_adc_remove,
//         .suspend = kw_adc_suspend,
//         .resume = kw_adc_resume,
    };
     
    static int __init kw_adc_init( void )
    {
        int ret = 0;
     
        printk("kw_adc module installing\n");
     
        workqueue = create_workqueue("kw_adc queue");
        if (workqueue == NULL){
            pr_err("%s: unable to create workqueue\n", __func__);
            ret = -1;
            goto out;
        }
     
     
        ret = spi_register_driver(&kw_adc_driver);
        if (ret)
            pr_err("%s: problem at spi_register_driver\n", __func__);
     
    out:
        return ret;
    }
    module_init(kw_adc_init);
     
    static void __exit kw_adc_exit( void )
    {
        /*should be implemented but I'm lazy and this is just a test */
    }
    module_exit(kw_adc_exit);
     
    MODULE_LICENSE("GPL v2");
    MODULE_AUTHOR("Tuomas Nylund");
    MODULE_DESCRIPTION("kw_adc");
    MODULE_VERSION("0.1");
     