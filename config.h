/* See LICENSE file for copyright and license details. */

/*
 * appearance
 *
 * font: see http://freedesktop.org/software/fontconfig/fontconfig-user.html
 */
static const char *font =
    "IosevkaTermNerdFontMono:antialias=true:autohint=true";
static const int fontsize = 14;
static const int borderpx = 2;

/*
 * What program is execed by st depends of these precedence rules:
 * 1: program passed with -e
 * 2: scroll and/or utmp
 * 3: SHELL environment variable
 * 4: value of shell in /etc/passwd
 * 5: value of shell in config.h
 */
static const char *shell = "/bin/sh";
const char *utmp         = NULL;
/* scroll program: to enable use a string like "scroll" */
const char *scroll    = NULL;
const char *stty_args = "stty raw pass8 nl -echo -iexten -cstopb 38400";

/* identification sequence returned in DA and DECID */
const char *vtiden = "\033[?6c";

#if 0 /* TODO */
/* Kerning / character bounding-box multipliers */
static const float cwscale = 1.0;
static const float chscale = 1.0;
#endif

/*
 * word delimiter string
 *
 * More advanced example: L" `'\"()[]{}"
 */
const wchar_t *worddelimiters = L" ";

#if 0 /* TODO */
/* selection timeouts (in milliseconds) */
static const unsigned int doubleclicktimeout = 300;
static const unsigned int tripleclicktimeout = 600;
#endif

/* alt screens */
int allowaltscreen = 1;

/* allow certain non-interactive (insecure) window operations such as:
   setting the clipboard text */
const int allowwindowops = 0;

/*
 * draw latency range in ms - from new content/keypress/etc until drawing.
 * within this range, st draws when content stops arriving (idle). mostly it's
 * near minlatency, but it waits longer for slow updates to avoid partial draw.
 * low minlatency will tear/flicker more, as it can "detect" idle too early.
 */
static const double minlatency = 2;
static const double maxlatency = 33;

/*
 * blinking timeout (set to 0 to disable blinking) for the terminal blinking
 * attribute.
 */
static const unsigned int blinktimeout = 800;

/*
 * thickness of underline and bar cursors
 */
static const unsigned int cursorthickness = 2;

#if 0 /* TODO */
/*
 * bell volume. It must be a value between -100 and 100. Use 0 for disabling
 * it
 */
static const int bellvolume = 0;
#endif

/* default TERM value */
const char *termname = "st-256color"; /* TODO: change to swt-256color */
const char *title    = "swt";
const char *app_id   = "swt";

/*
 * spaces per tab
 *
 * When you are changing this value, don't forget to adapt the »it« value in
 * the st.info and appropriately install the st.info in the environment where
 * you use this st version.
 *
 *	it#$tabspaces,
 *
 * Secondly make sure your kernel is not expanding tabs. When running `stty
 * -a` »tab0« should appear. You can tell the terminal to not expand tabs by
 *  running following command:
 *
 *	stty tabs
 */
const unsigned int tabspaces = 8;

/* Terminal colors (16 first used in escape sequence) */
/* clang-format off */
static const uint32_t colorpalette[] = {
#if 0
	/* 8 normal colors */
	0x282828,
	0xea6962,
	0xa9b665,
	0xd8a657,
	0x7daea3,
	0xd3869b,
	0x89b482,
	0xd4be98,
	/* 8 bright colors */
	0x928374,
	0xef938e,
	0xbbc585,
	0xe1bb7e,
	0x9dc2ba,
	0xe1acbb,
	0xa7c7a2,
	0xe2d3ba,
	/* foreground */
	0xd4be98,
	/* background */
	0x282828,
#else
	/* 8 normal colors */
	0xfbf1c7,
	0xc14a4a,
	0x6c782e,
	0xc35e0a,
	0x45707a,
	0x945e80,
	0x4c7a5d,
	0x654735,
	/* 8 bright colors */
	0x928374,
	0xc14a4a,
	0x6c782e,
	0xb47109,
	0x45707a,
	0x945e80,
	0x4c7a5d,
	0x654735,
	/* foreground */
	0x654735,
	/* background */
	0xfbf1c7,
#endif
	[255] = 0,
	/* NOTE: one may define more colors, but it requires increasing size in the config struct */
};
/* clang-format on */

/*
 * Default colors (colorname index)
 * foreground, background, cursor, reverse cursor
 */
const unsigned int defaultfg = 7;
const unsigned int defaultbg = 0;
const unsigned int defaultcs = 7;
#if 0 /* TODO */
static const unsigned int defaultrcs = 0;
#endif

/*
 * Default shape of cursor
 * 2: Block ("█")
 * 4: Underline ("_")
 * 6: Bar ("|")
 * 7: Snowman ("☃"), actually there is no snowman
 */
static const unsigned int cursorshape = 2;

/*
 * Default columns and rows numbers
 */

static unsigned int cols = 80;
static unsigned int rows = 24;

#if 0 /* TODO */
/*
 * Default colour and shape of the mouse cursor
 */
static const unsigned int mouseshape = XC_xterm;
static const unsigned int mousefg    = 7;
static const unsigned int mousebg    = 0;
#endif

#if 0 /* TODO */
/*
 * Color used to display font attributes when fontconfig selected a font which
 * doesn't match the ones requested.
 */
static const unsigned int defaultattr = 11;
#endif

#if 0 /* TODO */
/*
 * Force mouse select/shortcuts while mask is active (when MODE_MOUSE is set).
 * Note that if you want to use ShiftMask with selmasks, set this to an other
 * modifier, set to 0 to not use it.
 */
static const uint forcemousemod = ShiftMask;
#endif

/*
 * Internal mouse shortcuts.
 * Beware that overloading Button1 will disable the selection.
 */
#if 0 /* TODO */
static const MouseShortcut mshortcuts[] = {
    /* mask                 button   function        argument       release */
    {XK_ANY_MOD, Button2, selpaste, {.i = 0}, 1},
    {ShiftMask, Button4, ttysend, {.s = "\033[5;2~"}},
    {XK_ANY_MOD, Button4, ttysend, {.s = "\031"}},
    {ShiftMask, Button5, ttysend, {.s = "\033[6;2~"}},
    {XK_ANY_MOD, Button5, ttysend, {.s = "\005"}},
};
#endif

/* Internal keyboard shortcuts. */
#define MODKEY  Mod1Mask
#define TERMMOD (ControlMask | ShiftMask)

/* clang-format off */
static const Shortcut shortcuts[] = {
    /* mask                 keysym          function        argument */
#if 0 /* TODO */
    {XK_ANY_MOD,  XKB_KEY_Break,    sendbreak,     {.i =  0} },
    {ControlMask, XKB_KEY_Print,    toggleprinter, {.i =  0} },
    {ShiftMask,   XKB_KEY_Print,    printscreen,   {.i =  0} },
    {XK_ANY_MOD,  XKB_KEY_Print,    printsel,      {.i =  0} },
#endif
    {TERMMOD,     XKB_KEY_Prior,    zoom,          {.f = +1} },
    {TERMMOD,     XKB_KEY_Next,     zoom,          {.f = -1} },
    {TERMMOD,     XKB_KEY_Home,     zoomreset,     {.f =  0} },
#if 0 /* TODO */
    {TERMMOD,     XKB_KEY_C,        clipcopy,      {.i =  0} },
    {TERMMOD,     XKB_KEY_V,        clippaste,     {.i =  0} },
    {TERMMOD,     XKB_KEY_Y,        selpaste,      {.i =  0} },
    {ShiftMask,   XKB_KEY_Insert,   selpaste,      {.i =  0} },
#endif
    {TERMMOD,     XKB_KEY_Num_Lock, numlock,       {.i =  0} },
};
/* clang-format on */

/*
 * Special keys (change & recompile st.info accordingly)
 *
 * Mask value:
 * * Use XK_ANY_MOD to match the key no matter modifiers state
 * * Use XK_NO_MOD to match the key alone (no modifiers)
 * appkey value:
 * * 0: no value
 * * > 0: keypad application mode enabled
 * *   = 2: term.numlock = 1
 * * < 0: keypad application mode disabled
 * appcursor value:
 * * 0: no value
 * * > 0: cursor application mode enabled
 * * < 0: cursor application mode disabled
 *
 * Be careful with the order of the definitions because st searches in
 * this table sequentially, so any XK_ANY_MOD must be in the last
 * position for a key.
 */

#if 0 /* TODO */
/*
 * If you want keys other than the X11 function keys (0xFD00 - 0xFFFF)
 * to be mapped below, add them to this array.
 */
static const xkb_keysym_t mappedkeys[] = {-1};
#endif

/*
 * State bits to ignore when matching key or button events.  By default,
 * numlock (Mod2Mask) and keyboard layout (XK_SWITCH_MOD) are ignored.
 */
static const uint ignoremod = Mod2Mask | XKB_SWITCH_MOD;

/*
 * This is the huge key array which defines all compatibility to the Linux
 * world. Please decide about changes wisely.
 */
static const Key key[] = {
    /* keysym           mask            string      appkey appcursor */
    {XKB_KEY_KP_Home,      ShiftMask,                          "\033[2J",    0,  -1},
    {XKB_KEY_KP_Home,      ShiftMask,                          "\033[1;2H",  0,  +1},
    {XKB_KEY_KP_Home,      XKB_ANY_MOD,                        "\033[H",     0,  -1},
    {XKB_KEY_KP_Home,      XKB_ANY_MOD,                        "\033[1~",    0,  +1},
    {XKB_KEY_KP_Up,        XKB_ANY_MOD,                        "\033Ox",     +1, 0 },
    {XKB_KEY_KP_Up,        XKB_ANY_MOD,                        "\033[A",     0,  -1},
    {XKB_KEY_KP_Up,        XKB_ANY_MOD,                        "\033OA",     0,  +1},
    {XKB_KEY_KP_Down,      XKB_ANY_MOD,                        "\033Or",     +1, 0 },
    {XKB_KEY_KP_Down,      XKB_ANY_MOD,                        "\033[B",     0,  -1},
    {XKB_KEY_KP_Down,      XKB_ANY_MOD,                        "\033OB",     0,  +1},
    {XKB_KEY_KP_Left,      XKB_ANY_MOD,                        "\033Ot",     +1, 0 },
    {XKB_KEY_KP_Left,      XKB_ANY_MOD,                        "\033[D",     0,  -1},
    {XKB_KEY_KP_Left,      XKB_ANY_MOD,                        "\033OD",     0,  +1},
    {XKB_KEY_KP_Right,     XKB_ANY_MOD,                        "\033Ov",     +1, 0 },
    {XKB_KEY_KP_Right,     XKB_ANY_MOD,                        "\033[C",     0,  -1},
    {XKB_KEY_KP_Right,     XKB_ANY_MOD,                        "\033OC",     0,  +1},
    {XKB_KEY_KP_Prior,     ShiftMask,                          "\033[5;2~",  0,  0 },
    {XKB_KEY_KP_Prior,     XKB_ANY_MOD,                        "\033[5~",    0,  0 },
    {XKB_KEY_KP_Begin,     XKB_ANY_MOD,                        "\033[E",     0,  0 },
    {XKB_KEY_KP_End,       ControlMask,                        "\033[J",     -1, 0 },
    {XKB_KEY_KP_End,       ControlMask,                        "\033[1;5F",  +1, 0 },
    {XKB_KEY_KP_End,       ShiftMask,                          "\033[K",     -1, 0 },
    {XKB_KEY_KP_End,       ShiftMask,                          "\033[1;2F",  +1, 0 },
    {XKB_KEY_KP_End,       XKB_ANY_MOD,                        "\033[4~",    0,  0 },
    {XKB_KEY_KP_Next,      ShiftMask,                          "\033[6;2~",  0,  0 },
    {XKB_KEY_KP_Next,      XKB_ANY_MOD,                        "\033[6~",    0,  0 },
    {XKB_KEY_KP_Insert,    ShiftMask,                          "\033[2;2~",  +1, 0 },
    {XKB_KEY_KP_Insert,    ShiftMask,                          "\033[4l",    -1, 0 },
    {XKB_KEY_KP_Insert,    ControlMask,                        "\033[L",     -1, 0 },
    {XKB_KEY_KP_Insert,    ControlMask,                        "\033[2;5~",  +1, 0 },
    {XKB_KEY_KP_Insert,    XKB_ANY_MOD,                        "\033[4h",    -1, 0 },
    {XKB_KEY_KP_Insert,    XKB_ANY_MOD,                        "\033[2~",    +1, 0 },
    {XKB_KEY_KP_Delete,    ControlMask,                        "\033[M",     -1, 0 },
    {XKB_KEY_KP_Delete,    ControlMask,                        "\033[3;5~",  +1, 0 },
    {XKB_KEY_KP_Delete,    ShiftMask,                          "\033[2K",    -1, 0 },
    {XKB_KEY_KP_Delete,    ShiftMask,                          "\033[3;2~",  +1, 0 },
    {XKB_KEY_KP_Delete,    XKB_ANY_MOD,                        "\033[P",     -1, 0 },
    {XKB_KEY_KP_Delete,    XKB_ANY_MOD,                        "\033[3~",    +1, 0 },
    {XKB_KEY_KP_Multiply,  XKB_ANY_MOD,                        "\033Oj",     +2, 0 },
    {XKB_KEY_KP_Add,       XKB_ANY_MOD,                        "\033Ok",     +2, 0 },
    {XKB_KEY_KP_Enter,     XKB_ANY_MOD,                        "\033OM",     +2, 0 },
    {XKB_KEY_KP_Enter,     XKB_ANY_MOD,                        "\r",         -1, 0 },
    {XKB_KEY_KP_Subtract,  XKB_ANY_MOD,                        "\033Om",     +2, 0 },
    {XKB_KEY_KP_Decimal,   XKB_ANY_MOD,                        "\033On",     +2, 0 },
    {XKB_KEY_KP_Divide,    XKB_ANY_MOD,                        "\033Oo",     +2, 0 },
    {XKB_KEY_KP_0,         XKB_ANY_MOD,                        "\033Op",     +2, 0 },
    {XKB_KEY_KP_1,         XKB_ANY_MOD,                        "\033Oq",     +2, 0 },
    {XKB_KEY_KP_2,         XKB_ANY_MOD,                        "\033Or",     +2, 0 },
    {XKB_KEY_KP_3,         XKB_ANY_MOD,                        "\033Os",     +2, 0 },
    {XKB_KEY_KP_4,         XKB_ANY_MOD,                        "\033Ot",     +2, 0 },
    {XKB_KEY_KP_5,         XKB_ANY_MOD,                        "\033Ou",     +2, 0 },
    {XKB_KEY_KP_6,         XKB_ANY_MOD,                        "\033Ov",     +2, 0 },
    {XKB_KEY_KP_7,         XKB_ANY_MOD,                        "\033Ow",     +2, 0 },
    {XKB_KEY_KP_8,         XKB_ANY_MOD,                        "\033Ox",     +2, 0 },
    {XKB_KEY_KP_9,         XKB_ANY_MOD,                        "\033Oy",     +2, 0 },
    {XKB_KEY_Up,           ShiftMask,                          "\033[1;2A",  0,  0 },
    {XKB_KEY_Up,           Mod1Mask,			   "\033[1;3A",  0,  0 },
    {XKB_KEY_Up,           ShiftMask | Mod1Mask,               "\033[1;4A",  0,  0 },
    {XKB_KEY_Up,           ControlMask,                        "\033[1;5A",  0,  0 },
    {XKB_KEY_Up,           ShiftMask | ControlMask,            "\033[1;6A",  0,  0 },
    {XKB_KEY_Up,           ControlMask | Mod1Mask,             "\033[1;7A",  0,  0 },
    {XKB_KEY_Up,           ShiftMask | ControlMask | Mod1Mask, "\033[1;8A",  0,  0 },
    {XKB_KEY_Up,           XKB_ANY_MOD,                        "\033[A",     0,  -1},
    {XKB_KEY_Up,           XKB_ANY_MOD,                        "\033OA",     0,  +1},
    {XKB_KEY_Down,         ShiftMask,                          "\033[1;2B",  0,  0 },
    {XKB_KEY_Down,         Mod1Mask,                           "\033[1;3B",  0,  0 },
    {XKB_KEY_Down,         ShiftMask | Mod1Mask,               "\033[1;4B",  0,  0 },
    {XKB_KEY_Down,         ControlMask,                        "\033[1;5B",  0,  0 },
    {XKB_KEY_Down,         ShiftMask | ControlMask,            "\033[1;6B",  0,  0 },
    {XKB_KEY_Down,         ControlMask | Mod1Mask,             "\033[1;7B",  0,  0 },
    {XKB_KEY_Down,         ShiftMask | ControlMask | Mod1Mask, "\033[1;8B",  0,  0 },
    {XKB_KEY_Down,         XKB_ANY_MOD,                        "\033[B",     0,  -1},
    {XKB_KEY_Down,         XKB_ANY_MOD,                        "\033OB",     0,  +1},
    {XKB_KEY_Left,         ShiftMask,                          "\033[1;2D",  0,  0 },
    {XKB_KEY_Left,         Mod1Mask,                           "\033[1;3D",  0,  0 },
    {XKB_KEY_Left,         ShiftMask | Mod1Mask,               "\033[1;4D",  0,  0 },
    {XKB_KEY_Left,         ControlMask,                        "\033[1;5D",  0,  0 },
    {XKB_KEY_Left,         ShiftMask | ControlMask,            "\033[1;6D",  0,  0 },
    {XKB_KEY_Left,         ControlMask | Mod1Mask,             "\033[1;7D",  0,  0 },
    {XKB_KEY_Left,         ShiftMask | ControlMask | Mod1Mask, "\033[1;8D",  0,  0 },
    {XKB_KEY_Left,         XKB_ANY_MOD,                        "\033[D",     0,  -1},
    {XKB_KEY_Left,         XKB_ANY_MOD,                        "\033OD",     0,  +1},
    {XKB_KEY_Right,        ShiftMask,                          "\033[1;2C",  0,  0 },
    {XKB_KEY_Right,        Mod1Mask,                           "\033[1;3C",  0,  0 },
    {XKB_KEY_Right,        ShiftMask | Mod1Mask,               "\033[1;4C",  0,  0 },
    {XKB_KEY_Right,        ControlMask,                        "\033[1;5C",  0,  0 },
    {XKB_KEY_Right,        ShiftMask | ControlMask,            "\033[1;6C",  0,  0 },
    {XKB_KEY_Right,        ControlMask | Mod1Mask,             "\033[1;7C",  0,  0 },
    {XKB_KEY_Right,        ShiftMask | ControlMask | Mod1Mask, "\033[1;8C",  0,  0 },
    {XKB_KEY_Right,        XKB_ANY_MOD,                        "\033[C",     0,  -1},
    {XKB_KEY_Right,        XKB_ANY_MOD,                        "\033OC",     0,  +1},
    {XKB_KEY_ISO_Left_Tab, ShiftMask,                          "\033[Z",     0,  0 },
    {XKB_KEY_Return,       Mod1Mask,                           "\033\r",     0,  0 },
    {XKB_KEY_Return,       XKB_ANY_MOD,                        "\r",         0,  0 },
    {XKB_KEY_Insert,       ShiftMask,                          "\033[4l",    -1, 0 },
    {XKB_KEY_Insert,       ShiftMask,                          "\033[2;2~",  +1, 0 },
    {XKB_KEY_Insert,       ControlMask,                        "\033[L",     -1, 0 },
    {XKB_KEY_Insert,       ControlMask,                        "\033[2;5~",  +1, 0 },
    {XKB_KEY_Insert,       XKB_ANY_MOD,                        "\033[4h",    -1, 0 },
    {XKB_KEY_Insert,       XKB_ANY_MOD,                        "\033[2~",    +1, 0 },
    {XKB_KEY_Delete,       ControlMask,                        "\033[M",     -1, 0 },
    {XKB_KEY_Delete,       ControlMask,                        "\033[3;5~",  +1, 0 },
    {XKB_KEY_Delete,       ShiftMask,                          "\033[2K",    -1, 0 },
    {XKB_KEY_Delete,       ShiftMask,                          "\033[3;2~",  +1, 0 },
    {XKB_KEY_Delete,       XKB_ANY_MOD,                        "\033[P",     -1, 0 },
    {XKB_KEY_Delete,       XKB_ANY_MOD,                        "\033[3~",    +1, 0 },
    {XKB_KEY_BackSpace,    XKB_NO_MOD,                         "\177",       0,  0 },
    {XKB_KEY_BackSpace,    Mod1Mask,                           "\033\177",   0,  0 },
    {XKB_KEY_Home,         ShiftMask,                          "\033[2J",    0,  -1},
    {XKB_KEY_Home,         ShiftMask,                          "\033[1;2H",  0,  +1},
    {XKB_KEY_Home,         XKB_ANY_MOD,                        "\033[H",     0,  -1},
    {XKB_KEY_Home,         XKB_ANY_MOD,                        "\033[1~",    0,  +1},
    {XKB_KEY_End,          ControlMask,                        "\033[J",     -1, 0 },
    {XKB_KEY_End,          ControlMask,                        "\033[1;5F",  +1, 0 },
    {XKB_KEY_End,          ShiftMask,                          "\033[K",     -1, 0 },
    {XKB_KEY_End,          ShiftMask,                          "\033[1;2F",  +1, 0 },
    {XKB_KEY_End,          XKB_ANY_MOD,                        "\033[4~",    0,  0 },
    {XKB_KEY_Prior,        ControlMask,                        "\033[5;5~",  0,  0 },
    {XKB_KEY_Prior,        ShiftMask,                          "\033[5;2~",  0,  0 },
    {XKB_KEY_Prior,        XKB_ANY_MOD,                        "\033[5~",    0,  0 },
    {XKB_KEY_Next,         ControlMask,                        "\033[6;5~",  0,  0 },
    {XKB_KEY_Next,         ShiftMask,                          "\033[6;2~",  0,  0 },
    {XKB_KEY_Next,         XKB_ANY_MOD,                        "\033[6~",    0,  0 },
    {XKB_KEY_F1,           XKB_NO_MOD,                         "\033OP",     0,  0 },
    {XKB_KEY_F1,           /* F13 */ ShiftMask,                "\033[1;2P",  0,  0 },
    {XKB_KEY_F1,           /* F25 */ ControlMask,              "\033[1;5P",  0,  0 },
    {XKB_KEY_F1,           /* F37 */ Mod4Mask,                 "\033[1;6P",  0,  0 },
    {XKB_KEY_F1,           /* F49 */ Mod1Mask,                 "\033[1;3P",  0,  0 },
    {XKB_KEY_F1,           /* F61 */ Mod3Mask,                 "\033[1;4P",  0,  0 },
    {XKB_KEY_F2,           XKB_NO_MOD,                         "\033OQ",     0,  0 },
    {XKB_KEY_F2,           /* F14 */ ShiftMask,                "\033[1;2Q",  0,  0 },
    {XKB_KEY_F2,           /* F26 */ ControlMask,              "\033[1;5Q",  0,  0 },
    {XKB_KEY_F2,           /* F38 */ Mod4Mask,                 "\033[1;6Q",  0,  0 },
    {XKB_KEY_F2,           /* F50 */ Mod1Mask,                 "\033[1;3Q",  0,  0 },
    {XKB_KEY_F2,           /* F62 */ Mod3Mask,                 "\033[1;4Q",  0,  0 },
    {XKB_KEY_F3,           XKB_NO_MOD,                         "\033OR",     0,  0 },
    {XKB_KEY_F3,           /* F15 */ ShiftMask,                "\033[1;2R",  0,  0 },
    {XKB_KEY_F3,           /* F27 */ ControlMask,              "\033[1;5R",  0,  0 },
    {XKB_KEY_F3,           /* F39 */ Mod4Mask,                 "\033[1;6R",  0,  0 },
    {XKB_KEY_F3,           /* F51 */ Mod1Mask,                 "\033[1;3R",  0,  0 },
    {XKB_KEY_F3,           /* F63 */ Mod3Mask,                 "\033[1;4R",  0,  0 },
    {XKB_KEY_F4,           XKB_NO_MOD,                         "\033OS",     0,  0 },
    {XKB_KEY_F4,           /* F16 */ ShiftMask,                "\033[1;2S",  0,  0 },
    {XKB_KEY_F4,           /* F28 */ ControlMask,              "\033[1;5S",  0,  0 },
    {XKB_KEY_F4,           /* F40 */ Mod4Mask,                 "\033[1;6S",  0,  0 },
    {XKB_KEY_F4,           /* F52 */ Mod1Mask,                 "\033[1;3S",  0,  0 },
    {XKB_KEY_F5,           XKB_NO_MOD,                         "\033[15~",   0,  0 },
    {XKB_KEY_F5,           /* F17 */ ShiftMask,                "\033[15;2~", 0,  0 },
    {XKB_KEY_F5,           /* F29 */ ControlMask,              "\033[15;5~", 0,  0 },
    {XKB_KEY_F5,           /* F41 */ Mod4Mask,                 "\033[15;6~", 0,  0 },
    {XKB_KEY_F5,           /* F53 */ Mod1Mask,                 "\033[15;3~", 0,  0 },
    {XKB_KEY_F6,           XKB_NO_MOD,                         "\033[17~",   0,  0 },
    {XKB_KEY_F6,           /* F18 */ ShiftMask,                "\033[17;2~", 0,  0 },
    {XKB_KEY_F6,           /* F30 */ ControlMask,              "\033[17;5~", 0,  0 },
    {XKB_KEY_F6,           /* F42 */ Mod4Mask,                 "\033[17;6~", 0,  0 },
    {XKB_KEY_F6,           /* F54 */ Mod1Mask,                 "\033[17;3~", 0,  0 },
    {XKB_KEY_F7,           XKB_NO_MOD,                         "\033[18~",   0,  0 },
    {XKB_KEY_F7,           /* F19 */ ShiftMask,                "\033[18;2~", 0,  0 },
    {XKB_KEY_F7,           /* F31 */ ControlMask,              "\033[18;5~", 0,  0 },
    {XKB_KEY_F7,           /* F43 */ Mod4Mask,                 "\033[18;6~", 0,  0 },
    {XKB_KEY_F7,           /* F55 */ Mod1Mask,                 "\033[18;3~", 0,  0 },
    {XKB_KEY_F8,           XKB_NO_MOD,                         "\033[19~",   0,  0 },
    {XKB_KEY_F8,           /* F20 */ ShiftMask,                "\033[19;2~", 0,  0 },
    {XKB_KEY_F8,           /* F32 */ ControlMask,              "\033[19;5~", 0,  0 },
    {XKB_KEY_F8,           /* F44 */ Mod4Mask,                 "\033[19;6~", 0,  0 },
    {XKB_KEY_F8,           /* F56 */ Mod1Mask,                 "\033[19;3~", 0,  0 },
    {XKB_KEY_F9,           XKB_NO_MOD,                         "\033[20~",   0,  0 },
    {XKB_KEY_F9,           /* F21 */ ShiftMask,                "\033[20;2~", 0,  0 },
    {XKB_KEY_F9,           /* F33 */ ControlMask,              "\033[20;5~", 0,  0 },
    {XKB_KEY_F9,           /* F45 */ Mod4Mask,                 "\033[20;6~", 0,  0 },
    {XKB_KEY_F9,           /* F57 */ Mod1Mask,                 "\033[20;3~", 0,  0 },
    {XKB_KEY_F10,          XKB_NO_MOD,                         "\033[21~",   0,  0 },
    {XKB_KEY_F10,          /* F22 */ ShiftMask,                "\033[21;2~", 0,  0 },
    {XKB_KEY_F10,          /* F34 */ ControlMask,              "\033[21;5~", 0,  0 },
    {XKB_KEY_F10,          /* F46 */ Mod4Mask,                 "\033[21;6~", 0,  0 },
    {XKB_KEY_F10,          /* F58 */ Mod1Mask,                 "\033[21;3~", 0,  0 },
    {XKB_KEY_F11,          XKB_NO_MOD,                         "\033[23~",   0,  0 },
    {XKB_KEY_F11,          /* F23 */ ShiftMask,                "\033[23;2~", 0,  0 },
    {XKB_KEY_F11,          /* F35 */ ControlMask,              "\033[23;5~", 0,  0 },
    {XKB_KEY_F11,          /* F47 */ Mod4Mask,                 "\033[23;6~", 0,  0 },
    {XKB_KEY_F11,          /* F59 */ Mod1Mask,                 "\033[23;3~", 0,  0 },
    {XKB_KEY_F12,          XKB_NO_MOD,                         "\033[24~",   0,  0 },
    {XKB_KEY_F12,          /* F24 */ ShiftMask,                "\033[24;2~", 0,  0 },
    {XKB_KEY_F12,          /* F36 */ ControlMask,              "\033[24;5~", 0,  0 },
    {XKB_KEY_F12,          /* F48 */ Mod4Mask,                 "\033[24;6~", 0,  0 },
    {XKB_KEY_F12,          /* F60 */ Mod1Mask,                 "\033[24;3~", 0,  0 },
    {XKB_KEY_F13,          XKB_NO_MOD,                         "\033[1;2P",  0,  0 },
    {XKB_KEY_F14,          XKB_NO_MOD,                         "\033[1;2Q",  0,  0 },
    {XKB_KEY_F15,          XKB_NO_MOD,                         "\033[1;2R",  0,  0 },
    {XKB_KEY_F16,          XKB_NO_MOD,                         "\033[1;2S",  0,  0 },
    {XKB_KEY_F17,          XKB_NO_MOD,                         "\033[15;2~", 0,  0 },
    {XKB_KEY_F18,          XKB_NO_MOD,                         "\033[17;2~", 0,  0 },
    {XKB_KEY_F19,          XKB_NO_MOD,                         "\033[18;2~", 0,  0 },
    {XKB_KEY_F20,          XKB_NO_MOD,                         "\033[19;2~", 0,  0 },
    {XKB_KEY_F21,          XKB_NO_MOD,                         "\033[20;2~", 0,  0 },
    {XKB_KEY_F22,          XKB_NO_MOD,                         "\033[21;2~", 0,  0 },
    {XKB_KEY_F23,          XKB_NO_MOD,                         "\033[23;2~", 0,  0 },
    {XKB_KEY_F24,          XKB_NO_MOD,                         "\033[24;2~", 0,  0 },
    {XKB_KEY_F25,          XKB_NO_MOD,                         "\033[1;5P",  0,  0 },
    {XKB_KEY_F26,          XKB_NO_MOD,                         "\033[1;5Q",  0,  0 },
    {XKB_KEY_F27,          XKB_NO_MOD,                         "\033[1;5R",  0,  0 },
    {XKB_KEY_F28,          XKB_NO_MOD,                         "\033[1;5S",  0,  0 },
    {XKB_KEY_F29,          XKB_NO_MOD,                         "\033[15;5~", 0,  0 },
    {XKB_KEY_F30,          XKB_NO_MOD,                         "\033[17;5~", 0,  0 },
    {XKB_KEY_F31,          XKB_NO_MOD,                         "\033[18;5~", 0,  0 },
    {XKB_KEY_F32,          XKB_NO_MOD,                         "\033[19;5~", 0,  0 },
    {XKB_KEY_F33,          XKB_NO_MOD,                         "\033[20;5~", 0,  0 },
    {XKB_KEY_F34,          XKB_NO_MOD,                         "\033[21;5~", 0,  0 },
    {XKB_KEY_F35,          XKB_NO_MOD,                         "\033[23;5~", 0,  0 },
};

#if 0 /* TODO */
/*
 * Selection types' masks.
 * Use the same masks as usual.
 * Button1Mask is always unset, to make masks match between ButtonPress.
 * ButtonRelease and MotionNotify.
 * If no match is found, regular selection is used.
 */
static const uint selmasks[] = {
    [SEL_RECTANGULAR] = Mod1Mask,
};
#endif

#if 0 /* TODO */
/*
 * Printable characters in ASCII, used to estimate the advance width
 * of single wide characters.
 */
static const char ascii_printable[] = " !\"#$%&'()*+,-./0123456789:;<=>?"
				"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
				"`abcdefghijklmnopqrstuvwxyz{|}~";
#endif
