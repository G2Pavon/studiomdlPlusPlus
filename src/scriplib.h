#include "cmdlib.h"

#define MAXTOKEN 512

extern char token[MAXTOKEN];
extern char *script_p;
extern int scriptline;
extern bool endofscript;

void LoadScriptFile(char *filename);

bool GetToken(bool crossline);
bool TokenAvailable(void);
