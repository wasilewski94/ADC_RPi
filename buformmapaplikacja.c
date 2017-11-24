#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#define PAGE_SIZE 100

int main()
{
  printf("Probuje otworzyc urzadzenie!!!\n");
  fflush(stdout);
  int plik=open("/dev/kw_adc0", O_RDWR);
  if(plik==-1)
    {
      printf("Nie moge otworzyc urzadzenia!!!\n");
      fflush(stdout);
      exit(1);
    }
  printf("Otworzylem urzadzenie!!!\n");
  fflush(stdout);
  //Mapujemy pamiêæ
  unsigned short * devm = (unsigned short *) 
    mmap(0,PAGE_SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,
	 plik,0x0000000);
  if(devm == (void *) -1l)
    {
      perror("Nie moge zmapowac pamieci!!!\n");
    }
  printf("Zmapowalem pod adres: %x\n",devm);
  close(plik);
  fflush(stdout);
  int wyjscie=0;
  int i=0;
  while(1)
    {
      int i;
      char czas[20];
      time_t t1=time(NULL);
      struct tm * t=localtime(&t1);
      strftime(czas,20,"%H:%M:%S",t);
      for(i=0;i<strlen(czas);i++)
	{
	  devm[i]=(short)czas[i]+15*256;
	}
      //printf("%s\n",czas);
      sleep(1);
    }  
return 0;
}
