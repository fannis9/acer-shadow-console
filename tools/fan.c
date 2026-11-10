#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define IOCTL_MAGIC 0xEC
#define MAGIC_READ_CL (IOCTL_MAGIC + 1)
#define MAGIC_WRITE_CL (IOCTL_MAGIC + 2)
#define R_CL_HW_IF_STR _IOR(MAGIC_READ_CL, 0x00, char*)
#define R_CL_FANINFO1  _IOR(MAGIC_READ_CL, 0x10, int32_t*)
#define R_CL_FANINFO2  _IOR(MAGIC_READ_CL, 0x11, int32_t*)
#define R_CL_FANINFO3  _IOR(MAGIC_READ_CL, 0x12, int32_t*)
#define W_CL_FANSPEED  _IOW(MAGIC_WRITE_CL, 0x10, int32_t*)
#define W_CL_FANAUTO   _IOW(MAGIC_WRITE_CL, 0x11, int32_t*)

static int fd;
static int32_t info(int cmd){ int32_t v=-1; if(ioctl(fd,cmd,&v)) return -1; return v; }
static void show(void){
  int32_t a=info(R_CL_FANINFO1), b=info(R_CL_FANINFO2), c=info(R_CL_FANINFO3);
  if(a<0){printf("读取失败\n");return;}
  printf("CPU %3d°C   GPU %3d°C   风扇 %3d%%\n", a>>16&0xFF, a>>8&0xFF, a&0xFF);
  printf("INFO2 [%d %d %d]   INFO3 [%d %d %d]\n", b>>16&0xFF,b>>8&0xFF,b&0xFF, c>>16&0xFF,c>>8&0xFF,c&0xFF);
}
int main(int argc,char**argv){
  fd=open("/dev/tuxedo_io",O_RDWR);
  if(fd<0){perror("open /dev/tuxedo_io");return 1;}
  const char*cmd=argc>1?argv[1]:"show";
  if(!strcmp(cmd,"show")) show();
  else if(!strcmp(cmd,"auto")){ int32_t all=0xFF; if(ioctl(fd,W_CL_FANAUTO,&all)) perror("auto"); else printf("已交还固件自动控制\n"); }
  else if(!strcmp(cmd,"set")){
    int cpu=argc>2?atoi(argv[2]):40, gpu=argc>3?atoi(argv[3]):cpu;
    int32_t v=(cpu&0xFF)|((gpu&0xFF)<<8);
    if(ioctl(fd,W_CL_FANSPEED,&v)) perror("set"); else printf("设定 CPU %d%% GPU %d%%\n",cpu,gpu);
  } else { printf("用法: %s {show\|set\|auto}\n",argv[0]); }
  close(fd); return 0;
}
