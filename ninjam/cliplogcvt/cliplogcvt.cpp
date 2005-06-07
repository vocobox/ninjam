#include <windows.h>
#include <stdio.h>
#include "../../WDL/string.h"
#include "../../WDL/ptrlist.h"
#include "../../WDL/lineparse.h"

class UserChannelValueRec
{
public:
  int position;
  WDL_String guidstr;
  
};

class UserChannelList
{
  public:

   WDL_String user;
   int chidx;

   WDL_PtrList<UserChannelValueRec> items;
};


int WriteRec(FILE *fp, char *name, int id, int trackid, int position, int len, char *path)
{
  char *p=name;
  while (*p && *p == '0') p++;
  if (!*p) return 0; // empty name

  char *exts[]={".wav",".ogg"};
  WDL_String fnfind;
  int x;
  for (x = 0; x < sizeof(exts)/sizeof(exts[0]); x ++)
  {
    fnfind.Set(path);
    fnfind.Append("\\");
    fnfind.Append(name);
    fnfind.Append(exts[x]);

    FILE *tmp=fopen(fnfind.Get(),"rb");
    if (tmp) 
    {
      fclose(tmp);
      break;
    }
  }
  if (x == sizeof(exts)/sizeof(exts[0]))
  {
    printf("Error resolving guid %s\n",name);
    return 0;
  }

  fprintf(fp,"%d;\t" "%d;\t" "%f;\t" "%f;\t",id,trackid,(double)position,(double)len);
    
  fprintf(fp,"1.000000;\tFALSE;\tFALSE;\t0;\tFALSE;\tFALSE;\tAUDIO;\t");

  fprintf(fp,"\"%s.ogg\";\t",name);

  fprintf(fp,"0;\t" "0.0000;\t" "%f;\t" "0.0000;\t" "0.0000;\t" "1.000000;\t0;\t0.000000;\t-2;\t0.000000;\t0;\t-1;\t-2;\t2\n",
    (double)len);

  return 1;
}

int main(int argc, char **argv)
{
  printf("ClipLogCvt v0.0 -- converts Ninjam log file to Vegas 4 EDL text file\n");
  if (argc != 2)
  {
    printf("Usage: clipoutcvt session_directory\n");
    return 1;
  }
  WDL_String logfn(argv[1]);
  logfn.Append("\\clipsort.log");
  FILE *logfile=fopen(logfn.Get(),"rt");
  if (!logfile)
  {
    printf("Error opening logfile\n");
    return -1;
  }

  int m_cur_idx=0;
  double m_cur_bpm=-1.0;
  int m_cur_bpi=-1;

  UserChannelList localrecs[32];
  WDL_PtrList<UserChannelList> curintrecs;
  

  // go through the log file
  for (;;)
  {
    char buf[4096];
    buf[0]=0;
    fgets(buf,sizeof(buf),logfile);
    if (!buf[0]) break;
    if (buf[strlen(buf)-1]=='\n') buf[strlen(buf)-1]=0;
    if (!buf[0]) continue;

    LineParser lp(0);

    int res=lp.parse(buf);

    if (res)
    {
      printf("Error parsing log line!\n");
      return -1;
    }
    else
    {
      if (lp.getnumtokens()>0)
      {
        int w=lp.gettoken_enum(0,"interval\0local\0user\0end\0");
        if (w < 0)
        {
          printf("unknown token %s\n",lp.gettoken_str(0));
          return -1;
        }
        switch (w)
        {
          case 0: // interval
            {
              if (lp.getnumtokens() != 4)
              {
                printf("interval line has wrong number of tokens\n");
                return -2;
              }

              int idx=0;
              double bpm=0.0;
              int bpi=0;
              idx=lp.gettoken_int(1);
              bpm=lp.gettoken_float(2);
              bpi=lp.gettoken_int(3);

              if (idx != m_cur_idx+1)
              {
                printf("Error: interval %d out of sync, expected %d\n",idx,m_cur_idx);
                return -2;
              }
              if ((m_cur_bpi >= 0 && m_cur_bpi != bpi) ||
                  (m_cur_bpm >= 0 && m_cur_bpm != bpm))
              {
                printf("BPI/BPM changed from %d/%.2f to %d/%.2f\n",m_cur_bpi,m_cur_bpm,bpi,bpm);
                return -2;
              }

              m_cur_bpi=bpi;
              m_cur_bpm=bpm;
              m_cur_idx=idx;
            }
          break;
          case 1: // local
            {
              if (lp.getnumtokens() != 3)
              {
                printf("local line has wrong number of tokens\n");
                return -2;
              }
              UserChannelValueRec *p=new UserChannelValueRec;
              p->position=m_cur_idx;
              p->guidstr.Set(lp.gettoken_str(1));
              localrecs[(lp.gettoken_int(2))&31].items.Add(p);
            }
          break;
          case 2: // user
            {
              if (lp.getnumtokens() != 5)
              {
                printf("user line has wrong number of tokens\n");
                return -2;
              }

              char *guidtmp=lp.gettoken_str(1);
              char *username=lp.gettoken_str(2);
              int chidx=lp.gettoken_int(3);
              char *channelname=lp.gettoken_str(4);

              printf("Got user '%s' channel %d '%s' guid %s\n",username,chidx,channelname,guidtmp);

              UserChannelValueRec *ucvr=new UserChannelValueRec;
              ucvr->guidstr.Set(guidtmp);
              ucvr->position=m_cur_idx;

              int x;
              for (x = 0; x < curintrecs.GetSize(); x ++)
              {
                if (!stricmp(curintrecs.Get(x)->user.Get(),username) && curintrecs.Get(x)->chidx == chidx)
                {
                  break;
                }
              }
              if (x == curintrecs.GetSize())
              {
                // add the rec
                UserChannelList *t=new UserChannelList;
                t->user.Set(username);
                t->chidx=chidx;

                curintrecs.Add(t);
              }
              curintrecs.Get(x)->items.Add(ucvr);
              // add this record to it
            }

          break;
          case 3: // end
          break;
        }



      }
    }

  }
  fclose(logfile);

  printf("Done analyzing log, building output...\n");
  WDL_String outfn(argv[1]);
  outfn.Append("\\clipsort.txt");
  FILE *outfile=fopen(outfn.Get(),"wt");
  if (!outfile)
  {
    printf("Error opening outfile\n");
    return -1;
  }
  fprintf(outfile,"%s", 
    "\"ID\";\"Track\";\"StartTime\";\"Length\";\"PlayRate\";\"Locked\";\"Normalized\";\"StretchMethod\";\"Looped\";\"OnRuler\";\"MediaType\";\"FileName\";\"Stream\";\"StreamStart\";\"StreamLength\";\"FadeTimeIn\";\"FadeTimeOut\";\"SustainGain\";\"CurveIn\";\"GainIn\";\"CurveOut\";\"GainOut\";\"Layer\";\"Color\";\"CurveInR\";\"CurveOutR\"\n");


  int id=1;
  int track_id=0;
  int x;
  int len=(int) ((double)m_cur_bpi * 60000.0 / m_cur_bpm);
  for (x= 0; x < sizeof(localrecs)/sizeof(localrecs[0]); x ++)
  {
    int y;

    for (y = 0; y < localrecs[x].items.GetSize(); y ++)
    {
      int pos=(localrecs[x].items.Get(y)->position-1) * len;
      if (WriteRec(outfile, localrecs[x].items.Get(y)->guidstr.Get(), id, track_id, pos, len,argv[1])) id++;
    }
    if (y)  track_id++;

  }
  for (x= 0; x < curintrecs.GetSize(); x ++)
  {
    int y;

    for (y = 0; y < curintrecs.Get(x)->items.GetSize(); y ++)
    {
      int pos=(curintrecs.Get(x)->items.Get(y)->position-1) * len;
      if (WriteRec(outfile, curintrecs.Get(x)->items.Get(y)->guidstr.Get(), id, track_id, pos, len,argv[1])) id++;
    }
    if (y)  track_id++;

  }
  printf("wrote %d records, %d tracks\n",id-1,track_id);




  fclose(outfile);


  return 0;

}