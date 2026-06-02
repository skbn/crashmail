#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* AmigaOS includes for 64-bit file support */
#ifdef PLATFORM_AMIGA
#include <proto/dos.h>
#include <dos/dos.h>
#endif

#include <shared/types.h>

#include <oslib/osfile.h>

/* Check if we have 64-bit file support (AmigaOS 3.9+ or NewLib) */
#ifdef PLATFORM_AMIGA
#if defined(__amigaos4__) || defined(__MORPHOS__) || defined(__AROS__)
#define HAS_64BIT_FILES 1
#else
/* For AmigaOS 3.x, we'll try to use 64-bit if available, fallback to 32-bit */
#define HAS_64BIT_FILES 0
#endif
#endif

osFile osOpen(char *name,uint32_t mode)
{
   FILE *fh;

   if(mode == MODE_NEWFILE) 
	{
		fh=fopen(name,"wb");
	}
   else if(mode == MODE_OLDFILE) 
	{
		fh=fopen(name,"rb");
	}
	else
	{
   	if(!(fh=fopen(name,"r+b")))
	      fh=fopen(name,"w+b");
	}
	
   return (osFile) fh;
}

void osClose(osFile os)
{
    fclose((FILE *)os);
}

int osGetChar(osFile os)
{
   int c;

   c=fgetc((FILE *)os);

   if(c==EOF)
      c=-1;

   return(c);
}

uint32_t osRead(osFile os,void *buf,uint32_t bytes)
{
   return fread(buf,1,bytes,(FILE *)os);
}

bool osPutChar(osFile os, char ch)
{
   if(fputc(ch,(FILE *)os)==EOF)
		return(FALSE);
		
	return(TRUE);
}

bool osWrite(osFile os,const void *buf,uint32_t bytes)
{
   if(fwrite(buf,1,bytes,(FILE *)os)!=bytes)
		return(FALSE);
		
	return(TRUE);
}

bool osPuts(osFile os,char *str)
{
   if(fputs(str,(FILE *)os)==EOF)
		return(FALSE);
		
	return(TRUE);
}

uint32_t osFGets(osFile os,char *str,uint32_t max)
{
   char *s;

   s=fgets(str,max,(FILE *)os);

   if(s)
   {
      if(strlen(s)>=2 && s[strlen(s)-1]==10 && s[strlen(s)-2]==13)
      {
         /* CRLF -> LF */

         s[strlen(s)-2]=10;
         s[strlen(s)-1]=0;
      }

      return (uint32_t)strlen(s);
   }

   return(0);
}

off_t osFTell(osFile os)
{
   return (off_t)ftell((FILE *)os);
}

bool osFPrintf(osFile os,char *fmt,...)
{
   va_list args;
	int res;
	
   va_start(args, fmt);
   res=vfprintf((FILE *)os,fmt,args);
   va_end(args);
	
	if(!res)
		return(FALSE);
		
	return(TRUE);
}

bool osVFPrintf(osFile os,char *fmt,va_list args)
{
	int res;
	
   res=vfprintf((FILE *)os,fmt,args);
	
	if(!res)
		return(FALSE);
		
	return(TRUE);
}

void osSeek(osFile fh,off_t offset,short mode)
{
   int md;

   if(mode == OFFSET_BEGINNING)
      md=SEEK_SET;
   else if(mode == OFFSET_END)
      md=SEEK_END;
   else
      md=SEEK_SET;

   fseek((FILE *)fh,(long)offset,md);
}
