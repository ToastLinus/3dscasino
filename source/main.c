#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SAVE "sdmc:/3ds/casino3ds.sav"
#define RED "\x1b[31m"
#define GRN "\x1b[32m"
#define YEL "\x1b[33m"
#define CYN "\x1b[36m"
#define WHT "\x1b[37;1m"
#define RST "\x1b[0m"

static PrintConsole top, bot;
static long chips = 5000, lastWheel = 0;

static void frame(void){ gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank(); }
static void wait(int n){ while(n-- > 0 && aptMainLoop()) frame(); }
static void save(void){ FILE*f=fopen(SAVE,"wb"); if(f){ fwrite(&chips,sizeof chips,1,f); fwrite(&lastWheel,sizeof lastWheel,1,f); fclose(f);} }
static void load(void){ FILE*f=fopen(SAVE,"rb"); if(f){ if(fread(&chips,sizeof chips,1,f)!=1) chips=5000; if(fread(&lastWheel,sizeof lastWheel,1,f)!=1) lastWheel=0; fclose(f);} if(chips<100) chips=1000; }
static void help(const char*s){ consoleSelect(&bot); consoleClear(); printf("%s",s); consoleSelect(&top); }
static void clampBet(long*b){ if(*b>chips)*b=chips; if(*b<100)*b=100; if(chips<100)*b=chips; }
static void bonus(char*msg){ if(chips<100){ chips+=1000; strcat(msg," | Pit-Boss: +1000 Chips"); } }

/* ---------------- ROULETTE (amerikanisch: 0 und 00) ---------------- */
static const char*bn[]={"Rot","Schwarz","Ungerade","Gerade","1-18","19-36","1. Dutzend","2. Dutzend","3. Dutzend","Spalte 1","Spalte 2","Spalte 3","Volltreffer"};
static int isRed(int n){ static const int r[]={1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36}; for(int i=0;i<18;i++) if(r[i]==n) return 1; return 0; }
static const char*ns(int n){ static char b[4]; if(n==37) return "00"; snprintf(b,sizeof b,"%d",n); return b; }
static const char*col(int n){ return (n==0||n==37)?GRN:(isRed(n)?RED:WHT); }
static int payout(int t,int num,int n){
  if(t==12) return n==num?35:0;
  if(n<1||n>36) return 0;
  switch(t){
    case 0: return isRed(n)?1:0;   case 1: return !isRed(n)?1:0;
    case 2: return n%2?1:0;        case 3: return !(n%2)?1:0;
    case 4: return n<=18?1:0;      case 5: return n>=19?1:0;
    case 6: return n<=12?2:0;      case 7: return (n>12&&n<=24)?2:0;  case 8: return n>24?2:0;
    default: return ((n-1)%3==t-9)?2:0;
  }
}
static void rdraw(int t,int num,long bet,int shown,const char*msg){
  consoleSelect(&top); consoleClear();
  printf(YEL"=== CASINO ROULETTE ==="RST"\n\nChips:   %ld\nEinsatz: %ld\n\n",chips,bet);
  printf("Wette:   "CYN"< %s",bn[t]); if(t==12) printf(" auf %s",ns(num)); printf(" >"RST"\n\n");
  if(shown>=0) printf("Kugel:   %s[ %s ]"RST"\n\n",col(shown),ns(shown));
  printf("%s\n",msg);
}
static void roulette(void){
  int t=0,num=17,shown=-1; long bet=100; char msg[96]="Viel Glueck!";
  help("Links/Rechts: Wettart\nHoch/Runter: Zahl (0-36,00)\nL/R: Einsatz -/+100\nX: +1000   Y: All-In\nA: Drehen   B: Zurueck\n\nAuszahlung: Zahl 35:1\nDutzend/Spalte 2:1, Rest 1:1");
  while(aptMainLoop()){
    hidScanInput(); u32 k=hidKeysDown();
    if(k&KEY_B) return;
    if(k&KEY_RIGHT) t=(t+1)%13; if(k&KEY_LEFT) t=(t+12)%13;
    if(k&KEY_UP) num=(num+1)%38; if(k&KEY_DOWN) num=(num+37)%38;
    if(k&KEY_R) bet+=100; if(k&KEY_L) bet-=100; if(k&KEY_X) bet+=1000; if(k&KEY_Y) bet=chips;
    clampBet(&bet);
    if(k&KEY_A){
      chips-=bet; int n=0;
      for(int i=0;i<28&&aptMainLoop();i++){ n=rand()%38; rdraw(t,num,bet,n,"Rien ne va plus..."); wait(2+i/4); }
      shown=n; int m=payout(t,num,n);
      if(m){ chips+=bet*(m+1); snprintf(msg,sizeof msg,GRN"Gewonnen! +%ld"RST,bet*m); }
      else snprintf(msg,sizeof msg,RED"Verloren -%ld"RST,bet);
      bonus(msg); save(); clampBet(&bet);
    }
    rdraw(t,num,bet,shown,msg); frame();
  }
}

/* ---------------- PFERDERENNEN ---------------- */
#define LEN 30
static const char*hn[6]={"Blitz","Sternschnuppe","Donnerhuf","Mondschein","Glueckspilz","Outsider"};
static const int hp[6]={64,58,52,46,40,34}, ho[6]={2,3,4,5,7,10};
static void hdraw(int h,long bet,int*pos,int racing,const char*msg){
  consoleSelect(&top); consoleClear();
  printf(YEL"=== PFERDERENNEN ==="RST"  Chips: %ld\n\n",chips);
  for(int i=0;i<6;i++){
    printf("%s%d %-13s %2d:1 |",(i==h)?CYN:RST,i+1,hn[i],ho[i]-1);
    if(racing||pos[i]>0){ for(int j=0;j<LEN;j++) putchar(j<pos[i]?'=':(j==pos[i]?'>':'.')); }
    printf(RST"\n");
  }
  printf("\nEinsatz: %ld   (Gewinn bei Sieg: %ld)\n\n%s\n",bet,bet*ho[h],msg);
}
static void horses(void){
  int h=0; long bet=100; char msg[96]="Waehle dein Pferd!"; int pos[6]={0};
  help("Links/Rechts: Pferd waehlen\nL/R: Einsatz -/+100\nX: +1000   Y: All-In\nA: Rennen starten\nB: Zurueck\n\nQuote = Gewinn zu 1\nEinsatz inklusive");
  while(aptMainLoop()){
    hidScanInput(); u32 k=hidKeysDown();
    if(k&KEY_B) return;
    if(k&KEY_RIGHT) h=(h+1)%6; if(k&KEY_LEFT) h=(h+5)%6;
    if(k&KEY_R) bet+=100; if(k&KEY_L) bet-=100; if(k&KEY_X) bet+=1000; if(k&KEY_Y) bet=chips;
    clampBet(&bet);
    if(k&KEY_A){
      chips-=bet; memset(pos,0,sizeof pos); int win=-1;
      while(win<0&&aptMainLoop()){
        for(int i=0;i<6;i++) if(rand()%100<hp[i]) pos[i]++;
        for(int i=0;i<6;i++) if(pos[i]>=LEN&&(win<0||pos[i]>pos[win])) win=i;
        for(int i=0;i<6;i++) if(pos[i]>LEN) pos[i]=LEN;
        hdraw(h,bet,pos,1,"Und sie laufen!"); wait(2);
      }
      if(win==h){ chips+=bet*ho[h]; snprintf(msg,sizeof msg,GRN"%s gewinnt! +%ld"RST,hn[win],bet*(ho[h]-1)); }
      else snprintf(msg,sizeof msg,RED"%s gewinnt. -%ld"RST,hn[win],bet);
      bonus(msg); save(); clampBet(&bet);
    }
    hdraw(h,bet,pos,0,msg); frame();
  }
}

/* ---------------- GLUECKSRAD (alle 24h gratis) ---------------- */
static const long pz[8]={500,1000,1500,2500,5000,10000,25000,50000};
static const int pw[8]={30,25,18,12,8,4,2,1};
static void wheel(void){
  help("A: Rad drehen (1x pro Tag)\nB: Zurueck");
  char msg[96]="Dreh das Gluecksrad!";
  while(aptMainLoop()){
    hidScanInput(); u32 k=hidKeysDown();
    if(k&KEY_B) return;
    long left=86400-(long)(time(NULL)-lastWheel);
    if((k&KEY_A)&&left<=0){
      int r=rand()%100,s=0,w=0; for(;w<8;w++){ s+=pw[w]; if(r<s) break; } if(w>7) w=7;
      for(int i=0;i<30&&aptMainLoop();i++){
        consoleSelect(&top); consoleClear(); printf(YEL"=== GLUECKSRAD ==="RST"\n\n   >>> %ld <<<\n",pz[rand()%8]); wait(2+i/3);
      }
      chips+=pz[w]; lastWheel=time(NULL); save();
      snprintf(msg,sizeof msg,GRN"Gewonnen: %ld Chips!"RST,pz[w]);
    }
    consoleSelect(&top); consoleClear();
    printf(YEL"=== GLUECKSRAD ==="RST"  Chips: %ld\n\n%s\n\n",chips,msg);
    if(left>0) printf("Naechster Dreh in %ldh %ldmin",left/3600,(left%3600)/60); else printf(CYN"Dein Dreh ist bereit! (A)"RST);
    frame();
  }
}

int main(void){
  gfxInitDefault(); consoleInit(GFX_TOP,&top); consoleInit(GFX_BOTTOM,&bot);
  srand(time(NULL)); load();
  const char*items[]={"Roulette","Pferderennen","Gluecksrad"}; int c=0;
  while(aptMainLoop()){
    hidScanInput(); u32 k=hidKeysDown();
    if(k&KEY_START) break;
    if(k&KEY_DOWN) c=(c+1)%3; if(k&KEY_UP) c=(c+2)%3;
    if(k&KEY_A){ if(c==0) roulette(); else if(c==1) horses(); else wheel(); }
    help("Hoch/Runter: Spiel\nA: Auswaehlen\nSTART: Beenden\n\nSpielstand wird automatisch\ngespeichert.");
    consoleSelect(&top); consoleClear();
    printf(YEL"\n   ##  DIAMOND CASINO 3DS  ##"RST"\n\n   Chips: %ld\n\n",chips);
    for(int i=0;i<3;i++) printf("   %s %s"RST"\n",i==c?CYN">":" ",items[i]);
    frame();
  }
  save(); gfxExit(); return 0;
}
