#include <stdio.h>
#include "errno.h"
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <strings.h>
#include <termios.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <dirent.h>
#include <syslog.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "AccessFun.h"
#include "StdDataType.h"


#define LIB_ACCESS_VER 			0x0001

#define 	CODE 			0xa001
#define 	FILENAMELEN		128			//文件名字最大长度
#define 	FILEEXTLEN		5			//文件扩展名最大长度

typedef struct
{
   INT16U 		oi;  							//对象标识OI
   char			logic_name[OCTET_STRING_LEN];	//对象逻辑名字
   char			file_name[FILENAMELEN];		//对象保存名字
}CLASS_FILENAME;

//typedef struct{
//
//}Method;

const static CLASS_FILENAME  class_name[] ={
		{6001,"6000","/nand/para/table6000.par"},		//采集档案配置表
};

INT16S getclassindex(INT16U oi)
{
	INT16S i=0;
	for(i=0; i < sizeof(class_name)/sizeof(CLASS_FILENAME);i++)
	{
		if(class_name[i].oi == oi)
			return i;
	}
	return -1;
}

INT16U crc(INT16U Data)
{
	unsigned char n;

	INT16U Parity;
	Parity=Data;
	for(n=0;n<8;n++)
	{
		if ((Parity&0x1)==0x1)
		{
			Parity=Parity>>1;
			Parity=Parity^CODE;
		}
		else {Parity=Parity>>1;}
	}
	return(Parity);
}
// 写数据结构体crc16校验到结构体第一个成员crc16
// 输入参数：source:文件内容，size:文件尺寸
// 返回值：     crc：校验值
INT16U  make_parity(void *source,int size)
{
	int 	m;
	INT16U			Parity=0xffff;
	unsigned char   *buf = (unsigned char *)source;
//	 fprintf(stderr,"------------size=%d\n", size);

    for (m=0; m<(size-2); m++){
//    	if(m%32==0)fprintf(stderr,"\n");
//    	fprintf(stderr,"%02x ",buf[m]);
		Parity=Parity^buf[m];
		Parity=crc(Parity);
//		fprintf(stderr,"\n %04x \n",Parity);
	}
//    fprintf(stderr,"\n计算校验=%04x\n",Parity);
	return Parity;
}

// 读取数据到指定缓冲区,并进行CRC16校验
// 结构体数据定义要求
// 		1:使用#pragma pack(1)限制结构体对齐尺寸为1个字节
// 		2:结构体第一个成员为INT16U crc；保存crc校验位
// 输入参数：FileName:文件名，size:文件尺寸
// 输出：	source:文件内容
// 返回值：     =1，校验正确，=0，校验错误
INT8U file_read(char *FileName, void *source, int size,int index,INT16U *retcrc)
{
	FILE 	*fp=NULL;
	int 	num,ret=0;
	INT16U  readcrc;//=(INT16U *)((INT8U*)source+size-2);


//	fprintf(stderr,"read FileName=%s\n",FileName);

	fp = fopen(FileName, "r");
	if (fp != NULL) {
		fseek(fp, index*size, SEEK_SET);
		num=fread(source,1 ,size-2,fp);
		fread(&readcrc,1,2,fp);
//		fprintf(stderr,"read.num=%d,size=%d,reccrc=%04x\n",num,size,readcrc);
		if(num==(size-2)) {			//读取了size字节数据
			INT16U crc= make_parity(source,size);
//			fprintf(stderr,"\n计算 crc =%04x\n",crc);
			if(crc==readcrc)  {
//				fprintf(stderr,"read ok\n");
				*retcrc = readcrc;
				ret = 1;
			}
			else ret = 0;
		}
		fclose(fp);
	} else
	{
		ret = 0;
//		fprintf(stderr, "%s read error\n\r", FileName);
	}
	return ret;
}

//数据保存到指定文件,并进行CRC16校验
// Filename（文件名.扩展名格式）文件名长度限制FILENAMELEN（128），
//                    文件扩展名制定长度必须为3个字符
// 输入参数：FileName:文件名，source:文件内容，size:文件尺寸
// 返回值：     =1，文件保存成功，=0，文件保存失败
INT8U file_write(char *FileName, void *source, int size,int index)
{
	FILE *fp=NULL;
	int	  fd;
	INT8U res;
	int num=0;
	INT8U	*blockdata=NULL;
	INT16U	readcrc;
	int		offset=0;

//	fprintf(stderr,"\nwrite begin size=%d", size);
	blockdata = malloc(size);
//	fprintf(stderr,"\nwrite sourceaddr=%p,blockdata=%p\n", source,blockdata);
	if(blockdata!=NULL) {
//		fprintf(stderr,"write memcpy blockdata\n");
		memcpy(blockdata,source,size-2);
	} else {
		return 0;//error
	}
//	fprintf(stderr,"\nwrite sourceaddr=%p\n", source);
	readcrc = make_parity(source,size);			//计算crc16校验
	int i=0;
//	fpos_t pos;
	for(i=0;i<size;i++){
		fprintf(stderr,"%02x ",blockdata[i]);
	}
	fprintf(stderr,"\nwrite FileName %s source crc=%04x\n",FileName, readcrc);

	memcpy(blockdata+size-2,&readcrc,2);
	if(access(FileName,F_OK)!=0)
	{
		fp = fopen((char*) FileName, "w+");
		fprintf(stderr,"创建文件\n");
	}else {
		fp = fopen((char*) FileName, "r+");
		fprintf(stderr,"替换文件\n");
	}
	if (fp != NULL) {
		offset = fseek(fp, index*size, SEEK_SET);
//		fprintf(stderr,"goto seek=%d offset=%d\n",offset,ftell(fp));
//		//rewind(fp);
//		fgetpos(fp,&pos);
//		fprintf(stderr,"pos=%d\n",pos);
//		if(offset!=0) {
//			offset = fseek(fp, 0L, SEEK_END);
//			fprintf(stderr,"end offset=%d\n",offset);
//		}
		num = fwrite(blockdata, size,1,fp);
		fprintf(stderr,"write index=%d,size=%d num=%d\n",index,size,num);
		fd = fileno(fp);
		fsync(fd);
		fclose(fp);
//		syslog(LOG_NOTICE,"fwrite.num=%d,FileName=%s,size=%d\n",num,FileName,size);
//		fprintf(stderr,"fwrite.num=%d,FileName=%s,size=%d\n",num,FileName,size);
		if(num == 1) {
//			fprintf(stderr,"num=%d\n",num);
			res = 1;
		}else res = 0;
//		syslog(LOG_NOTICE,"fwrite %s end!\n",FileName);
//		fprintf(stderr,"fwrite %s end!\n",FileName);
	} else {
//		fprintf(stderr, "%s saved error\n\r", FileName);
		res = 0;
	}
//	fprintf(stderr, "free %p\n", blockdata);
	free(blockdata);//add by nl1031
	return res;
}


// 数据块文件存储同步
// 输入参数：fname:保存文件名，size:文件尺寸
// 输出：    		blockdata：文件数据缓冲区
// 返回值： =1:文件同步成功,使用blockdata数据源初始化内存
//         =0:文件同步失败，返回错误，参数初始化默认值，建议产生ERC2参数丢失事件
INT8U block_file_sync(char *fname,void *blockdata,int size,int index)
{
	INT8U	ret1=0,ret2=0;
	int		sizenew;
	void 	*blockdata1;
	void  	*blockdata2;
	struct 	stat info1,info2;		//文件信息stat包含头文件/sys/stat.h
	char	fname2[FILENAMELEN];
	INT16U  *readcrc1;//=(INT16U *)((INT8U*)blockdata+size-4);
	INT16U  *readcrc2;
	INT16U  ret=0;
//	fprintf(stderr,"\n read file :%s\n",fname);
	if(fname==NULL) 	return 0;

	//文件默认最后两个字节为CRC16校验，原结构体尺寸如果不是4个字节对齐，进行补齐，加CRC16
	if(size%4==0)	sizenew = size+2;
	else sizenew = size+(4-size%4)+2;
//	fprintf(stderr,"size=%d,sizenew=%d\n",size,sizenew);
	blockdata1 = malloc(sizenew);
	blockdata2 = malloc(sizenew);
	memset(blockdata1,0,sizenew);
	memset(blockdata2,0,sizenew);
//	fprintf(stderr,"\nmalloc p=%p",blockdata2);
	if(blockdata1==NULL || blockdata2==NULL ) {
		if(blockdata1!=NULL)free(blockdata1);
		if(blockdata2!=NULL)free(blockdata2);
		syslog(LOG_NOTICE," %s malloc error",fname);
		return 0;
	}

	readcrc1=(INT16U *)((INT8U*)blockdata1+sizenew-2);
	readcrc2=(INT16U *)((INT8U*)blockdata2+sizenew-2);

	memset(fname2,0,sizeof(fname2));
	strncpy(fname2,fname,strlen(fname)-4);
	strcat(fname2,".bak");

//	fprintf(stderr,"\n------par=%s",fname);
//	fprintf(stderr,"\n------bak=%s",fname2);

	ret1 = file_read(fname,blockdata1,sizenew,index,readcrc1);
	ret2 = file_read(fname2,blockdata2,sizenew,index,readcrc2);
//	fprintf(stderr,"\ncrc1=%04x,crc2=%04x,ret1=%d,ret2=%d\n",*readcrc1,*readcrc2,ret1,ret2);
	if((*readcrc1 == *readcrc2) && (ret1==1) && (ret2==1))  {		//两个文件校验正确，并且校验码相等，返回 1
//		fprintf(stderr,"正确\n");
//		syslog(LOG_NOTICE," %s 校验正确 ",fname);
		ret= 1;
	}
	if ((*readcrc1!=*readcrc2) && (ret1==1) && (ret2==1)) {		//两个文件校验正确，但是校验码不等，采用文件保存日期新的数据
//		fprintf(stderr,"校验码不等\n");
		stat(fname,&info1);
		stat(fname2,&info2);
//		fprintf(stderr,"info1=%ld,info2=%ld\n",info1.st_mtim.tv_sec,info2.st_mtim.tv_sec);
//		if(info1.st_mtim.tv_sec >= info2.st_mtim.tv_sec) {			//fname1文件修改时间新,更新fname2备份数据
		//校验码不等，使用fname1文件内容更新fname2
//			syslog(LOG_NOTICE," %s 校验码不等,更新备份文件 ",fname);
//			fprintf(stderr," %s 校验码不等,更新备份文件 ",fname);
			file_write(fname2,blockdata1,sizenew,index);
			ret= 1;
//		}else {														//fname2文件修改时间新,更新fname1源数据
//			syslog(LOG_NOTICE," %s 校验码不等,更新源文件 ",fname);
//			fprintf(stderr," %s 校验码不等,更新源文件 ",fname);
//			file_write(fname,blockdata2,sizenew);
//			ret= 1;
//		}
	}
	if((ret1==1) &&(ret2==0)) {							//fname1校验正确，fname2校验错误,更新fname2备份文件
//		fprintf(stderr,"备份文件校验错误\n");
		syslog(LOG_NOTICE," %s 备份文件校验错误 ",fname);
		file_write(fname2,blockdata1,sizenew,index);
		ret= 1;
	}
	if((ret1==0) &&(ret2==1)) {							//fname2校验正确，fname1校验错误,更新fname1源文件
//		fprintf(stderr,"主文件校验错误\n");
		syslog(LOG_NOTICE," %s 主文件校验错误 ",fname);
		file_write(fname,blockdata2,sizenew,index);
		memcpy(blockdata1,blockdata2,sizenew);
		ret= 1;
	}
	if(ret1==0 && ret2==0){
//		fprintf(stderr,"主文件 备份文件都错误  size=%d\n", sizenew);
		syslog(LOG_NOTICE," %s 主文件 备份文件都错误 ",fname);
		file_write(fname2,blockdata1,sizenew,index);
		file_write(fname,blockdata1,sizenew,index);
		ret = 1;
	}
	if (ret ==1)
	{
		memcpy(blockdata,blockdata1,size);
//		syslog(LOG_NOTICE," %s 返回数据 ",fname);
	}else {
//		fprintf(stderr,"\n 读取失败！！\n");
		syslog(LOG_NOTICE," %s 读取失败! ",fname);
	}
	free(blockdata1);
	free(blockdata2);
	return ret;					//异常情况，程序返回0，参数初始默认值，产生ERC2参数丢失事件
}

// 数据块数据保存文件
// 输入参数：fname:主文件名，blockdata：主文件块缓冲区，size:主文件尺寸，index:文件的存储索引位置
// 返回值：=1：文件保存成功，=0，文件保存失败，此时建议产生ERC2参数丢失事件通知主站异常
INT8U save_block_file(char *fname,void *blockdata,int size,int index)
{
	int		i,ret=0;
	int		sizenew;
	INT16U	readcrc;

	if(fname==NULL) 	  return 0;

	//文件默认最后两个字节为CRC16校验，原结构体尺寸如果不是4个字节对齐，进行补齐，加CRC16
	if(size%4==0)	sizenew = size+2;
	else sizenew = size+(4-size%4)+2;
	fprintf(stderr,"write fname=%s,size=%d,sizenew=%d\n",fname,size,sizenew);
	if(file_write(fname,blockdata,sizenew,index)==1) {
		for(i=0;i<3;i++) {
			fprintf(stderr,"read fname=%s,size=%d,sizenew=%d\n",fname,size,sizenew);
			if(file_read(fname,blockdata,sizenew,index,&readcrc)==1) {						//源文件正确，备份参数文件
//				fprintf(stderr,"保存文件成功，备份文件,crc=%04x\n",readcrc);
				ret = block_file_sync(fname,blockdata,size,index);			//配置文件同步处理
//				fprintf(stderr,"**********block_file_sync ret=%d\n",ret);
				if(ret==1) 	break;
			}else {
				syslog(LOG_NOTICE,"file_read %s error",fname);
			}
		}
	}else {
		syslog(LOG_NOTICE,"file_write %s error",fname);
	}
	return ret;
}

/*
 * 参数类存储
 * 输入参数：oi对象标识，blockdata：主文件块缓冲区，size:主文件尺寸，index:文件的存储索引位置
 * 返回值：=1：文件保存成功，=0，文件保存失败，此时建议产生ERC2参数丢失事件通知主站异常
 * =-1:  未查找到OI类数据
 */

INT8U ParaClassSave(INT16U oi,void *blockdata,int size,int index,int method)
{
	INT8U 	ret=-1;
	INT16S	classindex=-1;

	classindex = getclassindex(oi);
	if(classindex == -1) {
		return -1;
	}
//	save_block_file();
	return ret;
}
