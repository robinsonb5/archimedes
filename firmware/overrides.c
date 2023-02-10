#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

#include "configstring.h"
#include "keyboard.h"
#include "uart.h"
#include "spi.h"
#include "minfat.h"
#include "interrupts.h"
#include "ps2.h"
#include "user_io.h"
#include "osd.h"
#include "menu.h"
#include "font.h"
#include "cue_parser.h"
#include "pcecd.h"
#include "timer.h"
#include "diskimg.h"
#include "spi_sd.h"
#include "user_io.h"
#include "settings.h"

#undef DEBUG

#include "c64keys.c"
#include "archie_keys.c"
#include "ide.c"

struct mistery_config
{
	char version;
	char scandouble;
	char pad[2];
	int status;
	uint32_t romdir;
	uint32_t hdddir[2];
	char romname[12]; /* Must be null-terminated */
	char hddname[2][12]; /* Must be null-terminated */
};

struct mistery_config configfile_data;


/* Customise statusword and configstring handling. */

extern unsigned int statusword;

/* Override this since the archie core uses the BUT_SW command to send status */

#define UIO_BUT_SW 0x01

void sendstatus()
{
	configfile_data.status=statusword;
	spi_uio_cmd_cont(UIO_BUT_SW);
	SPI(statusword);
	DisableIO();
}

char *configstring="Arc;;"
	"S0U,ADF,Floppy 1:;"
	"S1U,ADF,Floppy 2:;"
	"S2U,HDF,Hard disk 1:;"
	"S3U,HDF,Hard disk 2:;"
	"SRU,RAM,Load CMOS RAM:;"
	"SSU,RAM,Save CMOS RAM:;"
	"T1,Reset;"
	"V,v1.0.";
static char *cfgptr;

int configstring_next()
{
	char result=0;
	if(cfgptr)
		result=*cfgptr++;
	if(!result)
		cfgptr=0;
	return(result);
}

void configstring_begin()
{
	cfgptr=configstring;
}


#define DIRECTUPLOAD 0x10

/* Initial ROM */
int LoadROM(const char *fn);
extern unsigned char romtype;
extern fileTYPE file;


void buildmenu(int offset);

unsigned char initmouse[]=
{	
	0x1,0xff, // Send 1 byte reset sequence
	0x82,	// Wait for two bytes in return (in addition to the normal acknowledge byte)
//	1,0xf4,0, // Uncomment this line to leave the mouse in 3-byte mode
	8,0xf3,200,0xf3,100,0xf3,80,0xf2,1, // Send PS/2 wheel mode knock sequence...
	0x81,	// Receive device ID (should be 3 for wheel mice)
	1,0xf4,0	// Enable reporting.
};

int MouseX;
int MouseY;
int MouseButtons;

void handlemouse(int reset)
{
	int byte;
//	static int delay=0;
	static int timeout;
	static int init=0;
	static int idx=0;
	static int txcount=0;
	static int rxcount=0;
	if(reset)
		idx=0;

	if(!idx)
	{
		while(PS2MouseRead()>-1)
			; // Drain the buffer;
		txcount=initmouse[idx++];
		rxcount=0;
	}
	else
	{
		if(rxcount)
		{
			int q=PS2MouseRead();
			if(q>-1)
			{
//				printf("Received %x\n",q);
				--rxcount;
			}
			else if(CheckTimer(timeout))
			{
				/* Clear the mouse buffer on timeout, to avoid blocking if no mouse if connected */
				ps2_ringbuffer_init(&mousebuffer);
				idx=0;
			}
	
			if(!txcount && !rxcount)
			{
				int next=initmouse[idx++];
				if(next&0x80)
				{
					rxcount=next&0x7f;
//					printf("Receiving %x bytes",rxcount);
				}
				else
				{
					txcount=next;
//					printf("Sending %x bytes",txcount);
				}
			}
		}
		else if(txcount)
		{
			PS2MouseWrite(initmouse[idx++]);
			--txcount;
			rxcount=1;
			timeout=GetTimer(3500);	//3.5 seconds
		}
		else
		{
			while(PS2MouseBytesReady()>=4) // FIXME - institute some kind of timeout here to re-sync if sync lost.
			{
				int nx;
				int w1,w2,w3,w4;
				w1=PS2MouseRead();
				w2=PS2MouseRead();
				w3=PS2MouseRead();
				w4=PS2MouseRead();
				MouseButtons=w1&0x7;
				if(w1 & (1<<5))
					w3|=0xffffff00;
				if(w1 & (1<<4))
					w2|=0xffffff00;

				nx=MouseX+w2;
				MouseX+=nx;

				nx=MouseY-w3;
				MouseY+=nx;
			}
		}
	}
}


int loadimage(const char *filename,int unit);

int loadconfig(const char *filename)
{
	int result=0;
	char *err=0;
	uint32_t currentdir=CurrentDirectory();
	if(!filename)
		return(result);

	romtype=1;

	if(FileOpen(&file,filename))
	{
		int hdddir[2]; /* Cache these to avoid multiple reloads of the conig */
		struct mistery_config *dat=(struct mistery_config *)sector_buffer;
		statusword|=1;
		sendstatus(); /* Put the core in reset */
		FileReadSector(&file,sector_buffer);

		hdddir[0]=dat->hdddir[0];
		hdddir[1]=dat->hdddir[1];

		/* Load the config file to sector_buffer */

		if(dat->version==1)
		{
//			printf("config version OK\n");
//			statusword=dat->status|1; /* Core will be in reset when status is next written */
//			statusword&=~(TOS_ACSI0_ENABLE|TOS_ACSI1_ENABLE); /* Disable hard drives, will be re-enabled if HDF opens successfully. */
			scandouble=dat->scandouble;

			if(ValidateDirectory(dat->romdir))
			{
				FileReadSector(&file,sector_buffer);
				ChangeDirectoryByCluster(dat->romdir);
				result=loadimage(dat->romname,0);
			}
#ifdef DEBUG
			else
				printf("ROM directory failed validation\n");
#endif

			/* Loading the ROM file will have overwritten the config, so reload it */
			ChangeDirectoryByCluster(currentdir);
			FileOpen(&file,filename);

			if(ValidateDirectory(hdddir[0]))
			{
				FileReadSector(&file,sector_buffer);
				ChangeDirectoryByCluster(dat->hdddir[0]);
				loadimage(dat->hddname[0],'2');
			}
#ifdef DEBUG
			else
				printf("HDDDir[0] bad\n");
#endif

			/* Reload the config again */

			ChangeDirectoryByCluster(currentdir);
			FileOpen(&file,filename);

			if(ValidateDirectory(hdddir[1]))
			{
				FileReadSector(&file,sector_buffer);
				ChangeDirectoryByCluster(dat->hdddir[1]);
				loadimage(dat->hddname[1],'3');
			}
#ifdef DEBUG
			else
				printf("HDDDir[1] bad\n");
#endif
		}
	}
	statusword|=2; /* Re-assert reset */
	sendstatus();
	statusword&=~2; /* Release reset */
	sendstatus();
	return(result);
}


int saveconfig(const char *filename)
{
	putchar('\n');
	if(FileOpen(&file,filename))
	{
		configfile_data.version=1;
		configfile_data.scandouble=scandouble;
		configfile_data.status=statusword;
		/* Ensure null-termination of filenames */
		configfile_data.romname[11]=0;
		configfile_data.hddname[0][11]=0;
		configfile_data.hddname[1][11]=0;
		FileWriteSector(&file,(char *)&configfile_data);
		return(1);
	}
	return(0);
}


#define SPIFPGA(a,b) SPI_ENABLE(HW_SPI_FPGA); *spiptr=(a); *spiptr=(b); SPI_DISABLE(HW_SPI_FPGA);

void saveram(const char *filename)
{
	register volatile int *spiptr=&HW_SPI(HW_SPI_DATA);
	int i;
	char *ptr=sector_buffer;
	/* Fetch CMOS RAM from data_io */
	SPIFPGA(SPI_FPGA_FILE_INDEX,0xff);
	*spiptr=0xff;
	SPIFPGA(SPI_FPGA_FILE_RX,0xff);
	SPI_ENABLE_FAST_INT(HW_SPI_FPGA);
	*spiptr=SPI_FPGA_FILE_RX_DAT;

	i=256;
	*spiptr=0xff;
	while(i--)
	{
		*spiptr=0xff;
		*ptr++=*spiptr;
	}
	SPI_DISABLE(HW_SPI_FPGA);

	SPIFPGA(SPI_FPGA_FILE_RX,0);
}


int loadimage(const char *filename,int unit)
{
	int result=0;
	int u=unit-'0';

	switch(unit)
	{
		/* ROM images */
		case 0: /* RISCOS ROM */
		case 3: /* CMOS RAM */
			if(filename && filename[0])
			{
				statusword|=2;
				sendstatus();
				result=LoadROM(filename);
			}
			break;
		/* Floppy images */
		case '0':
		case '1':
			diskimg_mount(0,u);
			diskimg_mount(filename,u);
			result=diskimg[u].file.size;
			break;
		/* Hard disk images */
		case '2':
		case '3':
			hdf[u-2].file.size=0;
			OpenHardfile(filename,u-2);
			break;
		/* Configuration files */
		case 'R': /* Load CMOS RAM */
			romtype=3;
			result=loadimage(filename,3);
			break;
		case 'S': /* Save config */
			romtype=3;
			if(FileOpen(&file,filename))
			{
				saveram(filename);
				FileWriteSector(&file,(char *)&sector_buffer);
			}
			break;
	}
	statusword&=~2;
	sendstatus();
	return(result);
}


const char *bootrom_name="RISCOS  ROM";
const char *bootcfg_name="ARCHIE  CFG";

char *autoboot()
{
	char *result=0;
	int s;
	int t;
	statusword=2;

	arckb.out_hw=0;
	arckb.out_cpu=0;
	arckb.ena=0;
	arckb.state=STATE_IDLE;

	romtype=1;
	configstring_index=0;

	sendstatus();
	if(!loadimage(bootrom_name,0))
		result="ROM loading failed.";

	statusword&=~2;
	sendstatus();

	initc64keys();

	/* Initialise the PS/2 mouse */
	EnableInterrupts();
	handlemouse(1);

	arckb_enqueue_state_timeout(HRST,STATE_RAK1,KBTIMEOUT);

	return(result);
}


char *get_rtc();


__weak void mainloop()
{
	int framecounter;
	while(1)
	{
		handlemouse(0);
		handlekeyboard();
		Menu_Run();
		diskimg_poll();
		HandleHDD();
//		mist_get_dmastate();
		if((framecounter++&8191)==0)
			user_io_send_rtc(get_rtc());
	}
}

