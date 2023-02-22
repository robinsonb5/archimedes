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
#include "mouse.h"
#include "user_io.h"
#include "osd.h"
#include "menu.h"
#include "font.h"
#include "timer.h"
#include "diskimg.h"
#include "user_io.h"
#include "settings.h"
#include "ide.h"

#undef DEBUG

#include "c64keys.c"
#include "archie_keys.c"


/* Customise statusword and configstring handling. */

extern unsigned int statusword;

/* Override this since the archie core uses the BUT_SW command to send status */

#define UIO_BUT_SW 0x01

void sendstatus()
{
//	configfile_data.status=statusword;
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


int MouseX;
int MouseY;
int MouseButtons;

void HandleMousePacket(int protocol)
{
	int nx;
	int w1,w2,w3,w4;
	w1=PS2MouseRead();
	w2=PS2MouseRead();
	w3=PS2MouseRead();
	if(protocol==4)
		w4=PS2MouseRead();
	MouseButtons=w1&0x7;
	if(w1 & (1<<5))
		w3|=0xffffff00;
	if(w1 & (1<<4))
		w2|=0xffffff00;

	nx=w2;
	MouseX+=nx;

	nx=-w3;
	MouseY+=nx;
}


int loadimage(const char *filename,int unit);


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

	romtype=3;
	loadimage("CMOS    RAM",3);
	loadimage("ARCHIE1 HDF",'2');

	if(!loadimage(bootrom_name,0))
		result="ROM loading failed.";

	statusword&=~2;
	sendstatus();

	initc64keys();

	/* Initialise the PS/2 mouse */
	EnableInterrupts();
	HandlePS2Mouse(1);

	arckb_enqueue_state_timeout(HRST,STATE_RAK1,KBTIMEOUT);

	return(result);
}


char *get_rtc();


__weak void mainloop()
{
	int framecounter;
	while(1)
	{
		int pausedisk;
		HandlePS2Mouse(0);
		pausedisk=handlekeyboard();
		Menu_Run();
		if(!pausedisk)
		{
			diskimg_poll();
			HandleHDD();
		}
		if((framecounter++&8191)==0)
			user_io_send_rtc(get_rtc());
	}
}

