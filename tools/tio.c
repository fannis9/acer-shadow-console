#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#define IOCTL_MAGIC 0xEC
#define MAGIC_READ_CL (IOCTL_MAGIC + 1)
#define MAGIC_WRITE_CL (IOCTL_MAGIC + 2)
#define R_CL_HW_IF_STR   _IOR(MAGIC_READ_CL, 0x00, char*)
#define R_CL_FANINFO1    _IOR(MAGIC_READ_CL, 0x10, int32_t*)
#define R_CL_FANINFO2    _IOR(MAGIC_READ_CL, 0x11, int32_t*)
#define R_CL_FANINFO3    _IOR(MAGIC_READ_CL, 0x12, int32_t*)
int main(void){
  int fd=open("/dev/tuxedo_io",O_RDWR);
  if(fd<0){perror("open");return 1;}
  char buf[128]={0};
  if(ioctl(fd,R_CL_HW_IF_STR,buf)==0) printf("接口: %s\n",buf);
  int32_t v;
  const struct {int cmd; const char*name;} q[]={
    {R_CL_FANINFO1,"FANINFO1"},{R_CL_FANINFO2,"FANINFO2"},{R_CL_FANINFO3,"FANINFO3"}};
  for(int i=0;i<3;i++){v=-999; if(ioctl(fd,q[i].cmd,&v)==0) printf("%s = %d\n",q[i].name,v); else printf("%s = ioctl失败\n",q[i].name);}
  close(fd); return 0;
}
