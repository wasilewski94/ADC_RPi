#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/types.h>

#define IOCTL_TYPE (253)
#define ADC_SET		_IOW(IOCTL_TYPE, 1, unsigned long)
#define ADC_START	_IOR(IOCTL_TYPE,2, __u8)
#define ADC_STOP	_IO(IOCTL_TYPE,3)

extern int errno;
  
static const char *dev = "/dev/kw_adc0";

 
int main(int argc, char* argv[]) {
    const char *file_path;
        file_path = dev;
	
    int fd = open(file_path, O_RDWR);
  
    if(fd < 0) {
      printf("Cannot open device %s!%d\n", file_path, fd);
      printf("Value of errno: %d\n", errno);
      perror("open perror:");
      exit(1);
    }
      
    close(fd);
    return 0;
  }
