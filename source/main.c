#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define SAVE "sdmc:/3ds/casino3ds.sav"
#define BW 320
#define BH 240
#define PI 3.14159265f
#define PI2 6.28318531f
#define WR 46
#define WS (WR * 2 + 1)
#define BR 40
#define RLEN 100

typedef struct { u8 r, g, b; } Col;
typedef struct { u8 r, g, b, on; } Px;

#define C_GOLD ((Col){235, 185, 45})
#define C_RED ((Col){205, 25, 35})
#define C_BLK ((Col){25, 25, 30})
#define C_GRN ((Col){0, 150, 75})
#define C_WHT ((Col){250, 250, 250})
#define C_FELT ((Col){12, 85, 45})
#define C_PINK ((Col){255, 60, 160})
#define C_PURP ((Col){140, 40, 200})
#define C_BLUE ((Col){40, 110, 235})

static const long pz[8] = {500, 1000, 1500, 2500, 5000, 10000, 25000, 50000};
static const int pw[8] = {30, 25, 18, 12, 8, 4, 2, 1};
static const Col pcols[8] = {{205,25,35},{40,110,235},{235,185,45},{255,60,160},{0,150,75},{255,130,20},{140,40,200},{0,225,255}};
static const Col hcols[6] = {{205,25,35},{40,110,235},{235,185,45},{255,60,160},{0,150,75},{255,130,20}};
static const Col rowCol[13] = {{205,25,35},{25,25,30},{140,40,200},{40,110,235},{255,130,20},{255,60,160},{235,185,45},{0,225,255},{0,150,75},{205,25,35},{40,110,235},{140,40,200},{250,250,250}};
static const char *hn[6] = {"Blitz", "Sternschnuppe", "Donnerhuf", "Mondschein", "Glueckspilz", "Outsider"};
static const char *bn[13] = {"Rot", "Schwarz", "Ungerade", "Gerade", "1-18", "19-36", "1. Dutzend", "2. Dutzend", "3. Dutzend", "Spalte 1", "Spalte 2", "Spalte 3", "Volltreffer"};
static const int hp[6] = {64, 58, 52, 46, 40, 34};
static const int ho[6] = {2, 3, 4, 5, 7, 10};

static u8 *fb;
static u16 fw, fh;
static PrintConsole top;
static long chips = 5000, lastWheel = 0;
static Px rouletteImg[WS * WS], prizeImg[WS * WS];
static Col rcols[38];
static float rRot = 0, pRot = 0;

/* ---------- Zeichnen auf den unteren Bildschirm ---------- */
static inline void put(int x, int y, Col c) {
  if ((unsigned)x >= BW || (unsigned)y >= BH) return;
  u8 *p = fb + ((((int)fw - 1 - y) * (int)fh) + x) * 3;
  p[0] = c.b; p[1] = c.g; p[2] = c.r;
}
static void rect(int x, int y, int w, int h, Col c) {
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) put(i, j, c);
}
static void disc(int cx, int cy, int r, Col c) {
  for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++)
    if (i * i + j * j <= r * r) put(cx + i, cy + j, c);
}
static void boxf(int x, int y, int w, int h, int t, Col c) {
  rect(x, y, w, t, c); rect(x, y + h - t, w, t, c);
  rect(x, y, t, h, c); rect(x + w - t, y, t, h, c);
}
static void beginFrame(void) { fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh); }
static void endFrame(void) { gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank(); }
static void bgFelt(void) { rect(0, 0, BW, BH, C_FELT); boxf(4, 4, BW - 8, BH - 8, 3, C_GOLD); }

static void buildWheel(Px *img, int n, const Col *cols) {
  for (int j = -WR; j <= WR; j++) for (int i = -WR; i <= WR; i++) {
    Px *p = &img[(j + WR) * WS + (i + WR)];
    float d = sqrtf((float)(i * i + j * j));
    p->on = 0;
    if (d > WR) continue;
    Col c;
    if (d > WR - 4 || d < 7) c = C_GOLD;
    else {
      float a = atan2f((float)j, (float)i);
      if (a < 0) a += PI2;
      int k = (int)(a / PI2 * n);
      if (k >= n) k = n - 1;
      c = cols[k];
    }
    p->r = c.r; p->g = c.g; p->b = c.b; p->on = 1;
  }
}

static void drawWheel(const Px *img, int cx, int cy, float rot) {
  float c = cosf(rot), s = sinf(rot);
  for (int j = -WR; j <= WR; j++) for (int i = -WR; i <= WR; i++) {
    float sx = i * c + j * s, sy = -i * s + j * c;
    int ix = (int)lroundf(sx) + WR, iy = (int)lroundf(sy) + WR;
    if (ix < 0 || iy < 0 || ix >= WS || iy >= WS) continue;
    const Px *p = &img[iy * WS + ix];
    if (!p->on) continue;
    put(cx + i, cy + j, (Col){p->r, p->g, p->b});
  }
}

static void pointer(int cx, int cy) {
  for (int k = 0; k < 10; k++) {
    int hw = (10 - k) / 2 + 1;
    rect(cx - hw, cy - WR - 12 + k, 2 * hw + 1, 1, C_WHT);
  }
}

static void ball(int cx, int cy, float a) {
  disc(cx + (int)(BR * cosf(a)), cy + (int)(BR * sinf(a)), 3, C_WHT);
}

static void drawTable(int sel, int placed) {
  for (int i = 0; i < 13; i++) {
    int y = 12 + i * 17;
    rect(186, y, 116, 13, rowCol[i]);
    if (i == sel) boxf(183, y - 2, 122, 17, 2, C_GOLD);
    if (i == placed) disc(300, y + 6, 4, C_WHT);
  }
}

static void drawChips(long bet) {
  long n = bet / 200;
  if (n > 12) n = 12;
  for (int i = 0; i < n; i++) disc(14 + i * 12, 226, 4, C_GOLD);
}

static void legend(void) {
  for (int i = 0; i < 8; i++) rect(20 + i * 36, 212, 30, 12, pcols[i]);
}

static void drawRace(const int *pos, int sel) {
  bgFelt();
  for (int i = 0; i < 6; i++) {
    int y = 18 + i * 34;
    int p = pos[i] > RLEN ? RLEN : pos[i];
    int x = 14 + p * 280 / RLEN;
    rect(12, y + 27, 296, 1, C_GOLD);
    rect(x, y + 8, 14, 10, hcols[i]);
    rect(x + 12, y + 3, 6, 7, hcols[i]);
    rect(x + 2, y + 18, 3, 6, hcols[i]);
    rect(x + 9, y + 18, 3, 6, hcols[i]);
  }
  rect(296, 14, 3, 206, C_WHT);
  if (sel >= 0) boxf(4, 14 + sel * 34, 312, 34, 1, C_GOLD);
}

/* ---------- Spielstand ---------- */
static void save(void) {
  FILE *f = fopen(SAVE, "wb");
  if (f) { fwrite(&chips, sizeof chips, 1, f); fwrite(&lastWheel, sizeof lastWheel, 1, f); fclose(f); }
}
static void load(void) {
  FILE *f = fopen(SAVE, "rb");
  if (f) {
    if (fread(&chips, sizeof chips, 1, f) != 1) chips = 5000;
    if (fread(&lastWheel, sizeof lastWheel, 1, f) != 1) lastWheel = 0;
    fclose(f);
  }
  if (chips < 100) chips = 1000;
}
static void clampBet(long *b) {
  if (*b > chips) *b = chips;
  if (*b < 100) *b = 100;
  if (chips < 100) *b = chips;
}
static void bonus(char *msg) {
  if (chips < 100) { chips += 1000; strcat(msg, " | Pit-Boss: +1000 Chips"); }
}

/* ---------- Roulette ---------- */
static int isRed(int n) {
  static const int r[] = {1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36};
  for (int i = 0; i < 18; i++) if (r[i] == n) return 1;
  return 0;
}
static int payout(int t, int num, int n) {
  if (t == 12) return n == num ? 35 : 0;
  if (n < 1 || n > 36) return 0;
  switch (t) {
    case 0: return isRed(n) ? 1 : 0;
    case 1: return !isRed(n) ? 1 : 0;
    case 2: return n % 2 ? 1 : 0;
    case 3: return !(n % 2) ? 1 : 0;
    case 4: return n <= 18 ? 1 : 0;
    case 5: return n >= 19 ? 1 : 0;
    case 6: return n <= 12 ? 2 : 0;
    case 7: return (n > 12 && n <= 24) ? 2 : 0;
    case 8: return n > 24 ? 2 : 0;
    default: return ((n - 1) % 3 == t - 9) ? 2 : 0;
  }
}
static const char *numName(int n) {
  static char b[4];
  if (n == 37) return "00";
  snprintf(b, sizeof b, "%d", n);
  return b;
}
static void rouletteTop(int t, int num, long bet, int shown, const char *msg) {
  consoleClear();
  printf("=== ROULETTE ===\n\nChips:   %ld\nEinsatz: %ld\n\n", chips, bet);
  printf("Wette:   %s\n", bn[t]);
  if (t == 12) printf("Zahl:    %s\n", numName(num));
  if (shown >= 0) printf("Kugel:   %s\n", numName(shown));
  printf("\n%s\n\nLinks/Rechts: Wette\nHoch/Runter: Zahl\nL/R: Einsatz  X: +1000  Y: All-In\nA: Drehen   B: Zurueck\n", msg);
}
static void spinRoulette(int win, int placed, long bet) {
  const int F = 260;
  float rest = -PI / 2 - (win + 0.5f) / 38 * PI2;
  for (int f = 0; f <= F && aptMainLoop(); f++) {
    float e = 1 - powf(1 - (float)f / F, 3);
    float rot = rest - 8 * PI2 * (1 - e);
    float ba = -PI / 2 + 9 * PI2 * (1 - e);
    beginFrame();
    bgFelt();
    drawWheel(rouletteImg, 94, 118, rot);
    pointer(94, 118);
    ball(94, 118, ba);
    drawTable(-1, placed);
    drawChips(bet);
    endFrame();
  }
  rRot = rest;
}
static void roulette(void) {
  int t = 0, num = 17, placed = -1, shown = -1, changed = 1;
  long bet = 100;
  char msg[96] = "Viel Glueck!";
  while (aptMainLoop()) {
    hidScanInput();
    u32 k = hidKeysDown();
    if (k & KEY_B) return;
    if (k & KEY_RIGHT) { t = (t + 1) % 13; changed = 1; }
    if (k & KEY_LEFT) { t = (t + 12) % 13; changed = 1; }
    if (k & KEY_UP) { num = (num + 1) % 38; changed = 1; }
    if (k & KEY_DOWN) { num = (num + 37) % 38; changed = 1; }
    if (k & KEY_R) { bet += 100; changed = 1; }
    if (k & KEY_L) { bet -= 100; changed = 1; }
    if (k & KEY_X) { bet += 1000; changed = 1; }
    if (k & KEY_Y) { bet = chips; changed = 1; }
    clampBet(&bet);
    if (k & KEY_A) {
      chips -= bet;
      int win = rand() % 38;
      placed = t;
      spinRoulette(win, placed, bet);
      int m = payout(t, num, win);
      if (m) {
        chips += bet * (m + 1);
        snprintf(msg, sizeof msg, "Gewonnen! +%ld", bet * m);
      } else {
        snprintf(msg, sizeof msg, "Verloren -%ld", bet);
      }
      bonus(msg);
      save();
      clampBet(&bet);
      shown = win;
      placed = -1;
      changed = 1;
    }
    if (changed) { rouletteTop(t, num, bet, shown, msg); changed = 0; }
    beginFrame();
    bgFelt();
    drawWheel(rouletteImg, 94, 118, rRot);
    pointer(94, 118);
    ball(94, 118, -PI / 2);
    drawTable(t, placed);
    drawChips(bet);
    endFrame();
  }
}

/* ---------- Pferderennen ---------- */
static void topHorses(int h, long bet, const char *msg) {
  consoleClear();
  printf("=== PFERDERENNEN ===\n\nChips: %ld   Einsatz: %ld\n\n", chips, bet);
  for (int i = 0; i < 6; i++)
    printf("%s%d %-13s %2d:1\n", i == h ? ">" : " ", i + 1, hn[i], ho[i] - 1);
  printf("\n%s\n\nLinks/Rechts: Pferd\nL/R: Einsatz  X: +1000  Y: All-In\nA: Rennen   B: Zurueck\n", msg);
}
static void horses(void) {
  int h = 0, changed = 1, pos[6] = {0};
  long bet = 100;
  char msg[96] = "Waehle dein Pferd!";
  while (aptMainLoop()) {
    hidScanInput();
    u32 k = hidKeysDown();
    if (k & KEY_B) return;
    if (k & KEY_RIGHT) { h = (h + 1) % 6; changed = 1; }
    if (k & KEY_LEFT) { h = (h + 5) % 6; changed = 1; }
    if (k & KEY_R) { bet += 100; changed = 1; }
    if (k & KEY_L) { bet -= 100; changed = 1; }
    if (k & KEY_X) { bet += 1000; changed = 1; }
    if (k & KEY_Y) { bet = chips; changed = 1; }
    clampBet(&bet);
    if (k & KEY_A) {
      chips -= bet;
      memset(pos, 0, sizeof pos);
      int win = -1;
      while (win < 0 && aptMainLoop()) {
        for (int i = 0; i < 6; i++) if (rand() % 100 < hp[i]) pos[i]++;
        for (int i = 0; i < 6; i++)
          if (pos[i] >= RLEN && (win < 0 || pos[i] > pos[win])) win = i;
        beginFrame();
        drawRace(pos, h);
        endFrame();
      }
      if (win >= 0) {
        if (win == h) {
          chips += bet * ho[h];
          snprintf(msg, sizeof msg, "%s gewinnt! +%ld", hn[win], bet * (ho[h] - 1));
        } else {
          snprintf(msg, sizeof msg, "%s gewinnt. -%ld", hn[win], bet);
        }
        bonus(msg);
        save();
      }
      clampBet(&bet);
      changed = 1;
    }
    if (changed) { topHorses(h, bet, msg); changed = 0; }
    beginFrame();
    drawRace(pos, h);
    endFrame();
  }
}

/* ---------- Glücksrad ---------- */
static void topWheel(const char *msg, long left) {
  consoleClear();
  printf("=== GLUECKSRAD ===\n\nChips: %ld\n\n%s\n\n", chips, msg);
  if (left > 0) printf("Naechster Dreh in %ldh %ldm\n", left / 3600, (left % 3600) / 60);
  else printf("Dein Dreh ist bereit! (A)\n");
  printf("\nB: Zurueck\n");
}
static void spinPrize(int win) {
  const int F = 240;
  float rest = -PI / 2 - (win + 0.5f) / 8 * PI2;
  for (int f = 0; f <= F && aptMainLoop(); f++) {
    float e = 1 - powf(1 - (float)f / F, 3);
    float rot = rest - 6 * PI2 * (1 - e);
    beginFrame();
    bgFelt();
    drawWheel(prizeImg, 160, 118, rot);
    pointer(160, 118);
    legend();
    endFrame();
  }
  pRot = rest;
}
static void wheel(void) {
  int changed = 1;
  char msg[96] = "Dreh das Gluecksrad!";
  while (aptMainLoop()) {
    hidScanInput();
    u32 k = hidKeysDown();
    if (k & KEY_B) return;
    long left = 86400 - (long)(time(NULL) - lastWheel);
    if ((k & KEY_A) && left <= 0) {
      int r = rand() % 100, s = 0, w;
      for (w = 0; w < 8; w++) { s += pw[w]; if (r < s) break; }
      if (w > 7) w = 7;
      spinPrize(w);
      chips += pz[w];
      lastWheel = time(NULL);
      save();
      snprintf(msg, sizeof msg, "Gewonnen: %ld Chips!", pz[w]);
      changed = 1;
    }
    if (changed) { topWheel(msg, left); changed = 0; }
    beginFrame();
    bgFelt();
    drawWheel(prizeImg, 160, 118, pRot);
    pointer(160, 118);
    legend();
    endFrame();
  }
}

/* ---------- Menü ---------- */
static void topMenu(int c) {
  const char *items[3] = {"Roulette", "Pferderennen", "Gluecksrad"};
  consoleClear();
  printf("\n   DIAMOND CASINO 3DS\n\n   Chips: %ld\n\n", chips);
  for (int i = 0; i < 3; i++) printf("   %s %s\n", i == c ? ">" : " ", items[i]);
  printf("\n   Hoch/Runter: Auswahl\n   A: Starten   START: Beenden\n\n   Spielstand wird gespeichert.\n");
}
static void drawMenu(int c) {
  bgFelt();
  for (int i = 0; i < 3; i++) {
    int x = 20 + i * 100, y = 50;
    rect(x, y, 90, 130, i == 0 ? C_RED : (i == 1 ? C_BLUE : C_PURP));
    if (i == c) boxf(x - 4, y - 4, 98, 138, 3, C_GOLD);
  }
  disc(65, 115, 26, C_BLK);
  disc(65, 115, 20, C_GRN);
  disc(65, 115, 4, C_GOLD);
  rect(140, 105, 50, 20, C_WHT);
  disc(265, 115, 26, C_GOLD);
  disc(265, 115, 12, C_PINK);
}

static void buildAll(void) {
  for (int i = 0; i < 38; i++) {
    if (i == 0 || i == 37) rcols[i] = (Col){0, 150, 75};
    else if (isRed(i)) rcols[i] = (Col){205, 25, 35};
    else rcols[i] = (Col){25, 25, 30};
  }
  buildWheel(rouletteImg, 38, rcols);
  buildWheel(prizeImg, 8, pcols);
}

int main(void) {
  gfxInitDefault();
  consoleInit(GFX_TOP, &top);
  gfxSetDoubleBuffering(GFX_BOTTOM, true);
  srand(time(NULL));
  load();
  buildAll();
  int c = 0, dirty = 1;
  while (aptMainLoop()) {
    hidScanInput();
    u32 k = hidKeysDown();
    if (k & KEY_START) break;
    if (k & KEY_DOWN) { c = (c + 1) % 3; dirty = 1; }
    if (k & KEY_UP) { c = (c + 2) % 3; dirty = 1; }
    if (k & KEY_A) {
      if (c == 0) roulette();
      else if (c == 1) horses();
      else wheel();
      dirty = 1;
    }
    if (dirty) { topMenu(c); dirty = 0; }
    beginFrame();
    drawMenu(c);
    endFrame();
  }
  save();
  gfxExit();
  return 0;
}
