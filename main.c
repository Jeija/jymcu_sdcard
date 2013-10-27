#define F_CPU 16000000UL

#include <stdio.h>
#include <string.h>	
#include <avr/io.h>	
#include <avr/eeprom.h>	
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <util/delay.h>
	
#include "mmc.h"
#include "fat.h"
#include "uart.h"
#include "systimer.h"
#include "global.h"
#include "util.h"

struct fs_entry
{
	char name[512];		// Filename
	uint8_t  attrib;	// http://www.tavi.co.uk/phobos/fat.html#file_attributes
	uint32_t cluster;	// Position on drive
	uint32_t size;		// Filesize
};

char *setlen(char *string, uint8_t len, char filler)
{
	for (uint8_t i = 0; i < len; ++i)
	{
		if (string[i] == 0x00)
		{
			for (; i < len; ++i)
				string[i] = filler;
			string[i] = 0x00;
			break;
		}
	}

	return (char *)string;
}

char *num2str(int num, uint8_t base)
{
	static char numstr[32];
	itoa(num, numstr, base);
	return numstr;
}

uint64_t str2num(char *string, uint8_t base)
{
	return strtoul(string, NULL, base);
}

void uart_input(char *input)
{
	uint8_t i = 0;
	char c;

	_delay_ms(10);
	c = uart_getc();
	while(c != '\r')
	{
		if(c)
		{
			uart_putc(c);
			input[i++] = c;
		}
		c = uart_getc();
	}
	input[i] = 0x00;
	uart_puts("\n\r");
}

uint8_t strings_equal(char *string1, char *string2)
{
	uint8_t n=0;
	while(string1[n] == string2[n])
	{
		++n;
		if (string1[n] == 0x00 && string2[n] == 0x00) return 1;
	}
	return 0;
}

uint8_t getparams(char *input, uint8_t num, uint8_t maxlen, char parameters[num][maxlen])
{
	uint8_t it = 0;
	uint8_t n = 0;
	uint8_t l = 0;

	while (input[it] != 0x00)
	{
		if (input[it] == 0x20 && l != 0)
		{
			parameters[n][l] = 0x00;
			++n;
			l = 0;
		}
		else if (input[it] != 0x20)
		{
			parameters[n][l] = input[it];
			++l;
		}

		++it;

		if (l >= 32)	return 1;
		if (n >= num)	return 2;
	}
	parameters[n][l] = 0x00;

	return 0;
}

/*
####################################################################
###				Commands			####
####################################################################
*/

void cmd_ls (uint32_t cluster)
{
	uart_puts_P("\r\n|------------------------------------------------------|");
	uart_puts_P("\r\n|Filename         | Cluster  |  Attrib  |   Filesize   |");
	uart_puts_P("\r\n|------------------------------------------------------|\r\n");

	for (uint8_t element = 1; element < 240; ++element)
	{
		struct fs_entry entry;
		entry.cluster = fat_read_dir_ent(cluster, element,
				&entry.size, &entry.attrib, (unsigned char *)entry.name);
		if (entry.cluster == 0xffff) break;
		uart_puts_P("| ");
		uart_puts(setlen(entry.name, 15, 0x20));
		uart_puts_P(" | ");
		uart_puts(setlen(num2str(entry.cluster, 16), 8, 0x20));
		uart_puts_P(" | ");
		uart_puts(setlen(num2str(entry.attrib, 16), 8, 0x20));
		uart_puts_P(" | ");
		uart_puts(setlen(num2str(entry.size, 16), 12, 0x20));
		uart_puts_P(" | ");
		uart_puts("\r\n");
	}

	uart_puts_P("|------------------------------------------------------|\r\n");
}

void cmd_cat (uint64_t cluster, uint64_t size)
{
	uart_puts_P("|-------------------------CAT--------------------------|\r\n");
	char buf[512];

	uint64_t clustersize	= size / 512;
	uint16_t restsize	= 512;

	// Datei ueber UART ausgeben
	for (uint64_t b = 0; b < clustersize + 1; ++b)
	{
		fat_read_file (cluster, (unsigned char *)buf, b);

		if (clustersize == b)
			restsize = size % 512;

		for (int a = 0; a < restsize; ++a)
		{
			uart_putc(buf[a]);
			if (buf[a] == '\n') uart_putc('\r');
		}
	}
	uart_puts_P("\r\n|-------------------------/CAT-------------------------|\r\n");
}

void string_append (char *buffer, char *string) // append string to buffer
{
	uint16_t i;
	for (i = 0; buffer[i] != 0x00; ++i); // find empty space in buffer
	for (uint16_t j = 0; string[j] != 0x00; ++j)
		buffer[i+j] = string[j];
}

void cmd_write (uint64_t cluster, uint8_t adcport, uint64_t maxsize, uint8_t speed)
{
	uart_puts_P("Writing...\r\n");

	uint16_t maxblocks = maxsize / 512;
	uint16_t block = 0;
	char c = 0x00;
	while(c != 'q')
	{
		char values[512] = {0x00};
		for (uint16_t i = 0; i<=80; ++i)
		{
			adc_request(adcport);
			uint16_t adcvalue = adc_read();
			string_append(values, num2str(adcvalue, 10));
			string_append(values, "\r\n");
			for (uint8_t d = 0; d<=speed; ++d)
				_delay_ms(3);

			uart_puts(num2str(i, 10));
			uart_puts(": ");
			uart_puts(num2str(adcvalue, 10));
			uart_puts("\r\n");
			c = uart_getc();
			if (c == 'q') break;
		}
		fat_write_file(cluster, (unsigned char*)values, block);
		++block;
		if (block >= maxblocks) break;
	}

	uart_puts_P("...done\r\n");
}

void cmd_clear (void)
{
	uart_putc (27);
	uart_putc('[');
	uart_putc('2');
	uart_putc('J');
}

void cmd_readtest (uint64_t cluster, uint64_t size)
{
	char buf[512];

	uint64_t clustersize = size / 512;

	float time_before = SYSTIME;

	// Bytes lesen
	for (uint64_t b = 0; b < clustersize + 1; ++b)
		fat_read_file (cluster, (unsigned char *)buf, b);

	float duration = SYSTIME - time_before;

	uart_puts_P("Read ");
	uart_puts(num2str(size/1000, 10));
	uart_puts_P(" kBytes in ");
	uart_puts(num2str(duration * 1000, 10));
	uart_puts_P(" milliseconds.\r\n");
	uart_puts_P("(Speed: ");
	uart_puts(num2str(((size / 1000) / duration), 10));
	uart_puts_P(" kByte/s)\r\n");
}

void cmd_time (void)
{
	uart_puts("Time in ms: ");
	uart_puts((char *)num2str(SYSTIME * 1000, 10));
	uart_puts("\r\n");
}

void cmd_help(char *topic)
{
	if (topic[0] == 0x00 || strings_equal(topic, "help"))
	{
		uart_puts_P("SD Card Tool\r\n");
		uart_puts_P("Version 1\r\n");
		uart_puts_P("Based on UlrichRadig's MMC-SD Library \r\n");
		uart_puts_P("------------------------------------- \r\n");
		uart_puts_P("____________          ________________\r\n");
		uart_puts_P("|       PB1 |--------| CS            |\r\n");
		uart_puts_P("|       PB5 |--------| MOSI = DO     |\r\n");
		uart_puts_P("|       PB6 |--------| MISO = DI     |\r\n");
		uart_puts_P("| AVR   PB7 |--------| SCLK  SD Card |\r\n");
		uart_puts_P("|       GND |--------| GND           |\r\n");
		uart_puts_P("|       +5V |--------| +5V           |\r\n");
		uart_puts_P("|___________|        |_______________|\r\n");
		uart_puts_P("\r\n");
		uart_puts_P("You don't need to connect any other wiring,\r\n");
		uart_puts_P("no need for a second GND or a +3.3V connection.\r\n");
		uart_puts_P("Actually MOSI = DI and MISO = DO, but it seems like\r\n");
		uart_puts_P("this is labelled the wrong way round on the\r\n");
		uart_puts_P("     Jiayuan JY-MCU SD Card Module V1.0\r\n");
		uart_puts_P("\r\n");
		uart_puts_P("\r\n");
		uart_puts_P("Software reference\r\n");
		uart_puts_P("Commands:\r\n");
		uart_puts_P("ls       - List all files in directory\r\n");
		uart_puts_P("cat      - Dump file to UART\r\n");
		uart_puts_P("write    - Write from ADC to a file\r\n");
		uart_puts_P("time     - Display time since system started in ms\r\n");
		uart_puts_P("readtest - Perform a read speed test on the SD Card\r\n");
		uart_puts_P("clear    - Clear the screen\r\n");
		uart_puts_P("init     - Initialize the SD Card. Only call this if it failed on boot.\r\n");
		uart_puts_P("help     - Display this help screen\r\n");
		uart_puts_P("\r\n");
		uart_puts_P("To get more information about a specific command, type\r\n");
		uart_puts_P("help [command]\r\n");		
	}
	else if (strings_equal(topic, "ls"))
	{
		uart_puts_P("ls [cluster]\r\n");
		uart_puts_P("List contents of directory at [cluster], cluster is hexadecimal.\r\n");
		uart_puts_P("Type ls without any parameter to list contents of root directory.\r\n");
	}
	else if (strings_equal(topic, "cat"))
	{
		uart_puts_P("cat [cluster] [filesize]\r\n");
		uart_puts_P("Output contents of file at [cluster] to the serial port.\r\n");
		uart_puts_P("Filesize must be provided, can also be lower than the actual\r\n");
		uart_puts_P("size of the file to not output everything, but mustn't be higher.\r\n");
		uart_puts_P("Cluster and filesize are both hexadecimal.\r\n");
	}
	else if (strings_equal(topic, "clear"))
	{
		uart_puts_P("clear\r\n");
		uart_puts_P("Clears the screen. 0x1b[2J sequence must be supported by your terminal.\r\n");
	}
	else if (strings_equal(topic, "time"))
	{
		uart_puts_P("time\r\n");
		uart_puts_P("Displays time since AVR OS boot in milliseconds.\r\n");
	}
	else if (strings_equal(topic, "write"))
	{
		uart_puts_P("write [cluster] [ADC] [maxsize] [speed]\r\n");
		uart_puts_P("Writes values from the ADC to a file at [cluster].\r\n");
		uart_puts_P("The file must be created before. [ADC] is the ADC port\r\n");
		uart_puts_P("from 1-8. If [maxsize] is provided, writing will stop after\r\n");
		uart_puts_P("the amount of clusters determined by it. [cluster] and [maxsize]\r\n");
		uart_puts_P("are both hexadecimal. [speed] is from fastest = 0 to 255 = slowest.\r\n");
		uart_puts_P("The files have to be created beforehand on a computer. The files\r\n");
		uart_puts_P("created there provide the space for the data. So make sure to create\r\n");
		uart_puts_P("large enough files on the card. If [maxsize] is not provided\r\n");
		uart_puts_P("the AVR will continue writing and may eventually erase all data\r\n");
		uart_puts_P("on the SD Card. Be beware of that and provide [maxsize]\r\n");
	}
	else if (strings_equal(topic, "init"))
	{
		uart_puts_P("init\r\n");
		uart_puts_P("Tries to initialize the SD Card and load the FAT16 filesystem.\r\n");
		uart_puts_P("Only needs to be called if it fails in the first place as it\r\n");
		uart_puts_P("is executed when booting. Will not reset any timers etc.\r\n");
	}
	else if (strings_equal(topic, "readtest"))
	{
		uart_puts_P("readtest [cluster] [filesize]\r\n");
		uart_puts_P("Just like cat. But instead of dumping the contents to the serial\r\n");
		uart_puts_P("port it will display the duration and the speed in kByte/s of the\r\n");
		uart_puts_P("read cycle. The larger the files, the more precise the measurement will be.\r\n");
	}
	else
	{
		uart_puts_P("Help topic not found. Call help for basic information.\r\n");
	}
}

void cmd_init(void)
{
	/* SD Karte Initilisieren */
	if (mmc_init())
	{
		uart_puts_P("Searching for SD/MMC  [Error]\r\n");
		uart_puts_P("mmc_init()            [Error]\r\n");
		uart_puts_P("Skipping! You MUST call init later on.\r\n");
	}
	else
	{
		uart_puts_P("Searching for SD/MMC  [OK]\r\n");
		uart_puts_P("mmc_init()            [OK]\r\n");

		fat_init(); // laden Cluster OFFSET und Size
		uart_puts_P("fat_init()            [OK]\r\n");
	}
}

/*
####################################################################
###				Main				####
####################################################################
*/

int main (void)
{
	sei();
	uart_init(UART_BAUD_SELECT(9600, F_CPU));
	uart_puts("\r\n\r\nTHIS PROGRAM USES 57600 BAUD - PLEASE CHANGE\r\n\r\n");

	_delay_ms(100);
	uart_init(UART_BAUD_SELECT(57600, F_CPU));
	cmd_clear();

	uart_puts_P("\n\n\n\n\r");
	uart_puts_P("--------------------------\r\n");
	uart_puts_P("System                [OK]\r\n");
	systimer_init();
	uart_puts_P("System Timer          [OK]\r\n");

	/* SD Karte Initilisieren */
	cmd_init();

	uart_puts_P("--------------------------\r\n\r\n");

	_delay_ms(100);
	char input[50]={0};
	static char parameters[10][32];
	while(1)
	{
		for (uint8_t i = 0; i<=10; *parameters[++i]=0x00);

		// Input
		uart_puts_P("> ");
		uart_input(input);

		// Process Input
		getparams(input, 10, 32, parameters);
		char *cmd = parameters[0];

		// Call Functions
		if (strings_equal(cmd, "ls"))
		{
			uint32_t cluster = 0;
			if (parameters[1][0] != 0x00)
				cluster = str2num(parameters[1], 16);
			cmd_ls(cluster);
		}
		else if (strings_equal(cmd, "cat"))
		{
			if (parameters[1][0] == 0x00 || parameters[2][0] == 0x00)
				cmd_help("cat");
			else
			{
				uint64_t cluster	= str2num(parameters[1], 16);
				uint64_t size		= str2num(parameters[2], 16);
				cmd_cat(cluster, size);
			}
		}
		else if (strings_equal(cmd, "readtest"))
		{
			if (parameters[1][0] == 0x00 || parameters[2][0] == 0x00)
				cmd_help("readtest");
			else
			{
				uint64_t cluster	= str2num(parameters[1], 16);
				uint64_t size		= str2num(parameters[2], 16);
				cmd_readtest(cluster, size);
			}
		}
		else if (strings_equal(cmd, "write"))
		{
			if (parameters[1][0] == 0x00 || parameters[2][0] == 0x00)
				cmd_help("write");
			else
			{
				uint64_t cluster	= str2num(parameters[1], 16);
				uint8_t  adcport	= str2num(parameters[2], 10);
				uint64_t maxsize	= str2num(parameters[3], 16);
				uint8_t  speed		= str2num(parameters[4], 10);
				if (parameters[3][0] == 0x00)
					maxsize = 512;
				if (parameters[4][0] == 0x00)
					speed = 50;
				cmd_write(cluster, adcport, maxsize, speed);
			}
		}
		else if (strings_equal(cmd, "clear"))
			cmd_clear();
		else if (strings_equal(cmd, "time"))
			cmd_time();
		else if (strings_equal(cmd, "init"))
			cmd_init();
		else if (strings_equal(cmd, "help"))
			cmd_help(parameters[1]);
		else
			uart_puts_P("Error: Command not found\n\r");
	}

	while (1);
	return (1);
}

