#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define SPI_IOC_MAGIC			'k'
/* Read / Write of SPI mode (SPI_MODE_0..SPI_MODE_3) (limited to 8 bits) */
#define SPI_IOC_RD			_IOR(SPI_IOC_MAGIC, 1, __u8)
#define SPI_IOC_WR			_IOW(SPI_IOC_MAGIC, 1, __u8)
#define DEVICE_NAME "kw_adc"

extern int errno;
  
static const char *dev = "/dev/kw_adc0";
static float lsb = 0.001;

 
int main(int argc, char* argv[]) {
    uint8_t bufor[3];
    const char *file_path;
  
    if(argc == 1)
        file_path = dev;
    else
        file_path = argv[1];
  
    int fd = open(file_path, O_RDWR);
  
    if(fd < 0) {
      printf("Cannot open device %s!%d\n", file_path, fd);
      printf("Value of errno: %d\n", errno);
      perror("open perror:");
      exit(1);
    }
  
    /*
   * Pomiar single-ended i unipolar, wybor kanalu:
  format komendy: 
  1XXX1111
   000 - CH0 --> 8f
   100 - CH1 --> cf 
   001 - CH2 --> 9f
   101 - CH3 --> df
   010 - CH4 --> af
   110 - CH5 --> ef
   011 - CH6 --> bf
   111 - CH7 --> ff
   */
  
    
    while(1) {
    bufor[0] = 0x9f;
    bufor[1] = 0;
    bufor[2] = 0;
      
      int ret = ioctl(fd, SPI_IOC_RD, bufor);
      
      signed long value;
      value = bufor[2] >> 3; 
      value |= bufor[1] << 5;
      float volt = value * lsb;
	  
      
      printf("ret:%d  bufor:%d %d %d  wartosc: %f\n", ret, bufor[0], bufor[1], bufor[2], volt);
      if(ret < 0) {
	
          printf("Value of errno: %d\n", errno);
          perror("open perror:");
      }
      sleep(1);
    }
    return 0;
  }
