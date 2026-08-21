#include <adios.h>
#include <stdio.h>

extern unsigned char signInputTable1[];
extern unsigned char signInputTable2[];

void PrintPhonemes(unsigned char *phonemeindex, unsigned char *phonemeLength, unsigned char *stress)
{
	int i = 0;
	DEBUG_MSG("===========================================\n");

	DEBUG_MSG("Internal Phoneme presentation:\n\n");
	DEBUG_MSG(" idx    phoneme  length  stress\n");
	DEBUG_MSG("------------------------------\n");

	while((phonemeindex[i] != 255) && (i < 255))
	{
		if (phonemeindex[i] < 81)
		{
			DEBUG_MSG(" %3d      %c%c      %3d       %d\n",
			phonemeindex[i],
			signInputTable1[phonemeindex[i]],
			signInputTable2[phonemeindex[i]],
			phonemeLength[i],
			stress[i]
			);
		} else
		{
			DEBUG_MSG(" %3d      ??      %3i       %d\n", phonemeindex[i], phonemeLength[i], stress[i]);
		}
		i++;
	}
	DEBUG_MSG("===========================================\n");
	DEBUG_MSG("\n");
}

void PrintOutput(unsigned char t,
	unsigned char *flag, 
	unsigned char *f1, 
	unsigned char *f2, 
	unsigned char *f3,
	unsigned char *a1, 
	unsigned char *a2, 
	unsigned char *a3,
	unsigned char *p)
{
	int i = 0;
	DEBUG_MSG("===========================================\n");
	DEBUG_MSG("Final data for speech output, %d frames:\n", t);
	DEBUG_MSG(" flags ampl1 freq1 ampl2 freq2 ampl3 freq3 pitch\n");
	DEBUG_MSG("------------------------------------------------\n");
	while(i <= t)
	{
		DEBUG_MSG("%5d %5d %5d %5d %5d %5d %5d %5d\n", flag[i], a1[i], f1[i], a2[i], f2[i], a3[i], f3[i], p[i]);
		i++;
	}
	DEBUG_MSG("===========================================\n");

}

extern unsigned char GetRuleByte(unsigned short mem62, unsigned char Y);

void PrintRule(unsigned short offset)
{
	unsigned char i = 1;
	unsigned char A = 0;
	DEBUG_MSG("Applying rule: ");
	do
	{
		A = GetRuleByte(offset, i);
		if ((A&127) == '=') DEBUG_MSG(" -> "); else DEBUG_MSG("%c", A&127);
		i++;
	} while ((A&128)==0);
	DEBUG_MSG("\n");
}
