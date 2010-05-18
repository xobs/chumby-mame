/****************************************************************************

Sega "Space Tactics" Driver

Frank Palazzolo (palazzol@home.com)

Master processor - Intel 8080A

Memory Map:

0000-2fff ROM                       R
4000-47ff RAM                       R/W
5000-5fff switches/status           R
6000-6fff dips sw                   R
6000-600f Coin rjct/palette select  W
6010-601f sound triggers            W
6020-602f lamp latch                W
6030-603f speed latch               W
6040-605f shot related              W
6060-606f score display             W
60a0-60e0 sound triggers2           W
7000-7fff RNG/swit                  R     LS Nibble are a VBlank counter
								          used as a RNG
8000-8fff swit/stat                 R
8000-8fff offset RAM                W
9000-9fff V pos reg.                R     Reads counter from an encoder wheel
a000-afff H pos reg.                R     Reads counter from an encoder wheel
b000-bfff VRAM B                    R/W   alphanumerics, bases, barrier,
                                          enemy bombs
d000-dfff VRAM D                    R/W   furthest aliens (scrolling)
e000-efff VRAM E                    R/W   middle aliens (scrolling)
f000-ffff VRAM F                    R/W   closest aliens (scrolling)

--------------------------------------------------------------------

At this time, emulation is missing:

Lamps (Credit, Barrier, and 5 lamps for firing from the bases)
Sound (all discrete and a 76447)
Verify Color PROM resistor values (Last 8 colors)
Verify Bar graph displays

****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* Defined in machine/stactics.c */
extern int stactics_port_0_r(int);
extern int stactics_port_2_r(int);
extern int stactics_port_3_r(int);
extern int stactics_vert_pos_r(int);
extern int stactics_horiz_pos_r(int);
extern int stactics_interrupt(void);

/* Defined in vidhrdw/stactics.c */
extern int stactics_vh_start(void);
extern void stactics_vh_stop(void);
extern void stactics_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
extern unsigned char *stactics_scroll_ram;
extern unsigned char *stactics_videoram_b;
extern unsigned char *stactics_chardata_b;
extern unsigned char *stactics_videoram_d;
extern unsigned char *stactics_chardata_d;
extern unsigned char *stactics_videoram_e;
extern unsigned char *stactics_chardata_e;
extern unsigned char *stactics_videoram_f;
extern unsigned char *stactics_chardata_f;

void stactics_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,
                                    const unsigned char *color_prom);

extern int stactics_videoram_b_r(int);
extern int stactics_chardata_b_r(int);
extern int stactics_videoram_d_r(int);
extern int stactics_chardata_d_r(int);
extern int stactics_videoram_e_r(int);
extern int stactics_chardata_e_r(int);
extern int stactics_videoram_f_r(int);
extern int stactics_chardata_f_r(int);

extern void stactics_palette_w(int, int);
extern void stactics_score_w(int, int);
extern void stactics_motor_w(int, int);
extern void stactics_scroll_ram_w(int, int);
extern void stactics_speed_latch_w(int, int);
extern void stactics_shot_trigger_w(int, int);
extern void stactics_shot_flag_clear_w(int, int);

extern void stactics_videoram_b_w(int, int);
extern void stactics_chardata_b_w(int, int);
extern void stactics_videoram_d_w(int, int);
extern void stactics_chardata_d_w(int, int);
extern void stactics_videoram_e_w(int, int);
extern void stactics_chardata_e_w(int, int);
extern void stactics_videoram_f_w(int, int);
extern void stactics_chardata_f_w(int, int);

static struct MemoryReadAddress readmem[] =
{
    { 0x0000, 0x2fff, MRA_ROM },
    { 0x4000, 0x47ff, MRA_RAM },
    { 0x5000, 0x5fff, input_port_0_r, },
    { 0x6000, 0x6fff, input_port_1_r, },
    { 0x7000, 0x7fff, stactics_port_2_r, },
    { 0x8000, 0x8fff, stactics_port_3_r },
    { 0x9000, 0x9fff, stactics_vert_pos_r },
    { 0xa000, 0xafff, stactics_horiz_pos_r },

    { 0xb000, 0xb3ff, stactics_videoram_b_r, &stactics_videoram_b },
    { 0xb400, 0xb7ff, MRA_RAM },   /* Unused */
    { 0xb800, 0xbfff, stactics_chardata_b_r, &stactics_chardata_b },

    { 0xd000, 0xd3ff, stactics_videoram_d_r, &stactics_videoram_d },
    { 0xd400, 0xd7ff, MRA_RAM },   /* Unused */
    { 0xd800, 0xdfff, stactics_chardata_d_r, &stactics_chardata_d },

    { 0xe000, 0xe3ff, stactics_videoram_e_r, &stactics_videoram_e },
    { 0xe400, 0xe7ff, MRA_RAM },   /* Unused */
    { 0xe800, 0xefff, stactics_chardata_e_r, &stactics_chardata_e },

    { 0xf000, 0xf3ff, stactics_videoram_f_r, &stactics_videoram_f },
    { 0xf400, 0xf7ff, MRA_RAM },   /* Unused */
    { 0xf800, 0xffff, stactics_chardata_f_r, &stactics_chardata_f },

	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
    { 0x0000, 0x2fff, MWA_ROM },
    { 0x4000, 0x47ff, MWA_RAM },
    /* { 0x6000, 0x6001, stactics_coin_reject_w }, */
    { 0x6006, 0x6007, stactics_palette_w },
    /* { 0x6010, 0c601f, stactics_sound_w }, */
    { 0x6016, 0x6016, stactics_motor_w },  /* Note: This overlaps rocket sound */
    /* { 0x6020, 0x602f, stactics_lamp_latch_w }, */
    { 0x6030, 0x603f, stactics_speed_latch_w },
    { 0x6040, 0x604f, stactics_shot_trigger_w },
    { 0x6050, 0x605f, stactics_shot_flag_clear_w },
    { 0x6060, 0x606f, stactics_score_w },
    /* { 0x60a0, 0x60ef, stactics_sound2_w }, */

    { 0x8000, 0x8fff, stactics_scroll_ram_w, &stactics_scroll_ram },

    { 0xb000, 0xb3ff, stactics_videoram_b_w, &stactics_videoram_b },
    { 0xb400, 0xb7ff, MWA_RAM },   /* Unused */
    { 0xb800, 0xbfff, stactics_chardata_b_w, &stactics_chardata_b },

    { 0xc000, 0xcfff, MWA_NOP }, /* according to the schematics, nothing is mapped here */
                                 /* but, the game still tries to clear this out         */

    { 0xd000, 0xd3ff, stactics_videoram_d_w, &stactics_videoram_d },
    { 0xd400, 0xd7ff, MWA_RAM },   /* Unused */
    { 0xd800, 0xdfff, stactics_chardata_d_w, &stactics_chardata_d },

    { 0xe000, 0xe3ff, stactics_videoram_e_w, &stactics_videoram_e },
    { 0xe400, 0xe7ff, MWA_RAM },   /* Unused */
    { 0xe800, 0xefff, stactics_chardata_e_w, &stactics_chardata_e },

    { 0xf000, 0xf3ff, stactics_videoram_f_w, &stactics_videoram_f },
    { 0xf400, 0xf7ff, MWA_RAM },   /* Unused */
    { 0xf800, 0xffff, stactics_chardata_f_w, &stactics_chardata_f },

	{ -1 }	/* end of table */
};

INPUT_PORTS_START( stactics_input_ports )

    PORT_START  /* IN0 */
    /*PORT_BIT (0x80, IP_ACTIVE_HIGH, IPT_UNUSED ) see stactics_port_0_r */
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 )
    PORT_BIT (0x08, IP_ACTIVE_LOW, IPT_BUTTON4 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON5 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON6 )
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON7 )

    PORT_START  /* IN1 */
    PORT_DIPNAME( 0x07, 0x07, "Coin Mech. 2", IP_KEY_NONE )
    PORT_DIPSETTING(    0x07, "1 Coin/1 Play" )
    PORT_DIPSETTING(    0x05, "2 Coins/1 Play" )
    PORT_DIPSETTING(    0x01, "4 Coins/1 Play" )
    PORT_DIPSETTING(    0x06, "1 Coin/2 Plays" )
    PORT_DIPSETTING(    0x04, "1 Coin/3 Plays" )
    PORT_DIPSETTING(    0x02, "1 Coin/4 Plays" )
    PORT_DIPSETTING(    0x03, "1 Coin/6 Plays" )
    PORT_DIPNAME( 0x38, 0x38, "Coin Mech. 1", IP_KEY_NONE )
    PORT_DIPSETTING(    0x38, "1 Coin/1 Play" )
    PORT_DIPSETTING(    0x28, "2 Coins/1 Play" )
    PORT_DIPSETTING(    0x08, "4 Coins/1 Play" )
    PORT_DIPSETTING(    0x30, "1 Coin/2 Plays" )
    PORT_DIPSETTING(    0x20, "1 Coin/3 Plays" )
    PORT_DIPSETTING(    0x10, "1 Coin/4 Plays" )
    PORT_DIPSETTING(    0x18, "1 Coin/6 Plays" )
    PORT_DIPNAME( 0x40, 0x40, "High Score Initial Entry", IP_KEY_NONE )
    PORT_DIPSETTING(    0x40, "off" )
    PORT_DIPSETTING(    0x00, "on" )
    PORT_DIPNAME( 0x80, 0x80, "Advertising Sound", IP_KEY_NONE )
    PORT_DIPSETTING(    0x80, "off" )
    PORT_DIPSETTING(    0x00, "on" )

    PORT_START  /* IN2 */
    /* This is accessed by stactics_port_2_r() */
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
    PORT_DIPNAME( 0x40, 0x40, "Free Play", IP_KEY_NONE )
    PORT_DIPSETTING( 0x00, "on" )
    PORT_DIPSETTING( 0x40, "off" )
    PORT_BITX( 0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Test Switch", OSD_KEY_F2, IP_JOY_NONE, 0 )
    PORT_DIPSETTING( 0x00, "on" )
    PORT_DIPSETTING( 0x80, "off" )

    PORT_START  /* IN3 */
    /* This is accessed by stactics_port_3_r() */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
    /* PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED ) */
    PORT_DIPNAME( 0x04, 0x04, "Number of Barriers", IP_KEY_NONE )
    PORT_DIPSETTING(    0x04, "4" )
    PORT_DIPSETTING(    0x00, "6" )
    PORT_DIPNAME( 0x08, 0x08, "Bonus Barriers", IP_KEY_NONE )
    PORT_DIPSETTING(    0x08, "1" )
    PORT_DIPSETTING(    0x00, "2" )
    PORT_DIPNAME( 0x10, 0x00, "Extended Play", IP_KEY_NONE )
    PORT_DIPSETTING(    0x00, "on" )
    PORT_DIPSETTING(    0x10, "off" )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
    /* PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) */

	PORT_START	/* FAKE */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
INPUT_PORTS_END

/* For the character graphics */

static struct GfxLayout gfxlayout =
{
    8,8,
    256,
    1,     /* 1 bit per pixel */
    { 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7 },
    { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
    8*8
};

/* For the LED Fire Beam (made up) */

static struct GfxLayout firelayout =
{
    16,9,
    256,
    1,     /* 1 bit per pixel */
    { 0 },
    { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 },
    { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8 },
    8*9
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
    { 0, 0, &gfxlayout,  0,      64*4 },    /* Dynamically decoded from RAM */
    { 0, 0, &gfxlayout,  1*2*16, 64*4 },    /* Dynamically decoded from RAM */
    { 0, 0, &gfxlayout,  2*2*16, 64*4 },    /* Dynamically decoded from RAM */
    { 0, 0, &gfxlayout,  3*2*16, 64*4 },    /* Dynamically decoded from RAM */
    { 0, 0, &firelayout, 0, 64*4 },         /* LED Fire beam (synthesized gfx) */
    { 0, 0, &gfxlayout,  0, 64*4 },         /* LED and Misc. Display characters */
    { -1 }
};

static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
            CPU_8080,
            1933560,
			0,
			readmem,writemem,0,0,
            stactics_interrupt,1
		},
	},
    60, DEFAULT_60HZ_VBLANK_DURATION,
    1,
	0,

	/* video hardware */
    32*8, 32*8, { 0*8, 32*8-1, 0*8, 32*8-1 },
    gfxdecodeinfo,
    16, 16*4*4*2,
    stactics_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,

    stactics_vh_start,
    stactics_vh_stop,
    stactics_vh_screenrefresh,

	/* sound hardware */
    0,
    0,  /* Start audio  */
    0,  /* Stop audio   */
    0   /* Update audio */
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( stactics_rom )
    ROM_REGION(0x10000) /* 64k for code */
    ROM_LOAD( "epr-218x",     0x0000, 0x0800, 0xb1186ad2 )
    ROM_LOAD( "epr-219x",     0x0800, 0x0800, 0x3b86036d )
    ROM_LOAD( "epr-220x",     0x1000, 0x0800, 0xc58702da )
    ROM_LOAD( "epr-221x",     0x1800, 0x0800, 0xe327639e )
    ROM_LOAD( "epr-222y",     0x2000, 0x0800, 0x24dd2bcc )
    ROM_LOAD( "epr-223x",     0x2800, 0x0800, 0x7fef0940 )

    ROM_REGION_DISPOSE(0x1060)  /* temporary space for graphics (disposed after conversion) */
    ROM_LOAD( "epr-217",      0x0000, 0x0800, 0x38259f5f )      /* LED fire beam data      */
    ROM_LOAD( "pr55",         0x0800, 0x0800, 0xf162673b )      /* timing PROM (unused)    */
    ROM_LOAD( "pr65",         0x1000, 0x0020, 0xa1506b9d )      /* timing PROM (unused)    */
    ROM_LOAD( "pr66",         0x1020, 0x0020, 0x78dcf300 )      /* timing PROM (unused)    */
    ROM_LOAD( "pr67",         0x1040, 0x0020, 0xb27874e7 )      /* LED timing ROM (unused) */

    ROM_REGION(0x0800)  /* color prom */
    ROM_LOAD( "pr54",         0x0000, 0x0800, 0x9640bd6e )         /* color/priority prom */


ROM_END

struct GameDriver stactics_driver =
{
    __FILE__,
    0,

    "stactics",
    "Space Tactics",
    "1981",
    "Sega",
    "Frank Palazzolo",
    0,
	&machine_driver,
	0,

    stactics_rom,
	0, 0,

	0,

	0,	/* sound_prom */

    stactics_input_ports,

    PROM_MEMORY_REGION(2), 0, 0,
    ORIENTATION_DEFAULT,

	0, 0
};
