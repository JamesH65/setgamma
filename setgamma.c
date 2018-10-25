

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>

#include "mailbox.h"

#define FADER 0
#define COLOURY 1
#define POSTERISE 2


static int fd;

void set_gamma(int display_num, uint32_t buffer)
{
   int i=0;
   unsigned int p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request
   p[i++] = 0x00008012; // set gamma
   p[i++] = 8; // buffer size
   p[i++] = 8; // data size

   p[i++] = display_num;
   p[i++] = buffer;           // ptr to VC memory buffer. Doesn't work in 64bit....

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof(*p); // actual size

   set_mailbox_property( fd, p);
}

void basic_assign(int channel, uint8_t i, uint8_t val, uint8_t *buf  )
{
    uint8_t *red = buf;
    uint8_t *green = red + 256;
    uint8_t *blue = green + 256;

	switch (channel)
	{
	case 0 :
		red[i] = val;
		break;
	case 1 :
		green[i] = val;
		break;
	case 2 :
		blue[i] = val;
		break;
	case 3 :
		red[i] = blue[i] = green[i] = val;
		break;
	}
}



void inverted_assign(int channel, uint8_t i, uint8_t val, uint8_t *buf  )
{
    uint8_t *red = buf;
    uint8_t *green = red + 256;
    uint8_t *blue = green + 256;

	switch (channel)
	{
	case 0 :
		red[i] = i;
		blue[i] = green[i] = val;
		break;
	case 1 :
		green[i] = i;
		blue[i] = red[i] = val;
		break;
	case 2 :
		blue[i] = i;
		red[i] = green[i] = val;
		break;
	case 3 :
		red[i] = blue[i] = green[i] = val;
		break;
	}
}

void channel_fader(int channel, int i, int j, uint8_t *buf)
{
    uint8_t *red = buf;
    uint8_t *green = red + 256;
    uint8_t *blue = green + 256;

	j = 255 - j;
	uint8_t val = (uint8_t)( (i/255.0) * (j/255.0) * 255) ;

    inverted_assign(channel, i, val, buf);
}

void coloury_fader(int channel, int i, int j, uint8_t *buf )
{
	uint8_t val = 0;

	if (j > i)
		val = i;

    basic_assign(channel, i, val, buf);
}

void posterise(int channel, int i, int j, uint8_t *buf )
{
    if (j == 0)
    	j = 1;

    uint8_t val = (i / j) * j;

    inverted_assign(channel, i, val, buf);
}

void calculate(int type, int channel, int i, int j, uint8_t *buf)
{
	switch (type)
	{
	default:
	case FADER :
		channel_fader(channel, i, j, buf);
		break;

	case COLOURY:
		coloury_fader(channel, i, j, buf);
		break;

	case POSTERISE:
		posterise(channel, i, j, buf);
		break;
	}
}

int main(int argc, char *argv[])
{
	VIDEOCORE_MEMORY_H mem;
	int alloc_size, dummy;
	int max_loops = 5, opt, delay = 1000;
	int channel, type;

	while ((opt = getopt(argc, argv, "l:t:c:d:")) != -1)
	{
	   switch (opt)
	   {
	   case 'l':
		   max_loops = atoi(optarg);
		   break;
	   case 'd':
		   delay = atoi(optarg);
		   break;
	   case 'c' :
		   channel = atoi(optarg);
		   if (channel > 3 || channel < 0)
			   channel = 3; // all
		   break;
	   case 't':
		   type = atoi(optarg);
		   break;

	   default: /* '?' */
		   fprintf(stderr, "Usage: %s [-l loops]\n",
				   argv[0]);
		   exit(EXIT_FAILURE);
	   }
   }


	fd = mailbox_init();

	if (fd == 0)
	{
       fprintf(stderr, "setgamma: Failed to initialise mailbox\n");
	   exit(EXIT_FAILURE);
	}

		// Get some videocore memory for pixel buffer when transferring gamma table to GPU

	const int transfer_buffer_size = 768;
    VIDEOCORE_MEMORY_H  transfer_buffer = mailbox_videocore_alloc(fd, transfer_buffer_size);

    if (!transfer_buffer.user)
    {
       fprintf(stderr, "setgamma: Failed to allocate VC memory, might need to run as sudo?");
	   exit(EXIT_FAILURE);
    }

    uint8_t *red = (uint8_t*)transfer_buffer.user;
    uint8_t *green = red + 256;
    uint8_t *blue = green + 256;

    for (int i=0;i<256;i++)
    {
    	red[i] = green[i] = blue[i] = i;
    }

    for (int loops = 0; loops < max_loops; loops++)
    {
		for (int j=0;j<256;j++)
		{
			for (int i=0;i<256;i++)
			{
				calculate(type, channel, i, j, red);
			}

			set_gamma(1, transfer_buffer.buffer);

			usleep(delay);
		}

		for (int j=255;j>=0;j--)
		{
			for (int i=0;i<256;i++)
			{
				calculate(type, channel, i, j, red);
			}

			set_gamma(1, transfer_buffer.buffer);

			usleep(delay);
		}
    }

    for (int i=0;i<256;i++)
    {
    	red[i] = green[i] = blue[i] = i;
    }

    set_gamma(1, transfer_buffer.buffer);


    mailbox_videocore_free(fd, transfer_buffer);

    mailbox_deinit(fd);

    return 0;
}
