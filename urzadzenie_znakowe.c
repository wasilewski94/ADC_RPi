
static int my_init(void)
{
int res;
printk(KERN_ALERT "Witam serdecznie\n");
res = register_chrdev(0,DEVICE_NAME,&fops);
if(res<0) {
printk (KERN_ALERT "The device %s
registration failed: %d.\n",
DEVICE_NAME,
res);
goto err1;
}
my_dev = res;
printk (KERN_ALERT "Registration is a
success. The major device number
for %s is %d.\n",
DEVICE_NAME,
my_dev);
return SUCCESS;
err1:
my_cleanup();
return res;
}

static void my_cleanup(void)
{
printk(KERN_ALERT "Do
widzenia\n");
if (my_dev) {
unregister_chrdev(my_dev,DEVIC
E_NAME);
my_dev=0;
}
}
