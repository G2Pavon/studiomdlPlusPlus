#include "cmdlib.h"

#define MAXTOKEN 512

extern char token[MAXTOKEN];
extern char *script_p;
extern int scriptline;
extern qboolean endofscript;

void LoadScriptFile(char *filename);

qboolean GetToken(qboolean crossline);
qboolean TokenAvailable(void);
