#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define BACKFILENAME "REBUILDFILEBACK"
int main(int argc, char **argv)
{
	if(argc != 2)
	{
		printf("should be ./rebuildfile inputfile\ndo not have inputfile\n");
		return -1;
	}

	if(!strcmp(argv[1], "remove"))
	{
		FILE *pRemove = fopen(BACKFILENAME,"rb");
		if(!pRemove)
		{
			return -1;
		}
		char backfile[64];
		int tmp = fread(backfile, 1,sizeof(backfile),pRemove);
		if(tmp<= 0)
		{
			fclose(pRemove);
			return -1;
		}
		printf("rm -f %s\n",backfile);
		unlink(backfile);
		fclose(pRemove);
		unlink(BACKFILENAME);
		return 0;
	}
	char filename[32] = {0};
	strncpy(filename,argv[1],sizeof(filename));
	char outfilename[64] = {0};
	sprintf(outfilename, "%s_tmp",filename);
	printf("OUTFILE:%s\n",outfilename);
	struct stat st;
	if(stat(filename, &st))
	{
		printf("FILE:%s do not exist!\n",filename);
		return -1;
	}
	int totalsize = st.st_size;
	FILE *pFilein = fopen(filename, "rb+");
	if(!pFilein)
	{
		printf("can not open file %s\n", filename);
		return -1;
	}
	FILE *pFileOut = fopen(outfilename, "wb");
	if(!pFileOut)
	{
		printf("can not create file %s\n",outfilename);
		fclose(pFilein);
		return -1;
	}
	int num = 0;
	#define READBUF_SIZE 2048
	char buf[READBUF_SIZE] = {0};
	long long total = 0;
	char tmpbuf[READBUF_SIZE] = {0};
	char *p = NULL;
	char *q = NULL;
	while(fgets(buf, READBUF_SIZE,pFilein))
	{
		num = strlen(buf);
		total += num;
		p = &buf[0];
		int flag = 0;
		while(p && *p != '\0')
		{
			if(flag == 1)
			{
				if(*p == '<')
				{
					*q = '\0';
					fwrite(buf,q - buf,1, pFileOut);
					fwrite(p,&buf[num] - p - 1,1,pFileOut);
					break;
				}
				else
				{
					char *tmpstr = NULL;
					if(tmpstr = strstr(p, "0m#015"))
					{
						*q = '\n';
						*(q+1) = '\0';
						fwrite(&buf[32], q - &buf[32]+1, 1, pFileOut);
						break;
					}
				}
			}
			else if((*p == '#')&&(*(p+1) == '0') && (*(p+2) == '3') && (*(p+3) == '3') && (*(p+4) == '['))
			{
				q = p;
				p+= 5;
				flag = 1;
				continue;
			}
			p++;
		}
		if(!flag)
		{
			fwrite(buf, num,1, pFileOut);
		}
		memset(buf, 0, num);
		num = 0;
		int per = total*100 / totalsize;
		int i = 0;
		printf("[");
		for(i = 1; i <= 100;i++)
		{
			if(i<per)
			{
				printf("=");
			}
			else if(i == per)
			{
				printf(">");
			}
			else
			{
				printf(" ");
			}
		}
		printf("]%d\r", per);
	}
	fsync(fileno(pFileOut));
	fflush(pFileOut);
	FILE *pBack = fopen(BACKFILENAME,"wb");
	if(!pBack)
	{
		printf("can not open %s\n", BACKFILENAME);
		fclose(pFilein);
		fclose(pFileOut);
		return -1;
	}
	fwrite(outfilename,strlen(outfilename),1,pBack);
	fsync(fileno(pBack));
	fflush(pBack);
	fclose(pBack);
	fclose(pFilein);
	fclose(pFileOut);
	printf("\ntotal = %lld\n", total);
}
