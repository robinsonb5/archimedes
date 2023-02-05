#include "keyboard.h"

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
	0x00,
	KEY_BACKSPACE,
	KEY_INS,

	/* 20 */
	KEY_HOME,
	KEY_PAGEUP,
	KEY_NUMLOCK,
	KEY_NKSLASH,
	KEY_NKASTERISK,
	0x00,
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
	0x00,
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
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	/* 70 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	/* 78 */
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00
};

