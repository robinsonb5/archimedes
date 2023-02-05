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

#include "archie.h"
#include "c64keys.c"
#include "acsi.c"
#include "keytable.c"

#define UIO_BUT_SW 0x01


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


extern unsigned int statusword; /* So we can toggle the write-protect bits and provoke diskchanges. */

/* Override this since the status is sent to dataio, not userio */
void sendstatus()
{
	configfile_data.status=statusword;
	spi_uio_cmd_cont(UIO_BUT_SW);
	SPI(statusword);
	DisableIO();
}

char *configstring="Arc;;"
	"S0U,ADF,Floppy A:;"
	"P1,Storage;"
	"P1S0U,ADF,Floppy A:;"
	"P1S1U,ADF,Floppy B:;"
	"P1OAB,Hard disks,None,Unit 0,Unit 1,Both;"
	"P1S2U,HDFVHD,Hardfile 0;"
	"P1S3U,HDFVHD,Hardfile 1;"
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


/* Key -> gamepad mapping.  We override this to swap buttons A and B for NES. */

unsigned char joy_keymap[]=
{
	KEY_CAPSLOCK,
	KEY_LSHIFT,
	KEY_LCTRL,
	KEY_ALT,
	KEY_W,
	KEY_S,
	KEY_A,
	KEY_D,
	KEY_ENTER,
	KEY_RSHIFT,
	KEY_RCTRL,
	KEY_ALTGR,
	KEY_UPARROW,
	KEY_DOWNARROW,
	KEY_LEFTARROW,
	KEY_RIGHTARROW,
};

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

//	if(!CheckTimer(delay))
//		return;
//	delay=GetTimer(20);

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

#define STATUS_WP_UNIT0 6
#define STATUS_WP_UNIT1 7

void toggle_wp(int unit)
{
	unsigned int s=statusword;
	if(unit)
		s^=1<<STATUS_WP_UNIT1;
	else
		s^=1<<STATUS_WP_UNIT0;

	sendstatus();

	s=GetTimer(500);
	while(!CheckTimer(s))
		;
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
			statusword=dat->status|1; /* Core will be in reset when status is next written */
			statusword&=~(TOS_ACSI0_ENABLE|TOS_ACSI1_ENABLE); /* Disable hard drives, will be re-enabled if HDF opens successfully. */
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


int loadimage(const char *filename,int unit)
{
	int result=0;
	int u=unit-'0';

	switch(unit)
	{
		/* ROM images */
		case 0:
			if(filename && filename[0])
			{
				statusword|=2;
				sendstatus();
				strncpy(configfile_data.romname,filename,11);
				configfile_data.romname[11]=0;
				configfile_data.romdir=CurrentDirectory();
				result=LoadROM(configfile_data.romname);
			}
			break;
		/* Floppy images */
		case '0':
		case '1':
			diskimg_mount(0,u);
			toggle_wp(u);
			diskimg_mount(filename,u);
			result=diskimg[u].file.size;
			break;
		/* Hard disk images */
		case '2':
		case '3':
			diskimg[u].file.size=0;
			if(filename)// && filename[0])
			{
				strncpy(configfile_data.hddname[u-2],filename,11);
				configfile_data.hddname[u-2][11]=0;
				FileOpen(&diskimg[u].file,configfile_data.hddname[u-2]);
			}
			else
				configfile_data.hddname[u-2][0]=0;
			configfile_data.hdddir[u-2]=CurrentDirectory();

			if(diskimg[u].file.size)
				statusword|=(TOS_ACSI0_ENABLE<<(u-2));
			else
				statusword&=~(TOS_ACSI0_ENABLE<<(u-2));
			result=diskimg[u].file.size;
			break;
		/* Configuration files */
		case 'C': /* Load config */
			result=loadconfig(filename);
			break;
		case 'D': /* Save config */
			result=saveconfig(filename);
			break;
	}
	statusword&=~2;
	sendstatus();
	return(result);
}

#define KBTIMEOUT 200
#define ARCKB_RINGBUFFER_SIZE 16

struct arckeyboard
{
	volatile int out_hw;
	volatile int out_cpu;
	unsigned char outbuf[ARCKB_RINGBUFFER_SIZE];
	int ena;
	int timeout;
	enum arckdbstate state;
	enum arckbdstate nextstate;
};

struct arckeyboard arckb;



void arckb_enqueue_state_timeout(char c,enum arckbdstate state,int timeout)
{
	arckb.outbuf[arckb.out_cpu++]=c;
	arckb.out_cpu&=ARCKB_RINGBUFFER_SIZE-1;
	arckb.state=STATE_SEND;
	arckb.nextstate=state;
	arckb.timeout=GetTimer(timeout);
}


#define HEXDIGIT(x) ('0'+(x) + ((x)>9 ? 'A'-'9'-1 : 0))

void sendmousebutton(int button,int edge,int code)
{
	if(button&1)
	{
		int t=edge&1 ? KDDA : KUDA;
		arckb_enqueue_state_timeout(t|0x07,STATE_WAIT4ACK1,KBTIMEOUT);
		arckb_enqueue_state_timeout(t|code,STATE_WAIT4ACK1,KBTIMEOUT);
	}
}

void handlekeyboard()
{
	int to=CheckTimer(arckb.timeout);
	int status,data;

	spi_uio_cmd_cont(0x04);
	status=SPI(0xff);
	data=SPI(0xff);
	DisableIO();

	if(status == 0xa1) {

		switch(data)
		{
			case HRST:
//				putchar('R');
				arckb_enqueue_state_timeout(HRST,STATE_RAK1,KBTIMEOUT);
				break;

			case RAK1:
//				putchar('1');
				if(arckb.state == STATE_RAK1)
					arckb_enqueue_state_timeout(RAK1,STATE_RAK2,KBTIMEOUT);
				else
					arckb.state = STATE_HRST;
				break;

			case RAK2:
//				putchar('2');
				if(arckb.state == STATE_RAK2)
					arckb_enqueue_state_timeout(RAK2,STATE_IDLE,KBTIMEOUT);
				else
					arckb.state = STATE_HRST;
				break;

			// arm request keyboard id
			case RQID:
//				putchar('I');
				arckb_enqueue_state_timeout(KBID | 1,STATE_IDLE,0);
				break;

			// arm acks first byte
			case BACK:
//				putchar('A');
//				if(arckb.state != STATE_WAIT4ACK1)
//					arckb.state = STATE_HRST;
//				else
					arckb.state = STATE_IDLE;
				break;

			case NACK:
			case SACK:
			case MACK:
			case SMAK:
				arckb.ena=data&3;
				arckb.state = STATE_IDLE;
//				putchar('M'+(data&3));
				break;
		}
	}
	

	switch(arckb.state)
	{
		case STATE_WAIT4ACK1:
		case STATE_WAIT4ACK2:
//			putchar('w');
			if(to)
			{
//				putchar('T');
				arckb.state=STATE_IDLE;
			}
			break;

		case STATE_RAK2:
//			putchar('W');
			if(to)
			{
//				putchar('Z');
				arckb_enqueue_state_timeout(HRST,STATE_RAK1,KBTIMEOUT);
			}
			break;
		case STATE_IDLE:
			if((arckb.ena & ARC_ENA_MOUSE) && (MouseX || MouseY))
			{
				int t=MouseX;
//				putchar('m');
				if(t<-64)
					t=-64;
				if(t>63)
					t=63;
				MouseX-=t;
				arckb_enqueue_state_timeout(t&0x7f,STATE_WAIT4ACK1,KBTIMEOUT);
				t=-MouseY;
				if(t<-64)
					t=-64;
				if(t>63)
					t=63;
				MouseY+=t;
				arckb_enqueue_state_timeout(t&0x7f,STATE_WAIT4ACK2,KBTIMEOUT);				
			}
			if(arckb.ena & ARC_ENA_KEYBOARD)
			{
				static int prev=0;
				int e=MouseButtons;
				int b=prev ^ MouseButtons;
				sendmousebutton(b,e,0);
				b>>=1;
				e>>=1;
				sendmousebutton(b,e,2);
				b>>=1;
				e>>=1;
				sendmousebutton(b,e,1);
				prev=MouseButtons;
			}
			/* Fall through */
		case STATE_SEND:
			if(arckb.out_cpu!=arckb.out_hw)
			{
				int t=arckb.outbuf[arckb.out_hw++];
				arckb.out_hw&=ARCKB_RINGBUFFER_SIZE-1;
				spi_uio_cmd_cont(0x05);
				SPI(t);
				DisableIO();
				arckb.state=arckb.nextstate;
//				putchar(HEXDIGIT(t>>4));
//				putchar(HEXDIGIT(t&15));
			}
		default:
			break;
	}
}


void SendKey(int key,int ext,int keyup)
{
	int arccode;
	int t;
	if(!arckb.ena & ARC_ENA_KEYBOARD)
		return;
	if(ext)
		key|=0x80;
	t=TestKey(key);
	if(!keyup && t)
		return;
	for(arccode=0;arccode<128;++arccode)
	{
		if(archie_keycode[arccode]==key)
		{
			int t=keyup ? KUDA : KDDA;
			putchar(keyup ? 'u' : 'd');
			putchar(HEXDIGIT(arccode>>4));
			putchar(HEXDIGIT(arccode&15));
			arckb_enqueue_state_timeout(t|(arccode>>4),STATE_WAIT4ACK1,KBTIMEOUT);
			arckb_enqueue_state_timeout(t|(arccode&15),STATE_WAIT4ACK1,KBTIMEOUT);
			return;
		}
	}
}


int UpdateKeys(int blockkeys)
{
	handlec64keys();
	return(HandlePS2RawCodes(blockkeys));
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

	romtype=0;
	configstring_index=0;

	if(!loadconfig(bootcfg_name))
	{
		sendstatus();
		if(!loadimage(bootrom_name,0))
			result="ROM loading failed.";
	}
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
		mist_get_dmastate();
		if((framecounter++&8191)==0)
			user_io_send_rtc(get_rtc());
	}
}

