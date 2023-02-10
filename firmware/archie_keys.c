#include "keyboard.h"

#include "archie.h"

/* Archimedes to PS/2 translation table.
   (This way round to halve the table size, at the expense of
   lookup speed.)  */

const unsigned char archie_keycode[] = {
	/* 00 */
	KEY_ESC,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,

	/* 08 */
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_F11,
	KEY_F12,
	KEY_PRTSCRN,
	KEY_SCROLLLOCK,
	KEY_BREAK,  /* awkward PS/2 scancode */

	/* 10 */
	KEY_TICK,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,

	/* 18 */
	KEY_8,
	KEY_9,
	KEY_0,
	KEY_MINUS,
	KEY_EQUALS,
	0xff, /* Not defined */
	KEY_BACKSPACE,
	KEY_INS,

	/* 20 */
	KEY_HOME,
	KEY_PAGEUP,
	KEY_NUMLOCK,
	KEY_NKSLASH,
	KEY_NKASTERISK,
	0xff, /* Not defined */
	KEY_TAB,
	KEY_Q,

	/* 28 */
	KEY_W,
	KEY_E,
	KEY_R,
	KEY_T,
	KEY_Y,
	KEY_U,
	KEY_I,
	KEY_O,

	/* 30 */
	KEY_P,
	KEY_LEFTBRACE,
	KEY_RIGHTBRACE,
	KEY_HASH,
	KEY_DELETE,
	KEY_END,
	KEY_PAGEDOWN,
	KEY_NK7,

	/* 38 */
	KEY_NK8,
	KEY_NK9,
	KEY_NKMINUS,
	KEY_LCTRL,
	KEY_A,
	KEY_S,
	KEY_D,
	KEY_F,

	/* 40 */
	KEY_G,
	KEY_H,
	KEY_J,
	KEY_K,
	KEY_L,
	KEY_SEMICOLON,
	KEY_APOSTROPHE,
	KEY_ENTER,

	/* 48 */
	KEY_NK4,
	KEY_NK5,
	KEY_NK6,
	KEY_NKPLUS,
	KEY_LSHIFT,
	0xff, /* Not defined */
	KEY_Z,
	KEY_X,

	/* 50 */
	KEY_C,
	KEY_V,
	KEY_B,
	KEY_N,
	KEY_M,
	KEY_COMMA,
	KEY_PERIOD,
	KEY_SLASH,

	/* 58 */
	KEY_RSHIFT,
	KEY_UPARROW,
	KEY_NK1,
	KEY_NK2,
	KEY_NK3,
	KEY_CAPSLOCK,
	KEY_ALT,
	KEY_SPACE,

	/* 60 */
	KEY_ALTGR,
	KEY_RCTRL,
	KEY_LEFTARROW,
	KEY_DOWNARROW,
	KEY_RIGHTARROW,
	KEY_NK0,
	KEY_NKPERIOD,
	KEY_NKENTER,

	/* 68 */
	0x00, /* Null terminate */
};

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
extern int MouseX;
extern int MouseY;
extern int MouseButtons;

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
	int arccode=0;
	int t;
	if(!arckb.ena & ARC_ENA_KEYBOARD)
		return;
	if(ext)
		key|=0x80;
	t=TestKey(key);
	if(!keyup && t)
		return;
	while(1) {
		t=archie_keycode[arccode];
		if(!t)
			return;		
		if(t==key)
		{
			t=keyup ? KUDA : KDDA;
//			putchar(keyup ? 'u' : 'd');
//			putchar(HEXDIGIT(arccode>>4));
//			putchar(HEXDIGIT(arccode&15));
			arckb_enqueue_state_timeout(t|(arccode>>4),STATE_WAIT4ACK1,KBTIMEOUT);
			arckb_enqueue_state_timeout(t|(arccode&15),STATE_WAIT4ACK1,KBTIMEOUT);
			return;
		}
		++arccode;
	}
}


int UpdateKeys(int blockkeys)
{
	handlec64keys();
	return(HandlePS2RawCodes(blockkeys));
}


