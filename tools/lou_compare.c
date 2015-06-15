#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include <unistd.h>
#include <fcntl.h>
#include "liblouis.h"
#include "louis.h"

#ifdef _WIN32
#define S_IRUSR 0
#define S_IWUSR 0
#endif

#define BUF_MAX  27720

static widechar inputText[BUF_MAX], output1Text[BUF_MAX], output2Text[BUF_MAX],
                expectText[BUF_MAX], expectDots[BUF_MAX],
                empText[BUF_MAX], etnText[BUF_MAX], tmpText[BUF_MAX];
static int inputLen, output1Len, output2Len, expectLen, empLen = 0, etnLen = 0, etnHave, tmpLen;
static formtype emphasis[BUF_MAX];
static formtype emp1[BUF_MAX], emp2[BUF_MAX];
static const char table_default[] = "en-ueb-g2.ctb", *table = table_default;
static char *charText, inputLine[BUF_MAX], origInput[BUF_MAX], origEmp[BUF_MAX], origEtn[BUF_MAX];
static FILE *input, *passFile;
static int inputPos[BUF_MAX], outputPos[BUF_MAX];
static int failFile, outFile;
static int pass_cnt = 0, fail_cnt = 0;
static int blank_out = 0, blank_pass = 1, blank_fail = 0;
static int out_more = 0, out_pos = 0, in_line = 0, paused = 0;

static unsigned short uni = 0xfeff, space = 0x0020, dash = 0x002d,
	              bar = 0x007c, plus = 0x002b, tab = 0x0009;
static unsigned int nl = 0x000a000d;//TODO:  doall.pl chokes on ueb-08

widechar underText[BUF_MAX];
char underLine[BUF_MAX];
int underLen;
widechar boldText[BUF_MAX];
char boldLine[BUF_MAX];
int boldLen;

static void trimLine(char *line)
{
	char *crs = line;
	while(*crs)
	if(*crs == '\n' || *crs == '\r')
	{
		*crs = 0;
		crs--;
//		while(crs > line && (*crs == ' ' || *crs == '\t'))
//		{
//			*crs = 0;
//			crs--;
//		}
		return;
	}
	else
		crs++;
}

static void addSlashes(char *line)
{
	char *sft, *crs = line;
	while(*crs)
	{
		if(*crs == '\\')
		{
			sft = crs;
			while(*sft)
				sft++;
			sft[1] = 0;
			sft--;
			while(sft > crs)
			{
				sft[1] = sft[0];
				sft--;
			}
			sft[1] = '\\';
			crs++;
		}
		crs++;
	}	
}

static int inputEmphasis(typeforms type, char *line, widechar *text, int *len)
{
	int i;
	
	if(fgets(inputLine, BUF_MAX - 97, input))
	{
		in_line++;
		trimLine(inputLine);
		strcpy(line, inputLine);
		*len = 0;
		for(i = 0; inputLine[i]; i++)
		{
			(*len)++;
			if(inputLine[i] == '+')
				emphasis[i] |= type;
		}
		if(*len)
			*len = extParseChars(line, text);
		return 1;
	}
	else
	{
		fprintf(stderr, "ERROR:  unexpected on of file, #%d\n", in_line);
		return 0;
	}
}

static void outputEmphasis(const int file, const int one, const char *token, const widechar *text, const int len)
{
	tmpLen = extParseChars(token, tmpText);
	write(file, tmpText, tmpLen * 2);
	if(!one)
		write(file, &nl, 2);
	write(file, text, len * 2);
	write(file, &nl, 2);
}

int main(int argn, char **args)
{	
	int i;
	
	input = stdin;
	for(i = 1; args[i]; i++)
	if(args[i][0] == '-' && args[i][1] == 'f')
	{
		i++;
		input = fopen(args[i], "r");
		if(!input)
		{
			fprintf(stderr, "ERROR:  cannot open input file %s\n", args[i]);
			return 1;
		}
	}
	else if(args[i][0] == '-' && args[i][1] == 't')
	{
		i++;
		table = args[i];
	}
	else if(args[i][0] == '-' && args[i][1] == 'm')
		out_more = 1;
	else if(args[i][0] == '-' && args[i][1] == 'p')
		out_pos = 1;
	
	passFile = fopen("pass.txt", "w");
	outFile = open("output.txt", O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	write(outFile, &uni, 2);
	//write(outFile, &nl, 2);
	failFile = open("fail.txt", O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	write(failFile, &uni, 2);
	//write(failFile, &nl, 2);
	
	memset(emphasis, 0, BUF_MAX);
	
	while(fgets(inputLine, BUF_MAX - 97, input))
	{
		in_line++;
		trimLine(inputLine);
			
		if(!strncmp("~end", inputLine, 4))
			break;
			
		if(paused)
		{
			if(!strncmp("~start", inputLine, 6))
				paused = 0;
			continue;
		}
		if(!strncmp("~stop", inputLine, 5))
		{
			paused = 1;
			continue;
		}
		
		if(!inputLine[0])
		{
//			if(blank_pass)
//			{
//				fprintf(passFile, "\n");
//				blank_pass = 0;
//			}
			if(!blank_fail)
			{
				write(failFile, &nl, 2);
				blank_fail = 1;
			}
			if(!blank_out)
			{
				write(outFile, &nl, 2);
				blank_out = 1;
			}
			continue;
		}
		
		if(inputLine[0] == '#')
		{
			if(inputLine[1] == '#')
				continue;
			addSlashes(inputLine);	
			inputLen = extParseChars(inputLine, inputText);
			if(inputLine[1] != '~')
			{
				if(!blank_pass)
				{
					blank_pass = 1;
					//fprintf(passFile, "\n", inputLine);
				}
				//fprintf(passFile, "%s\n", inputLine);	
				write(outFile, inputText, inputLen * 2);
				write(outFile, &nl, 2);
				blank_out = 0;
			}
			write(failFile, inputText, inputLen * 2);
			write(failFile, &nl, 2);
			blank_fail = 0;
			continue;
		}			

		if(!strncmp("~emp", inputLine, 4))
		if(fgets(inputLine, BUF_MAX - 97, input))
		{
			in_line++;
			trimLine(inputLine);
			strcpy(origEmp, inputLine);
			memset(emphasis, 0, BUF_MAX);
			for(empLen = 0; inputLine[empLen]; empLen++)
				emphasis[empLen] = inputLine[empLen] - '0';
			emphasis[empLen] = 0;
			if(empLen)
				empLen = extParseChars(origEmp, empText);
			continue;
		}
		else
		{
			fprintf(stderr, "ERROR:  unexpected on of file, #%d\n", in_line);
			return 1;
		}
		

		if(!strncmp("~etn", inputLine, 4))
		if(fgets(inputLine, BUF_MAX - 97, input))
		{
			in_line++;
			trimLine(inputLine);
			strcpy(origEtn, inputLine);
			etnLen = etnHave = 0;
			for(i = 0; inputLine[i]; i++)
			{
				etnLen++;
				if(inputLine[i] - '0')
				{
					etnHave = 1;
					emphasis[i] |= (inputLine[i] - '0') << 8;
				}
			}
			//emphasis[i] = 0;
			if(etnHave)
				etnLen = extParseChars(origEtn, etnText);
			else
				etnLen = 0;
			continue;
		}
		else
		{
			fprintf(stderr, "ERROR:  unexpected on of file, #%d\n", in_line);
			return 1;
		}
		
		if(!strncmp("~under", inputLine, 6))
		if(inputEmphasis(underline, underLine, underText, &underLen))
			continue;
		else
			return 1;
		
		if(!strncmp("~bold", inputLine, 5))
		if(inputEmphasis(bold, boldLine, boldText, &boldLen))
			continue;
		else
			return 1;
		
		memcpy(emp1, emphasis, BUF_MAX * sizeof(formtype));
		memcpy(emp2, emphasis, BUF_MAX * sizeof(formtype));
			
		strcpy(origInput, inputLine);
		addSlashes(inputLine);	
		memset(inputText, 0, BUF_MAX * sizeof(widechar));	
		inputLen = extParseChars(inputLine, inputText);
		
		expectLen = 0;
		if(fgets(inputLine, BUF_MAX - 97, input))
		{
			in_line++;
			trimLine(inputLine);
			if(!strncmp("~emp", inputLine, 4))
			{
				fprintf(stderr, "ERROR:  emphasis where expected line should be at #%d\n", in_line);
				return 1;
			}
			
			if(inputLine[0])
			{
				addSlashes(inputLine);	
				expectLen = extParseChars(inputLine, expectText);
			}
		}
		
		i = inputLen;
		output1Len = BUF_MAX;
		lou_translate(
			table,
			inputText,
			&inputLen,
			output1Text,
			&output1Len,
			emp1,
			NULL,
			inputPos,
			outputPos,
			NULL,
			0);
		inputLen = i;
		
		if(out_more)
		{	i = inputLen;
			output2Len = BUF_MAX;
			lou_translate(
				table,
				inputText,
				&inputLen,
				output2Text,
				&output2Len,
				emp2,
				NULL,
				NULL,
				NULL,
				NULL,
				dotsIO | ucBrl);
			inputLen = i;
		}
			
		if(!expectLen)
		{
			if(empLen)
			{
				write(outFile, empText, empLen * 2);
				write(outFile, &nl, 2);
			}
			if(etnLen)
			{
				write(outFile, etnText, etnLen * 2);
				write(outFile, &nl, 2);
			}
			if(boldLen)
				outputEmphasis(outFile, 0, "~bold", boldText, boldLen);
			if(underLen)
				outputEmphasis(outFile, 0, "~under", underText, underLen);
			write(outFile, inputText, inputLen * 2);
			if(out_pos)
			{
				char buf[7];
				write(outFile, &nl, 2);
				for(i = 0; i < inputLen; i++)
				if(inputPos[i] < 10)
				{
					buf[0] = inputPos[i] + '0';
					buf[1] = 0;
					tmpLen = extParseChars(buf, tmpText);
					write(outFile, tmpText, tmpLen * 2);
				}
				else if(inputPos[i] < 36)
				{
					buf[0] = (inputPos[i] - 10) + 'a';
					buf[1] = 0;
					tmpLen = extParseChars(buf, tmpText);
					write(outFile, tmpText, tmpLen * 2);
				}
				else
					write(outFile, &plus, 2);
			}
			write(outFile, &nl, 2);
			
			write(outFile, output1Text, output1Len * 2);
			if(out_pos)
			{
				char buf[7];
				write(outFile, &nl, 2);
				for(i = 0; i < output1Len; i++)
				if(outputPos[i] < 10)
				{
					buf[0] = outputPos[i] + '0';
					buf[1] = 0;
					tmpLen = extParseChars(buf, tmpText);
					write(outFile, tmpText, tmpLen * 2);
				}
				else if(outputPos[i] < 36)
				{
					buf[0] = (outputPos[i] - 10) + 'a';
					buf[1] = 0;
					tmpLen = extParseChars(buf, tmpText);
					write(outFile, tmpText, tmpLen * 2);
				}
				else
					write(outFile, &plus, 2);
			}
			write(outFile, &nl, 2);
			
			if(out_more)
			{
				write(outFile, output2Text, output2Len * 2);
				write(outFile, &nl, 2);
			}
			
			write(outFile, &nl, 2);
			blank_out = 0;
		}
		else if(   expectLen != output1Len
		        || memcmp(output1Text, expectText, expectLen * 2))
		{
			fail_cnt++;
				tmpLen = extParseChars("in:     ", tmpText);
			write(failFile, tmpText, tmpLen * 2);
			write(failFile, inputText, inputLen * 2);
			write(failFile, &nl, 2);
			if(empLen)
			{
				tmpLen = extParseChars("emp:    ", tmpText);
				write(failFile, tmpText, tmpLen * 2);
				write(failFile, empText, empLen * 2);
				write(failFile, &nl, 2);
			}
			if(etnLen)
			{
				tmpLen = extParseChars("etn:    ", tmpText);
				write(failFile, tmpText, tmpLen * 2);
				write(failFile, etnText, etnLen * 2);
				write(failFile, &nl, 2);
			}
			if(underLen)
				outputEmphasis(failFile, 1, "under:  ", underText, underLen);
			if(boldLen)
				outputEmphasis(failFile, 1, "bold:  ", boldText, boldLen);
			
				tmpLen = extParseChars("ueb:    ", tmpText);
			write(failFile, tmpText, tmpLen * 2);
			write(failFile, expectText, expectLen * 2);
			write(failFile, &nl, 2);
			
			if(out_more)
			{
				tmpLen = extParseChars("        ", tmpText);
				write(failFile, tmpText, tmpLen * 2);
				if(lou_charToDots("en-ueb-g2.ctb", expectText, tmpText, expectLen, ucBrl))
					write(failFile, tmpText, expectLen * 2);
				else
				{
					tmpLen = extParseChars("FAIL", tmpText);
					write(failFile, tmpText, tmpLen * 2);
				}
				write(failFile, &nl, 2);
			}
			
				tmpLen = extParseChars("lou:    ", tmpText);
			write(failFile, tmpText, tmpLen * 2);
			write(failFile, output1Text, output1Len * 2);
			write(failFile, &nl, 2);
			
			if(out_more)
			{
				tmpLen = extParseChars("        ", tmpText);
				write(failFile, tmpText, tmpLen * 2);
				write(failFile, output2Text, output2Len * 2);
				write(failFile, &nl, 2);
			}
			
			blank_fail = 0;
		}
		else
		{
			pass_cnt++;
			printf("%s\n", origInput);
			fprintf(passFile, "%s\n", origInput);
			blank_pass = 0;
		}
		
		/*   clear emphasis   */
		memset(emphasis, 0, BUF_MAX);
		underLen = boldLen = empLen = etnLen = 0;
	}
	
	if(pass_cnt + fail_cnt)
	{
		float percent = (float)pass_cnt / (float)(pass_cnt + fail_cnt);
		printf("%f%%\t%d\t%d\n", percent, pass_cnt, fail_cnt);
	}
	
	fclose(passFile);
	close(outFile);
	close(failFile);

	return 0;
}