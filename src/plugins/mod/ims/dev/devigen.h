//struct deviceinfo;
//struct sounddevice;

typedef struct 
{
  unsigned long (*GetOpt)(const char *);
  void (*Init)(const char *);
  void (*Close)();
  int (*ProcessKey)(unsigned short);
} devaddstruct;

typedef struct devinfonode_t
{
  struct devinfonode_t *next;
  char handle[9];
  deviceinfo dev;
  devaddstruct *addprocs;
  char name[32];
  char ihandle;
  char keep;
  int linkhand;
} devinfonode;

int deviReadDevices(const char *list, devinfonode **devs);
