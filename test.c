#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/time.h>

#define IOCTL_TYPE (253)
#define ADC_SET		_IOW(IOCTL_TYPE, 1, unsigned long)
#define ADC_START	_IOR(IOCTL_TYPE,2, __u8)
#define ADC_STOP	_IO(IOCTL_TYPE,3)

extern int errno;
  
static const char *dev = "/dev/kw_adc0";
static float lsb = 0.001;

FILE *plik;

//timestamp

unsigned long time_stamp = 0;

long getMicrotime(){
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (uint64_t)1e6 + currentTime.tv_usec;
}

int main(int argc, char* argv[]) {
    
    uint8_t bufor[2];
    const char *file_path;
    int fd = open(dev, O_RDWR);
  
    if(fd < 0) {
      printf("Cannot open device %s!%d\n", file_path, fd);
      printf("Value of errno: %d\n", errno);
      perror("open perror:");
      exit(1);
    }
    
// otwieram plik z danymi
 if ((plik=fopen("dane.txt", "w"))==NULL) {
     printf ("Nie mogę otworzyć pliku dane.txt\n");
     exit(1);
     }    
    
  /*
  Pomiar single-ended i unipolar, wybor kanalu:
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
    bufor[0] = 0;
    bufor[1] = 0;
    
   int ret = 0;
   int res = 0;
   unsigned long sampling_period = 1000000000;
   
   if(argc > 1){
       int c = atoi(argv[1]);
       sampling_period = c < 10000 ? 10000 : c;
   }
      
    //ustawiamy timer
      ret = ioctl(fd, ADC_SET, sampling_period);
      printf("ustawienie okresu probkowania: %lu \n", sampling_period);
         if(ret < 0) {
	
          printf("Value of errno: %d\n", errno);
          perror("open perror:");
      }
      //wlaczamy timer
      ret = ioctl(fd, ADC_START, 0);
      printf("wlaczenie timera\n");
      
         if(ret < 0) {
          printf("Value of errno: %d\n", errno);
          perror("open perror:");
      }
      while(1){
	
    //pobieram dane
	res = read(fd, bufor, 2);
    //zapisuje timestamp
    time_stamp = getMicrotime();
    
	signed long value;
	value = bufor[1] >> 3; 
	value |= bufor[0] << 5;
	float volt = value * lsb;  
	printf("%lu  bufor:%d %d wartosc: %f\n", time_stamp, bufor[0], bufor[1], volt);
    fflush(stdout);
    
    fprintf(plik, "%lu  bufor:%d %d wartosc: %f\n", time_stamp, bufor[0], bufor[1], volt);
    fflush(plik);
    
    sleep(1);
      }
      
    //     wylaczamy timer
    ret = ioctl(fd, ADC_STOP, 0);
    
    printf("wylaczenie timera\n");
    if(ret < 0) {
          printf("Value of errno: %d\n", errno);
          perror("open perror:");
      }
      
    fclose (plik);
    close(fd);
    return 0;
  }
