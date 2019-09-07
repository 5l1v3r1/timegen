/**

	Author: Bastian Born <info@bastianborn.de>
	Version: 0.3
	
	This program generates a *.wav file to "send" an own time signal to DCF77 compatible devices.
	Please note, that this is just a proof of concept.
	
	Caution: Maybe it is not legal to play these wave files in your country.
**/

#define __USE_POSIX2

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define SAMPLE_RATE 192000
#define FREQUENCY	77500

#ifndef M_PI
#    define M_PI 3.1415926535897932384626
#endif


// helper
int bcd1(int val, int pos)
{
	val %= 10;
	return ((val >> pos) & 1) == 1 ? true : false;
}

int bcd10(int val, int pos)
{
	val /= 10;
	return bcd1(val, pos);
}

int parity(bool* values, int length)
{
	bool is_even = true;

	for(int i = 0; i < length; i++)
	{
		if(values[i]) is_even = !is_even;
	}

	return !is_even;
}

// generates the signal data
void generate_minute(struct tm* minute, bool* values)
{
	memset(values, 0, 59); // set everything on false
	
	// convert some values from tm to dcf77
	int month = minute->tm_mon + 1;
	int year = minute->tm_year % 100;
	
	// dcf77 starts on monday (range 1-7), tm starts on sunday (range 0-6)
	int weekday = minute->tm_wday == 0 ? 7 : minute->tm_wday;
	
	// set values
	
	values[17] = minute->tm_isdst; // CEST ?
	values[18] = !minute->tm_isdst; // CET ?
	
	values[19] = false; // no leapsecond

	values[20] = true; // time marker

	values[21] = bcd1(minute->tm_min, 0); // min
	values[22] = bcd1(minute->tm_min, 1); // min
	values[23] = bcd1(minute->tm_min, 2); // min
	values[24] = bcd1(minute->tm_min, 3); // min
	values[25] = bcd10(minute->tm_min, 0); // min
	values[26] = bcd10(minute->tm_min, 1); // min
	values[27] = bcd10(minute->tm_min, 3); // min
	values[28] = parity(values + 21, 8);

	values[29] = bcd1(minute->tm_hour, 0); // hour
	values[30] = bcd1(minute->tm_hour, 1); // hour
	values[31] = bcd1(minute->tm_hour, 2); // hour
	values[32] = bcd1(minute->tm_hour, 3); // hour
	values[33] = bcd10(minute->tm_hour, 0); // hour
	values[34] = bcd10(minute->tm_hour, 1); // hour
	values[35] = parity(values + 29, 6);

	values[36] = bcd1(minute->tm_mday, 0); // day
	values[37] = bcd1(minute->tm_mday, 1); // day
	values[38] = bcd1(minute->tm_mday, 2); // day
	values[39] = bcd1(minute->tm_mday, 3); // day
	values[40] = bcd10(minute->tm_mday, 0); // day
	values[41] = bcd10(minute->tm_mday, 1); // day

	// day of week
	values[42] = weekday & 1;
	values[43] = (weekday >> 1) & 1;
	values[44] = (weekday >> 2) & 1;

	values[45] = bcd1(month, 0); // month
	values[46] = bcd1(month, 1); // month
	values[47] = bcd1(month, 2); // month
	values[48] = bcd1(month, 3); // month
	values[49] = bcd10(month, 0); // month

	values[50] = bcd1(year, 0); // year
	values[51] = bcd1(year, 1); // year
	values[52] = bcd1(year, 2); // year
	values[53] = bcd1(year, 3); // year
	values[54] = bcd10(year, 0); // year
	values[55] = bcd10(year, 1); // year
	values[56] = bcd10(year, 2); // year
	values[57] = bcd10(year, 3); // year

	values[58] = parity(values + 36, 22);
}

// writer functions
bool write_wav_header(FILE* fp, unsigned long samples )
{
	unsigned long data_size = samples * 2;
	unsigned long chunk_size = data_size + 36; // 8 bit per sample
	unsigned long subchunk_size = 16;
	unsigned short type = 1; // ieee float
	unsigned short channels = 1; // mono
	unsigned long sample_rate = SAMPLE_RATE;
	unsigned long byte_rate = SAMPLE_RATE * 2;
	unsigned short block_align = 1;
	unsigned short bits_per_sample = 16;

	// basic header
	fwrite("RIFF", 4, 1, fp);
	fwrite(&chunk_size, 4, 1, fp);
	fwrite("WAVE", 4, 1, fp);
	
	// subchunk "fmt "
	fwrite("fmt ", 4, 1, fp);
	fwrite(&subchunk_size, 4, 1, fp); // subchunk size
	fwrite(&type, 2, 1, fp); 
	fwrite(&channels, 2, 1, fp); 
	fwrite(&(sample_rate), 4, 1, fp); // sample rate
	fwrite(&(byte_rate), 4, 1, fp); // byte rate
	fwrite(&(block_align), 2, 1, fp); // block align
	fwrite(&(bits_per_sample), 2, 1, fp); // bits per sample
	
	// subchunk "data"
	fwrite("data", 4, 1, fp);
	fwrite(&data_size, 4, 1, fp);
	
	return true; // todo: check the fwrites ;-)
}

bool write_minute(FILE* fp, struct tm* minute)
{
	bool values[59];
	short buffer[2048]; // 4k
	unsigned int bufpos = 0;

	// generate booleans for this minute
	generate_minute(minute, values);

	// write every second
	for(int s = 0; s < 60; s++)
	{
		for(long i = 0; i < SAMPLE_RATE; i++)
		{
			double ampFactor = 1;
			double sec_progress = (double)i / SAMPLE_RATE;
			
			if(s < 59)
			{
				if(values[s])
				{
					if(sec_progress < 0.2) ampFactor = 0;
				}
				else
				{
					if(sec_progress < 0.1) ampFactor = 0;
				}
			}
			
			double carrier = sin(sec_progress * FREQUENCY * M_PI * 2); // oscillator
			short sample = (short)(carrier * ampFactor * 32767.0);
			buffer[bufpos] = sample;
			
			// flush buffer
			if(++bufpos == 2048)
			{
				fwrite(buffer, sizeof(buffer), 1, fp);
				bufpos = 0;
			}
		}
	}
	
	// final buffer flush
	if(bufpos > 0)
		fwrite(buffer, (bufpos << 1), 1, fp);
	
	return true;
}

bool generate_file(const char* path, struct tm* target_time_info, unsigned int minutes)
{
	bool ret = false;
	
	FILE* fp = fopen(path, "w+b");
	if(fp)
	{
		time_t target_time;
		int samples = SAMPLE_RATE * 60 * minutes;
		target_time = mktime(target_time_info);
		
		if(write_wav_header(fp, samples))
		{
			for(int i = 0; i < minutes; i++)
			{
				struct tm* timeinfo = localtime(&target_time);
				write_minute(fp, timeinfo);
				target_time += 60;
			}
			ret = true;
		}
		fclose(fp);
	}
	else
	{
		fprintf(stderr, "Error: Cannot open file for write.\n\n");
	}
	
	return ret;
}

// input parser for time
bool parse_time(char* str, struct tm* out)
{
	// validation stage #1
	if(	strlen(str) != 16 ||
		str[4] != '-' ||
		str[7] != '-' ||
		str[10] != '.' ||
		str[13] != ':')
	{
		fprintf(stderr, "Error: Please check the time format.\n\n");
		return false;
	}
	
	// parse
	unsigned int y,m,d,hour,min;
	sscanf ( str, "%u-%u-%u.%u:%u", &y, &m, &d, &hour, &min );
	
	// validation stage #2
	if(	y < 1900 ||
		y > 9999 ||
		m > 12 ||
		m == 0 ||
		d > 31 ||
		d == 0 ||
		hour > 23 ||
		min > 59)
	{
		fprintf(stderr, "Error: Please check the given time.\n\n");
		return false;
	}
	
	// fill tm
	out->tm_mday = d;
	out->tm_mon = m - 1;
	out->tm_year = y - 1900;
	out->tm_hour = hour;
	out->tm_min = min;
	
	return true;
}

void usage()
{
	printf("Usage: timegen [-o outputfile] [-t time] [-m minutes]\n\n");
	printf("-o\tSpecifies the target path of the wav file\n");
	printf("-t\tTarget time. Format: YYYY-MM-DD.hh:mm\n");
	printf("-m\tCount of generated minutes.\n\tA common clock needs at least 3 to 4 minutes to sync.\n\n");
	printf("Example: timegen -o dcf77.wav -t 2014-05-29.13:42 -m 10\n\n");
} 

int main(int argc, char **argv)
{
	char target_path[1024] = { 0 };
	char time[17] = { 0 };
	int minutes = 0;
	int c;
	
	while ((c = getopt (argc, argv, "o:t:m:")) != -1)
	{
		switch(c)
		{
			case 'o':
				snprintf(target_path, sizeof(target_path), "%s", optarg);
				break;
				
			case 't':
				snprintf(time, sizeof(time), "%s", optarg);
				break;
				
			case 'm':
				minutes = atoi(optarg);
				break;
				
			default:
				usage();
				return 0;
		}
	}
	
	struct tm target_time;
	memset(&target_time, 0, sizeof(struct tm));

	if(minutes > 0 && minutes <= 80 && *target_path != 0 && parse_time(time, &target_time))
	{
		printf("Starting flux capacitor... This may take a few seconds.\n");
		if(generate_file(target_path, &target_time, minutes))
		{
			printf("Done. File written: %s\n", target_path);
		}
	}
	else
	{
		if(minutes > 80) fprintf(stderr, "Error: Cannot generate more than 80 minutes per file.\n\n");
		usage();
	}
	
	return 0;
}
