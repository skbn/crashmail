#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include <proto/dos.h>
#include <dos/dos.h>
#include <dos/dosextens.h>

#include <shared/types.h>
#include <shared/jblist.h>
#include <shared/mystrncpy.h>
#include <shared/path.h>

#include <oslib/osmem.h>
#include <oslib/osdir.h>

bool osReadDir(char *dirname,struct jbList *filelist,bool (*acceptfunc)(char *filename))
{
   BPTR lock;
   struct FileInfoBlock *fib;
   struct osFileEntry *tmp;
   struct DateStamp *ds;
   char buf[200];
   LONG result;

   jbNewList(filelist);

   if(!(lock = Lock(dirname, ACCESS_READ)))
      return(FALSE);

   if(!(fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL)))
   {
      UnLock(lock);
      return(FALSE);
   }

   if(!Examine(lock, fib))
   {
      FreeDosObject(DOS_FIB, fib);
      UnLock(lock);
      return(FALSE);
   }

   while((result = ExNext(lock, fib)))
   {
      bool add;

      /* Skip directories */
      if(fib->fib_DirEntryType > 0)
         continue;

      if(!acceptfunc)   add=TRUE;
      else              add=(*acceptfunc)(fib->fib_FileName);

      if(add)
      {
         if(!(tmp=(struct osFileEntry *)osAllocCleared(sizeof(struct osFileEntry))))
         {
            jbFreeList(filelist);
            FreeDosObject(DOS_FIB, fib);
            UnLock(lock);
            return(FALSE);
         }

         mystrncpy(tmp->Name,fib->fib_FileName,100);
         tmp->Size=fib->fib_Size;
         
         /* Convert AmigaOS DateStamp to Unix timestamp */
         /* DateStamp uses days since Jan 1, 1978, minutes, and ticks */
         /* Unix timestamp uses seconds since Jan 1, 1970 */
         ds = &fib->fib_Date;
         
         /* Days from 1970 to 1978 = 8 years = 2922 days (including 2 leap years) */
         #define DAYS_1970_TO_1978 2922
         tmp->Date = (time_t)(((ds->ds_Days + DAYS_1970_TO_1978) * 86400UL) + 
                              (ds->ds_Minute * 60UL) + 
                              (ds->ds_Tick / TICKS_PER_SECOND));

         jbAddNode(filelist,(struct jbNode *)tmp);
      }
   }

   FreeDosObject(DOS_FIB, fib);
   UnLock(lock);

   /* ExNext returns 0 when done, check if it's a real error */
   if(result == 0 && IoErr() == ERROR_NO_MORE_ENTRIES)
      return(TRUE);

   return(TRUE);
}

bool osScanDir(char *dirname,void (*func)(char *file))
{
   BPTR lock;
   struct FileInfoBlock *fib;
   LONG result;

   if(!(lock = Lock(dirname, ACCESS_READ)))
      return(FALSE);

   if(!(fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL)))
   {
      UnLock(lock);
      return(FALSE);
   }

   if(!Examine(lock, fib))
   {
      FreeDosObject(DOS_FIB, fib);
      UnLock(lock);
      return(FALSE);
   }

   while((result = ExNext(lock, fib)))
      (*func)(fib->fib_FileName);

   FreeDosObject(DOS_FIB, fib);
   UnLock(lock);

   return(TRUE);
}

struct osFileEntry *osGetFileEntry(char *file)
{
   BPTR lock;
   struct FileInfoBlock *fib;
   struct osFileEntry *tmp;
   struct DateStamp *ds;

   if(!(lock = Lock(file, ACCESS_READ)))
      return(NULL);

   if(!(fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL)))
   {
      UnLock(lock);
      return(NULL);
   }

   if(!Examine(lock, fib))
   {
      FreeDosObject(DOS_FIB, fib);
      UnLock(lock);
      return(NULL);
   }

   if(!(tmp=(struct osFileEntry *)osAllocCleared(sizeof(struct osFileEntry))))
   {
      FreeDosObject(DOS_FIB, fib);
      UnLock(lock);
      return(NULL);
   }

   mystrncpy(tmp->Name,GetFilePart(file),100);
   tmp->Size=fib->fib_Size;
   
   /* Convert AmigaOS DateStamp to Unix timestamp */
   ds = &fib->fib_Date;
   #define DAYS_1970_TO_1978 2922
   tmp->Date = (time_t)(((ds->ds_Days + DAYS_1970_TO_1978) * 86400UL) + 
                        (ds->ds_Minute * 60UL) + 
                        (ds->ds_Tick / TICKS_PER_SECOND));

   FreeDosObject(DOS_FIB, fib);
   UnLock(lock);

   return(tmp);
}
