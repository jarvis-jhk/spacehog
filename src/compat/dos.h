/* DOS/DJGPP runtime pieces the game depends on. */
#ifndef SH_DOS_H
#define SH_DOS_H

int dos_rand(void);
void dos_srand(unsigned int seed);

/* conio (text mode setup screen) */
/* conio cprintf: writes with the current text attribute */
void cprintf(const char *fmt, ...);
/* stdout printf on DOS: BIOS teletype output, keeps the attribute already in each cell */
void tprintf(const char *fmt, ...);
void gotoxy(int x, int y);
void textcolor(int c);
void textbackground(int c);
void clreol(void);
void clrscr(void);

void sh_screenshot(const char *tag);

#endif
