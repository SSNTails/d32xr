typedef struct
{
	int     score;
	int     exiting;
	VINT    deadTimer;
	VINT    rings;
	VINT    lives;

	boolean intermission;
} stbar_t;

extern	stbar_t	*stbar;
extern	int stbar_tics;
extern void valtostr(char *string,int val);
void ST_InitEveryLevel(void);


