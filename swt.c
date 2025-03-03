/* See LICENSE file for copyright and license details. */

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

#include <fcft/fcft.h>
#include <linux/input-event-codes.h>
#include <pixman-1/pixman.h>
#include <xkbcommon/xkbcommon.h>

#include <wayland-client-protocol.h>
#include <wayland-cursor.h>

#include "xdg-shell-protocol.h"

char *argv0;
#include "arg.h"
#include "bufpool.h"
#include "st.h"
#include "util.h"
#include "win.h"

/* types used in config.h */
typedef struct {
	uint mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Shortcut;

typedef struct {
	uint mod;
	uint button;
	void (*func)(const Arg *);
	const Arg arg;
	uint release;
} MouseShortcut;

typedef struct {
	xkb_keysym_t k;
	uint mask;
	char *s;
	/* three-valued logic variables: 0 indifferent, 1 on, -1 off */
	signed char appkey;    /* application keypad */
	signed char appcursor; /* application cursor */
} Key;

/* xkb modifiers */
#define XKB_ANY_MOD    (UINT_MAX)
#define XKB_NO_MOD     (0)
#define XKB_SWITCH_MOD (1 << 13 | 1 << 14)
#define ShiftMask      (1 << 0)
#define LockMask       (1 << 1)
#define ControlMask    (1 << 2)
#define Mod1Mask       (1 << 3)
#define Mod2Mask       (1 << 4)
#define Mod3Mask       (1 << 5)
#define Mod4Mask       (1 << 6)
#define Mod5Mask       (1 << 7)
#define Button1Mask    (1 << 8)
#define Button2Mask    (1 << 9)
#define Button3Mask    (1 << 10)
#define Button4Mask    (1 << 11)
#define Button5Mask    (1 << 12)

#define Button1 (1)
#define Button2 (2)
#define Button3 (3)
#define Button4 (4)
#define Button5 (5)

/* function definitions used in config.h */
static void numlock(const Arg *);
static void zoom(const Arg *);
static void zoomabs(const Arg *);
static void zoomreset(const Arg *);
static void ttysend(const Arg *);
static void changealpha(const Arg *);

/* config.h for applying patches and the configuration. */
#include "config.h"

/* macros */
#define IS_SET(flag) ((win.mode & (flag)) != 0)
#define TRUERED(x)   (((x) & 0xff0000) >> 8)
#define TRUEGREEN(x) (((x) & 0xff00))
#define TRUEBLUE(x)  (((x) & 0xff) << 8)
#define GETPIXMANCOLOR(c)                                                      \
	(pixman_color_t *)(IS_TRUECOL(c)                                       \
			       ? &(pixman_color_t){TRUERED(c), TRUEGREEN(c),   \
						   TRUEBLUE(c), 0xFFFF}        \
			       : &(pixman_color_t){                            \
				     dc.col[c].red, dc.col[c].green,           \
				     dc.col[c].blue, dc.col[c].alpha})

/* Purely graphic info */
typedef struct {
	int tw, th; /* tty width and height */
	int w, h;   /* window width and height */
	int ch;     /* char height */
	int cw;     /* char width  */
	int mode;   /* window state/mode flags */
	int cursor; /* cursor style */
} TermWindow;

struct swt_wl {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_surface *surface;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_output *output;
	uint32_t scale;
	struct wl_pointer *pointer;
	struct {
		uint32_t serial;

		struct wl_surface *surface;
		struct wl_cursor_theme *theme;
		struct wl_cursor *cursor;
		struct wl_callback *callback;

		double scroll[2];

		int x;
		int y;
	} cursor;

	struct wl_callback *callback;
};

struct swt_xdg {
	struct xdg_surface *surface;
	struct xdg_toplevel *toplevel;
	struct xdg_wm_base *wm_base;
};

struct swt_xkb {
	struct xkb_context *context;
	struct xkb_state *state;
	struct xkb_keymap *keymap;
	uint mods_mask;

	struct {
		uint32_t key;
		xkb_keysym_t keysym;
		struct itimerspec ts;
	} repeat;
};

struct swt {
	pid_t pid;

	BufPool pool;
	pixman_image_t *pix;

	struct {
		int display;
		int signal;
		int pty;
		int repeat;
	} fd;

	bool running   : 1;
	bool need_draw : 1;
};

/* Drawing Context */
typedef struct {
	pixman_color_t *col;
	size_t collen;
	struct fcft_font *font[4];
	int fontsize;
} DC;

/* function definitions for wayland {{{ */
/* clang-format off */
static void noop();

static void wl_callback_done(void *data, struct wl_callback *wl_callback, uint32_t callback_data);
static void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys);
static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size);
static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
static void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface);
static void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
static void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay);
static void wl_output_done(void *data, struct wl_output *wl_output);
static void wl_output_scale(void *data, struct wl_output *wl_output, int32_t factor);
static void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis);
static void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y);
static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface);
static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
static void wl_registry_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version);
static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities);
static void wl_shm_format(void *data, struct wl_shm *wl_shm, uint32_t format);

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel);
static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
/* clang-format on */
/* }}} */

static inline ushort sixd_to_16bit(int);
#ifndef LIGATURES
static void xdrawglyph(Glyph, int, int);
#else
static void xdrawglyph(Glyph, int, int, const struct fcft_glyph *);
static void xdrawglyphbg(Glyph, int, int);
#endif
static void xclear(int, int, int, int);
static void xdrawunderline(Glyph, int, int, struct fcft_font *,
			   pixman_color_t *);
static void cresize(int, int);
static void xresize(int, int);
static int xloadcolor(int, const char *, pixman_color_t *);
static void xloadfonts(const char *, double);
static void xunloadfonts(void);
static void xseturgency(int);
static int evcol(void);
static int evrow(void);

static void kpress(uint32_t key, xkb_keysym_t keysym);
static uint buttonmask(uint);
static int mouseaction(uint, uint);
static void brelease(uint);
static void bpress(uint);
static void bmotion(void);
static void mousereport(uint, bool, bool);
static char *kmap(xkb_keysym_t, uint);
static int match(uint, uint);

static void xinit(int, int);
static void setup(void);
static void run(void);
static void usage(void);
static void cleanup(void);

/* globals */
static DC dc;
static TermWindow win;

static struct swt swt;
static struct swt_wl wl;
static struct swt_xdg xdg;
static struct swt_xkb xkb;

static char *usedfont         = NULL;
static double usedfontsize    = 0;
static double defaultfontsize = 0;

static char *opt_class = NULL;
static char **opt_cmd  = NULL;
static char *opt_embed = NULL;
static char *opt_font  = NULL;
static char *opt_io    = NULL;
static char *opt_line  = NULL;
static char *opt_name  = NULL;
static char *opt_title = NULL;

static uint buttons; /* bit field of pressed buttons */

/* wayland listeners {{{ */
static const struct {
	struct wl_callback_listener wl_callback;
	struct wl_keyboard_listener wl_keyboard;
	struct wl_output_listener wl_output;
	struct wl_pointer_listener wl_pointer;
	struct wl_registry_listener wl_registry;
	struct wl_seat_listener wl_seat;
	struct wl_shm_listener wl_shm;
	struct xdg_surface_listener xdg_surface;
	struct xdg_toplevel_listener xdg_toplevel;
	struct xdg_wm_base_listener xdg_wm_base;
} listener = {
    .wl_callback  = {.done = wl_callback_done},
    .wl_keyboard  = {.keymap      = wl_keyboard_keymap,
		     .enter       = wl_keyboard_enter,
		     .leave       = wl_keyboard_leave,
		     .key         = wl_keyboard_key,
		     .modifiers   = wl_keyboard_modifiers,
		     .repeat_info = wl_keyboard_repeat_info},
    .wl_output    = {.geometry    = noop,
		     .mode        = noop,
		     .done        = wl_output_done,
		     .scale       = wl_output_scale,
		     .name        = noop,
		     .description = noop},
    .wl_pointer   = {.enter                   = wl_pointer_enter,
		     .leave                   = wl_pointer_leave,
		     .motion                  = wl_pointer_motion,
		     .button                  = wl_pointer_button,
		     .axis                    = wl_pointer_axis,
		     .frame                   = noop,
		     .axis_source             = noop,
		     .axis_stop               = wl_pointer_axis_stop,
		     .axis_discrete           = noop,
		     .axis_value120           = noop,
		     .axis_relative_direction = noop},
    .wl_registry  = {.global = wl_registry_global, .global_remove = noop},
    .wl_seat      = {.capabilities = wl_seat_capabilities, .name = noop},
    .wl_shm       = {.format = wl_shm_format},
    .xdg_surface  = {.configure = xdg_surface_configure},
    .xdg_toplevel = {.configure        = xdg_toplevel_configure,
		     .close            = xdg_toplevel_close,
		     .configure_bounds = noop,
		     .wm_capabilities  = noop},
    .xdg_wm_base  = {.ping = xdg_wm_base_ping},
};
/* }}} */

void numlock(const Arg *dummy)
{
	(void)dummy;
	win.mode ^= MODE_NUMLOCK;
}

void zoom(const Arg *arg)
{
	Arg larg;

	larg.f = usedfontsize + arg->f;
	zoomabs(&larg);
}

void zoomabs(const Arg *arg)
{
	xunloadfonts();
	xloadfonts(usedfont, arg->f);
	cresize(0, 0);
	redraw();
}

void zoomreset(const Arg *arg)
{
	Arg larg;

	(void)arg;

	if (defaultfontsize > 0) {
		larg.f = defaultfontsize;
		zoomabs(&larg);
	}
}

void ttysend(const Arg *arg) { ttywrite(arg->s, strlen(arg->s), 1); }

void changealpha(const Arg *arg)
{
	float old = alpha;
	alpha += arg->f;
	LIMIT(alpha, 0.0, 1.0);

	if (old == alpha) return;
	swt.need_draw = 1;
	xloadcols();
	redraw();
}

int evcol(void)
{
	int x = wl.cursor.x - borderpx;
	LIMIT(x, 0, win.tw - 1);
	return x / win.cw;
}

int evrow(void)
{
	int y = wl.cursor.y - borderpx;
	LIMIT(y, 0, win.th - 1);
	return y / win.ch;
}

void mousereport(uint button, bool is_release, bool is_motion)
{
	int len, btn, code;
	int x = evcol(), y = evrow();
	int state = xkb.mods_mask;
	char buf[40];
	static int ox, oy;

	if (is_motion) {
		if (x == ox && y == oy) return;
		if (!IS_SET(MODE_MOUSEMOTION) && !IS_SET(MODE_MOUSEMANY))
			return;
		/* MODE_MOUSEMOTION: no reporting if no button is pressed */
		if (IS_SET(MODE_MOUSEMOTION) && buttons == 0) return;
		/* Set btn to lowest-numbered pressed button, or 12 if no
		 * buttons are pressed. */
		for (btn = 1; btn <= 11 && !(buttons & (1 << (btn - 1))); btn++)
			;
		code = 32;
	} else {
		btn = button;
		/* Only buttons 1 through 11 can be encoded */
		if (btn < 1 || btn > 11) return;
		if (is_release) {
			/* MODE_MOUSEX10: no button release reporting */
			if (IS_SET(MODE_MOUSEX10)) return;
			/* Don't send release events for the scroll wheel */
			if (btn == 4 || btn == 5) return;
		}
		code = 0;
	}

	ox = x;
	oy = y;

	/* Encode btn into code. If no button is pressed for a motion event in
	 * MODE_MOUSEMANY, then encode it as a release. */
	if ((!IS_SET(MODE_MOUSESGR) && is_release) || btn == 12)
		code += 3;
	else if (btn >= 8)
		code += 128 + btn - 8;
	else if (btn >= 4)
		code += 64 + btn - 4;
	else
		code += btn - 1;

	if (!IS_SET(MODE_MOUSEX10)) {
		code += ((state & ShiftMask) ? 4 : 0) +
			((state & Mod1Mask) ? 8 : 0) /* meta key: alt */
		      + ((state & ControlMask) ? 16 : 0);
	}

	if (IS_SET(MODE_MOUSESGR)) {
		len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", code,
			       x + 1, y + 1, is_release ? 'm' : 'M');
	} else if (x < 223 && y < 223) {
		len = snprintf(buf, sizeof(buf), "\033[M%c%c%c", 32 + code,
			       32 + x + 1, 32 + y + 1);
	} else {
		return;
	}

	ttywrite(buf, len, 0);
}

uint buttonmask(uint button)
{
	return button == Button1 ? Button1Mask
	     : button == Button2 ? Button2Mask
	     : button == Button3 ? Button3Mask
	     : button == Button4 ? Button4Mask
	     : button == Button5 ? Button5Mask
				 : 0;
}

int mouseaction(uint button, uint release)
{
	const MouseShortcut *ms;

	/* ignore Button<N>mask for Button<N> - it's set on release */
	uint state = xkb.mods_mask & ~buttonmask(button);

	for (ms = mshortcuts; ms < mshortcuts + LEN(mshortcuts); ms++) {
		if (ms->release == release && ms->button == button &&
		    (match(ms->mod, state) || /* exact or forced */
		     match(ms->mod, state & ~forcemousemod))) {
			ms->func(&(ms->arg));
			return 1;
		}
	}

	return 0;
}

void bpress(uint button)
{
	int btn = button;

	if (1 <= btn && btn <= 11) buttons |= 1 << (btn - 1);

	if (IS_SET(MODE_MOUSE) && !(xkb.mods_mask & forcemousemod)) {
		mousereport(button, false, false);
		return;
	}

	if (mouseaction(btn, 0)) return;
}

void brelease(uint btn)
{
	if (1 <= btn && btn <= 11) buttons &= ~(1 << (btn - 1));
	if (IS_SET(MODE_MOUSE) && !(xkb.mods_mask & forcemousemod)) {
		mousereport(btn, true, false);
		return;
	}

	if (mouseaction(btn, 1)) return;
}

void bmotion(void)
{
	if (IS_SET(MODE_MOUSE) && !(xkb.mods_mask & forcemousemod)) {
		mousereport(-1, false, true);
		return;
	}
}

void xclipcopy(void) { warn("TODO: xclipcopy"); }

void xsetsel(char *str)
{
	(void)str;
	warn("TODO: xsetsel");
}

void cresize(int width, int height)
{
	int col, row;

	if (width != 0) win.w = width;
	if (height != 0) win.h = height;

	col = MAX(1, (win.w - 2 * borderpx) / win.cw);
	row = MAX(1, (win.h - 2 * borderpx) / win.ch);

	tresize(col, row);
	xresize(col, row);
	ttyresize(win.tw, win.th);
}

void xresize(int col, int row)
{
	win.tw = col * win.cw;
	win.th = row * win.ch;
}

ushort sixd_to_16bit(int x) { return x == 0 ? 0 : 0x3737 + 0x2828 * x; }

int xloadcolor(int i, const char *name, pixman_color_t *clr)
{
	uint32_t c;
	size_t len;
	clr->alpha = 0xFFFF;

	if (!name) {
		if (BETWEEN(i, 16, 255)) {        /* 256 color */
			if (i < 6 * 6 * 6 + 16) { /* same colors as xterm */
				clr->red   = sixd_to_16bit(((i - 16) / 36) % 6);
				clr->green = sixd_to_16bit(((i - 16) / 6) % 6);
				clr->blue  = sixd_to_16bit(((i - 16) / 1) % 6);
			} else { /* greyscale */
				clr->red =
				    0x0808 + 0x0a0a * (i - (6 * 6 * 6 + 16));
				clr->green = clr->blue = clr->red;
			}
			return 1;
		}
		/* TODO: name = colorname[i]; */
		*clr = dc.col[i];
		return 1;
	}

	if (*name++ != '#') return 0;

	len = strlen(name);
	if (len != 8 && len != 6) return 0;

	c = strtoul(name, NULL, 16);
	if (len == 6) c |= 0xFF000000;

	clr->alpha = 0xFFFF * (c >> 24) / 0xFF;
	clr->red   = TRUERED(c);
	clr->green = TRUEGREEN(c);
	clr->blue  = TRUEBLUE(c);

	return 1;
}

void xloadcols(void)
{
	uint i;
	static int loaded;

	if (!loaded) {
		dc.collen = MAX(LEN(colorname), 256);
		dc.col    = xmalloc(dc.collen * sizeof(pixman_color_t));
	}

	for (i = 0; i < dc.collen; i++) {
		if (!xloadcolor(i, colorname[i], &dc.col[i])) {
			if (colorname[i])
				die("could not allocate color '%s'",
				    colorname[i]);
			else
				die("could not allocate color %d", i);
		}
	}

	dc.col[defaultbg].alpha *= alpha;
	dc.col[defaultbg].red *= alpha;
	dc.col[defaultbg].green *= alpha;
	dc.col[defaultbg].blue *= alpha;

	loaded = 1;
}

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b)
{
	if (!BETWEEN(x, 0, (int)dc.collen - 1)) return 1;

	*r = dc.col[x].red >> 8;
	*g = dc.col[x].green >> 8;
	*b = dc.col[x].blue >> 8;

	return 0;
}

int xsetcolorname(int x, const char *name)
{
	pixman_color_t color;

	if (!BETWEEN(x, 0, (int)dc.collen - 1)) return 1;

	if (!xloadcolor(x, name, &color)) return 1;

	dc.col[x] = color;

	/* set alpha value of bg color */
	if ((unsigned int)x == defaultbg) {
		dc.col[defaultbg].alpha *= alpha;
		dc.col[defaultbg].red *= alpha;
		dc.col[defaultbg].green *= alpha;
		dc.col[defaultbg].blue *= alpha;
	}

	return 0;
}

/*
 * Absolute coordinates
 */
void xclear(int x1, int y1, int x2, int y2)
{
	pixman_image_fill_boxes(
	    PIXMAN_OP_SRC, swt.pix,
	    &dc.col[IS_SET(MODE_REVERSE) ? defaultfg : defaultbg], 1,
	    &(pixman_box32_t){x1, y1, x2, y2});
}

void xloadfonts(const char *font, double fontsize)
{
	/* NOTE: this expects the length of the font name to be less than 256 */
	char f[256];
	char dpi[12];
	unsigned i;
	char attrs[64];
	static int loaded;

	if (!loaded) {
		if (!fcft_init(FCFT_LOG_COLORIZE_AUTO, false,
			       FCFT_LOG_CLASS_ERROR))
			die("fcft_init:");
		loaded = 1;
	}

	/* TODO: use fontconfig for reading font from config */

	fcft_set_scaling_filter(FCFT_SCALING_FILTER_LANCZOS3);
	snprintf(dpi, sizeof(dpi), "dpi=%d", wl.scale * 96);

	for (i = 0; i < 4; i++) {
		sprintf(f, "%s:size=%f", font, fontsize);
		sprintf(attrs, "%s%s", dpi,
			(const char *[]){"", ":weight=bold", ":slant=italic",
					 ":weight=bold:slant=italic"}[i]);
		dc.font[i] = fcft_from_name(1, (const char *[]){f}, attrs);
	}

	usedfontsize = fontsize;

	/* FIXME: only works for monospace fonts */
	win.cw = dc.font[0]->max_advance.x;
	win.ch = dc.font[0]->height;
}

void xunloadfonts(void)
{
	unsigned i;
	for (i = 0; i < LEN(dc.font); i++)
		if (dc.font[i]) fcft_destroy(dc.font[i]);
}

void xdrawunderline(Glyph g, int x, int y, struct fcft_font *f,
		    pixman_color_t *fg)
{
	pixman_image_t *pix = swt.pix, *fill;
	pixman_color_t *uc;
	int th  = f->underline.thickness;
	int off = win.ch - (f->underline.position + f->descent);
	int top, bot, lx, rx, mx;
	int dotn = MAX(1, win.cw / (th * 2)), dx, space, i;
	pixman_rectangle16_t rects[dotn];
	int dashw;

	if (~g.mode & ATTR_UNDERLINE) return;

	uc = g.mode & ATTR_COLORED_UNDERLINE ? GETPIXMANCOLOR(g.uc) : fg;

	switch (g.us) {
	case UNDERLINE_NONE:   break;
	case UNDERLINE_SINGLE: {
		pixman_image_fill_rectangles(PIXMAN_OP_SRC, pix, uc, 1,
					     (pixman_rectangle16_t[]){
						 {x, y + off, win.cw, th},
                });
	} break;
	case UNDERLINE_DOUBLE: {
		pixman_image_fill_rectangles(
		    PIXMAN_OP_SRC, pix, uc, 2,
		    (pixman_rectangle16_t[]){
			{x, y + off,          win.cw, th},
			{x, y + off + th * 2, win.cw, th},
                });
	} break;
	case UNDERLINE_CURLY: {
		top = y + off;
		bot = top + th * 5;
		lx  = x;
		rx  = x + win.cw;
		mx  = lx + win.cw / 2;
#define I(n) pixman_int_to_fixed(n)
		fill = pixman_image_create_solid_fill(uc);
		pixman_composite_trapezoids(
		    PIXMAN_OP_OVER, fill, pix, PIXMAN_a8, 0, 0, 0, 0, 2,
		    (pixman_trapezoid_t[]){
			{I(top),
			 I(bot),
			 {{I(lx), I(bot - th)}, {I(mx), I(top - th)}},
			 {{I(lx), I(bot + th)}, {I(mx), I(top + th)}}},
			{I(top),
			 I(bot),
			 {{I(mx), I(top + th)}, {I(rx), I(bot + th)}},
			 {{I(mx), I(top - th)}, {I(rx), I(bot - th)}}},
                });
		pixman_image_unref(fill);
	} break;
	case UNDERLINE_DOTTED: {
		dx    = x;
		space = win.cw - (dotn * 2) * th;
		for (i = 0; i < dotn; i++) {
			rects[i] = (pixman_rectangle16_t){dx, y + off, th, th};
			dx += th * 2 + (i < space);
		}
		pixman_image_fill_rectangles(PIXMAN_OP_SRC, pix, uc, dotn,
					     rects);
	} break;
	case UNDERLINE_DASHED: {
		dashw = win.cw / 3 + (win.cw % 3 > 0);
		pixman_image_fill_rectangles(
		    PIXMAN_OP_SRC, pix, uc, 2,
		    (pixman_rectangle16_t[]){
			{x,             y + off, dashw, th},
			{x + dashw * 2, y + off, dashw, th},
                });
	} break;
	default: warn("unsupported underline style");
	}
}

#ifndef LIGATURES
void xdrawglyph(Glyph g, int x, int y)
#else
void xdrawglyph(Glyph g, int x, int y, const struct fcft_glyph *lig)
#endif
{
	pixman_image_t *pix = swt.pix, *fill;
	pixman_color_t *fg, *bg, *tmp;
	const struct fcft_glyph *glyph;
	struct fcft_font *f =
	    dc.font[(!!(g.mode & ATTR_BOLD)) + (!!(g.mode & ATTR_ITALIC)) * 2];
	int winx = borderpx + x * win.cw, winy = borderpx + y * win.ch;

	fg = GETPIXMANCOLOR(g.fg);
	bg = GETPIXMANCOLOR(g.bg);

	if ((g.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
		fg->red += (1 - 2 * (fg->red > bg->red)) * fg->red / 2;
		fg->green += (1 - 2 * (fg->green > bg->green)) * fg->green / 2;
		fg->blue += (1 - 2 * (fg->blue > bg->blue)) * fg->blue / 2;
	}

	if (g.mode & ATTR_REVERSE) {
		tmp = fg;
		fg  = bg;
		bg  = tmp;
	}

	if (g.mode & ATTR_BLINK && win.mode & MODE_BLINK) fg = bg;

	if (g.mode & ATTR_INVISIBLE) fg = bg;

	/* Intelligent cleaning up of the borders. */
	/* NOTE: in st it is bugged, here it is fixed */
	if (x == 0) {
		xclear(0, (y == 0) ? 0 : winy, borderpx,
		       ((winy + win.ch >= borderpx + win.th)
			    ? win.h
			    : (winy + win.ch)));
	}
	if (winx + win.cw >= borderpx + win.tw) {
		xclear(winx + win.cw, (y == 0) ? 0 : winy, win.w,
		       ((winy + win.ch >= borderpx + win.th)
			    ? win.h
			    : (winy + win.ch)));
	}
	if (y == 0) xclear(winx, 0, winx + win.cw, borderpx);
	if (winy + win.ch >= borderpx + win.th)
		xclear(winx, winy + win.ch, winx + win.cw, win.h);

#ifndef LIGATURES
	pixman_image_fill_rectangles(
	    PIXMAN_OP_SRC, pix, bg, 1,
	    &(pixman_rectangle16_t){
		.x = winx, .y = winy, .width = win.cw, .height = win.ch});

	glyph = fcft_rasterize_char_utf32(f, g.u, FCFT_SUBPIXEL_NONE);
#else
	if (lig)
		glyph = lig;
	else
		glyph = fcft_rasterize_char_utf32(f, g.u, FCFT_SUBPIXEL_NONE);
#endif

	if (!glyph) return;

	fill = pixman_image_create_solid_fill(fg);
	pixman_image_composite32(
	    PIXMAN_OP_OVER, fill, glyph->pix, pix, 0, 0, 0, 0, winx + glyph->x,
	    winy + win.ch - f->descent - glyph->y, glyph->width, glyph->height);
	pixman_image_unref(fill);

	xdrawunderline(g, winx, winy, f, fg);

	if (g.mode & ATTR_STRUCK) {
		pixman_image_fill_rectangles(
		    PIXMAN_OP_SRC, pix, fg, 1,
		    &(pixman_rectangle16_t){
			.x = winx,
			.y = winy + win.ch - f->strikeout.position - f->descent,
			.width  = win.cw,
			.height = f->underline.thickness,
		    });
	}
}

#ifdef LIGATURES
void xdrawglyphbg(Glyph g, int x, int y)
{
	pixman_image_t *pix = swt.pix;
	pixman_color_t *bg;
	int winx = borderpx + x * win.cw, winy = borderpx + y * win.ch;

	bg = GETPIXMANCOLOR((g.mode & ATTR_REVERSE) ? g.fg : g.bg);

	pixman_image_fill_rectangles(
	    PIXMAN_OP_SRC, pix, bg, 1,
	    &(pixman_rectangle16_t){
		.x = winx, .y = winy, .width = win.cw, .height = win.ch});
}
#endif

#ifndef LIGATURES
void xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og)
#else
void xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og, Line line,
		 int len)
#endif
{
	unsigned int thicc  = cursorthickness;
	pixman_image_t *pix = swt.pix;
	pixman_color_t *drawcol;
	uint32_t tmp;
	int x = borderpx + cx * win.cw, y = borderpx + cy * win.ch;

	/* remove the old cursor */
	if (selected(ox, oy)) og.mode ^= ATTR_REVERSE;
#ifndef LIGATURES
	xdrawglyph(og, ox, oy);
#else
	xdrawline(line, 0, oy, len);
#endif

	if (IS_SET(MODE_HIDE)) return;

	/*
	 * Select the right color for the right mode.
	 */
	g.mode &=
	    ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE | ATTR_STRUCK | ATTR_WIDE;

	if (!selected(cx, cy)) {
		tmp  = g.fg;
		g.fg = g.bg;
		g.bg = tmp;
	}

	drawcol = GETPIXMANCOLOR(g.bg);

	/* draw the new one */
	if (IS_SET(MODE_FOCUSED)) {
		switch (win.cursor) {
		case 0: /* Blinking Block */
		case 1: /* Blinking Block (Default) */
		case 2: /* Steady Block */
#ifndef LIGATURES
			xdrawglyph(g, cx, cy);
#else
			xdrawglyphbg(g, cx, cy);
			xdrawglyph(g, cx, cy, NULL);
#endif
			break;
		case 3: /* Blinking Underline */
		case 4: /* Steady Underline */
			pixman_image_fill_rectangles(
			    PIXMAN_OP_SRC, pix, drawcol, 1,
			    &(pixman_rectangle16_t){x, y + win.ch - thicc,
						    win.cw, thicc});
			break;
		case 5: /* Blinking bar */
		case 6: /* Steady bar */
			pixman_image_fill_rectangles(
			    PIXMAN_OP_SRC, pix, drawcol, 1,
			    &(pixman_rectangle16_t){x, y, thicc, win.ch});
			break;
		}
	} else {
		pixman_image_fill_rectangles(PIXMAN_OP_SRC, pix, drawcol, 4,
					     (pixman_rectangle16_t[4]){
						 {x,              y,              win.cw, 1     },
						 {x,              y,              1,      win.ch},
						 {x,              y + win.ch - 1, win.cw, 1     },
						 {x + win.cw - 1, y,              1,      win.ch},
                });
	}
}

void xseticontitle(char *p) { warn("TODO: xseticontitle '%s'", p); }

void xsettitle(char *p)
{
	DEFAULT(p, opt_title);

	if (p[0] == '\0') p = opt_title;

	xdg_toplevel_set_title(xdg.toplevel, p);
}

int xstartdraw(void)
{
	int resized;
	DrwBuf *buf;

	if (!swt.need_draw || wl.callback) return 0;

	wl.callback = wl_surface_frame(wl.surface);
	wl_callback_add_listener(wl.callback, &listener.wl_callback, NULL);
	wl_surface_commit(wl.surface);

	buf = bufpool_getbuf(&swt.pool, wl.shm, win.w, win.h, &resized);
	if (!buf) {
		warn(errno ? "bufpool_getbuf:" : "no buffer available");
		return 0;
	}

	swt.pix = buf->pix;

	if (resized) xclear(0, 0, win.w, win.h);

	/* TODO: ensure window is visible */

	wl_surface_set_buffer_scale(wl.surface, wl.scale);
	wl_surface_attach(wl.surface, buf->wl_buf, 0, 0);

	swt.need_draw = false;
	return 1;
}

void xdrawline(Line line, int x1, int y1, int x2)
{
#ifndef LIGATURES
	Glyph g;
	for (; x1 < x2; x1++) {
		g = line[x1];
		if (g.mode == ATTR_WDUMMY) continue;
		xdrawglyph(g, x1, y1);
	}
#else
	struct fcft_font *f;
	struct fcft_text_run *run;
	Rune t[x2 - x1];
	uint i, len = 0, x = x1;
	ushort relevant = ATTR_BOLD | ATTR_ITALIC | ATTR_WDUMMY;

	for (; x1 < x2; x1++) {
		if (~line[x1].mode & ATTR_WDUMMY) t[len++] = line[x1].u;
		if (len != 0 &&
		    (x1 + 1 == x2 || ((line[x1 + 0].mode & relevant) !=
				      (line[x1 + 1].mode & relevant)))) {
			f   = dc.font[(!!(line[x1].mode & ATTR_BOLD)) +
                                    (!!(line[x1].mode & ATTR_ITALIC)) * 2];
			run = fcft_rasterize_text_run_utf32(f, len, t,
							    FCFT_SUBPIXEL_NONE);
			for (i = 0; i < len; i++)
				xdrawglyphbg(line[x + i], x + i, y1);
			for (i = 0; i < len; i++, x++)
				xdrawglyph(line[x], x, y1, run->glyphs[i]);
			fcft_text_run_destroy(run);
			len = 0;
		}
	}
#endif
}

void xfinishdraw(void)
{
	wl_surface_damage(wl.surface, 0, 0, win.w, win.h);
	wl_surface_commit(wl.surface);
}

void xximspot(int x, int y)
{
	(void)x;
	(void)y;
	/* TODO: probably nothing should happen, but im not sure */
}

void xsetpointermotion(int set)
{
	(void)set;
	/* TODO: probably nothing should happen, but im not sure */
}

void xsetmode(int set, unsigned int flags)
{
	int mode = win.mode;
	MODBIT(win.mode, set, flags);
	if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE)) redraw();
}

int xsetcursor(int cursor)
{
	if (!BETWEEN(cursor, 0, 6)) return 1;
	win.cursor = cursor;
	return 0;
}

void xseturgency(int add)
{
	(void)add;
	warn("TODO: xseturgency");
}

void xbell(void)
{
	if (!(IS_SET(MODE_FOCUSED))) xseturgency(1);
	warn("TODO: xbell");
}

int match(uint mask, uint state)
{
	return mask == XKB_ANY_MOD || mask == (state & ~ignoremod);
}

char *kmap(xkb_keysym_t k, uint state)
{
	const Key *kp;

	for (kp = key; kp < key + LEN(key); kp++) {
		if (kp->k != k) continue;

		if (!match(kp->mask, state)) continue;

		if (IS_SET(MODE_APPKEYPAD) ? kp->appkey < 0 : kp->appkey > 0)
			continue;
		if (IS_SET(MODE_NUMLOCK) && kp->appkey == 2) continue;

		if (IS_SET(MODE_APPCURSOR) ? kp->appcursor < 0
					   : kp->appcursor > 0)
			continue;

		return kp->s;
	}

	return NULL;
}

void kpress(uint32_t key, xkb_keysym_t keysym)
{
	int len;
	char buf[64], *customkey;
	Rune c;
	const Shortcut *bp;

	if (IS_SET(MODE_KBDLOCK)) return;

	/* 1. shortcuts */
	for (bp = shortcuts; bp < shortcuts + LEN(shortcuts); bp++) {
		if (keysym == bp->keysym && match(bp->mod, xkb.mods_mask)) {
			bp->func(&(bp->arg));
			return;
		}
	}

	/* 2. custom keys from config.h */
	if ((customkey = kmap(keysym, xkb.mods_mask))) {
		ttywrite(customkey, strlen(customkey), 1);
		return;
	}

	/* 3. composed string from input method */
	len = xkb_state_key_get_utf8(xkb.state, key, buf, sizeof(buf));
	if (len == 0) return;
	if (len == 1 && xkb.mods_mask & Mod1Mask) {
		if (IS_SET(MODE_8BIT)) {
			if (*buf < 0177) {
				c   = *buf | 0x80;
				len = utf8encode(c, buf);
			}
		} else {
			buf[1] = buf[0];
			buf[0] = '\033';
			len    = 2;
		}
	}
	ttywrite(buf, len, 1);
	swt.need_draw = true;
}

/* wayland functions {{{ */
void wl_callback_done(void *data, struct wl_callback *wl_callback,
		      uint32_t callback_data)
{
	(void)data;
	(void)callback_data;

	wl_callback_destroy(wl_callback);
	wl.callback = NULL;
	draw();
}

void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
		       uint32_t serial, struct wl_surface *surface,
		       struct wl_array *keys)
{
	(void)data;
	(void)wl_keyboard;
	(void)serial;
	(void)surface;
	(void)keys;

	win.mode |= MODE_FOCUSED;
	if (IS_SET(MODE_FOCUS)) ttywrite("\033[I", 3, 0);
}

void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
			uint32_t format, int32_t fd, uint32_t size)
{
	struct xkb_keymap *keymap;
	struct xkb_state *state;
	char *map;

	(void)data;
	(void)wl_keyboard;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) die("unknown keymap");

	map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) die("mmap:");

	keymap = xkb_keymap_new_from_string(xkb.context, map,
					    XKB_KEYMAP_FORMAT_TEXT_V1,
					    XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map, size);
	close(fd);

	if (!keymap) {
		warn("xkb_keymap_new_from_string:");
		return;
	}

	if (!(state = xkb_state_new(keymap))) {
		warn("xkb_state_new:");
		xkb_keymap_unref(keymap);
		return;
	}

	xkb_keymap_unref(xkb.keymap);
	xkb_state_unref(xkb.state);

	xkb.keymap = keymap;
	xkb.state  = state;
}

void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
		     uint32_t serial, uint32_t time, uint32_t key,
		     uint32_t state)
{
	xkb_keysym_t keysym;

	(void)data;
	(void)wl_keyboard;
	(void)serial;
	(void)time;

	if (!xkb.keymap || !xkb.state) return;

	key += 8; /* NOTE: I dont even know what this shit is */

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		if (key == xkb.repeat.key)
			timerfd_settime(swt.fd.repeat, 0,
					&(struct itimerspec){0}, NULL);
		return;
	}

	keysym = xkb_state_key_get_one_sym(xkb.state, key);
	if (keysym == XKB_KEY_NoSymbol) return;

	if (xkb_keymap_key_repeats(xkb.keymap, key)) {
		xkb.repeat.key    = key;
		xkb.repeat.keysym = keysym;
		timerfd_settime(swt.fd.repeat, 0, &xkb.repeat.ts, NULL);
	}

	kpress(key, keysym);
}

void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
		       uint32_t serial, struct wl_surface *surface)
{
	(void)data;
	(void)wl_keyboard;
	(void)serial;
	(void)surface;

	timerfd_settime(swt.fd.repeat, 0, &(struct itimerspec){0}, NULL);
	win.mode &= ~MODE_FOCUSED;
	if (IS_SET(MODE_FOCUS)) ttywrite("\033[O", 3, 0);
}

void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
			   uint32_t serial, uint32_t mods_depressed,
			   uint32_t mods_latched, uint32_t mods_locked,
			   uint32_t group)
{
	uint i;
	xkb_mod_index_t mod;
	struct {
		const char *xkb;
		uint mask;
	} mods[] = {
	    {XKB_MOD_NAME_SHIFT, ShiftMask  },
            {XKB_MOD_NAME_CAPS,  LockMask   },
	    {XKB_MOD_NAME_CTRL,  ControlMask},
            {XKB_MOD_NAME_MOD1,  Mod1Mask   },
	    {XKB_MOD_NAME_MOD2,  Mod2Mask   },
            {XKB_MOD_NAME_MOD3,  Mod3Mask   },
	    {XKB_MOD_NAME_MOD4,  Mod4Mask   },
            {XKB_MOD_NAME_MOD5,  Mod5Mask   },
	};

	(void)data;
	(void)wl_keyboard;
	(void)serial;

	if (!xkb.keymap || !xkb.state) return;

	xkb_state_update_mask(xkb.state, mods_depressed, mods_latched,
			      mods_locked, 0, 0, group);

	xkb.mods_mask = XKB_NO_MOD;
	for (i = 0; i < LEN(mods); i++) {
		mod = xkb_keymap_mod_get_index(xkb.keymap, mods[i].xkb);
		if (xkb_state_mod_index_is_active(
			xkb.state, mod, XKB_STATE_MODS_EFFECTIVE) == 1) {
			xkb.mods_mask |= mods[i].mask;
		}
	}
}

void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
			     int32_t rate, int32_t delay)
{
	/* FIXME: rate 0 should disable repeat and here it will divide by zero
	 */

	/* rate is in characters per second */
	struct timespec interval = {
	    .tv_sec  = rate == 1 ? 1 : 0,
	    .tv_nsec = rate == 1 ? 0 : 1000000000 / rate,
	};

	struct timespec value = {
	    .tv_sec  = delay / 1000,
	    .tv_nsec = (delay % 1000) * 1000000,
	};

	(void)data;
	(void)wl_keyboard;

	xkb.repeat.ts = (struct itimerspec){interval, value};
}

void wl_output_done(void *data, struct wl_output *wl_output)
{
	(void)data;
	(void)wl_output;
	xunloadfonts();
	xloadfonts(usedfont, defaultfontsize);
}

void wl_output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
	(void)data;
	(void)wl_output;
	wl.scale = factor;
}

void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
			  uint32_t time, uint32_t axis)
{
	(void)data;
	(void)time;
	if (!wl_pointer) return;
	wl.cursor.scroll[axis] = 0;
}

void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
		     uint32_t axis, wl_fixed_t value)
{
	int sign;

	(void)data;
	(void)time;

	if (!wl_pointer) return;

	/* TODO: axis 0 is vertical and only it is handled for now */
	if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;

	wl.cursor.scroll[axis] += wl_fixed_to_double(value);
	sign = 1 - 2 * (wl.cursor.scroll[axis] < 0);
	if (wl.cursor.scroll[axis] * sign < win.ch) return;

	wl.cursor.scroll[axis] -= sign * win.ch;
	bpress(sign == -1 ? Button4 : Button5);
}

void wl_pointer_button(void *data, struct wl_pointer *wl_pointer,
		       uint32_t serial, uint32_t time, uint32_t button,
		       uint32_t state)
{
	int btn;

	(void)data;
	(void)serial;
	(void)time;

	if (!wl_pointer) return;

	switch (button) {
	case BTN_LEFT:   btn = Button1; break;
	case BTN_MIDDLE: btn = Button2; break;
	case BTN_RIGHT:  btn = Button3; break;
	default:         warn("unknown button: %x", button); return;
	}

	switch (state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:  bpress(btn); break;
	case WL_POINTER_BUTTON_STATE_RELEASED: brelease(btn); break;
	default:                               warn("unknown button state: %d", state); break;
	}
}

void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
		      uint32_t serial, struct wl_surface *surface,
		      wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	char *env, *end;
	int size, frame;

	(void)data;
	(void)wl_pointer;
	if (!surface) return;
	wl.cursor.serial = serial;
	wl.cursor.x      = wl_fixed_to_int(surface_x);
	wl.cursor.y      = wl_fixed_to_int(surface_y);

	/* FIXME: probably if scale changes, the theme needs to be destroyed */
	if (wl.cursor.theme) goto set;

	env  = getenv("XCURSOR_SIZE");
	size = env ? (int)strtol(env, &end, 10) : 24;
	env  = getenv("XCURSOR_THEME");

	wl.cursor.theme = wl_cursor_theme_load(env, size, wl.shm);
	if (!wl.cursor.theme) {
		warn("failed to load xcursor theme");
		return;
	}

	wl.cursor.surface = wl_compositor_create_surface(wl.compositor);
	if (!wl.cursor.surface) {
		wl_cursor_theme_destroy(wl.cursor.theme);
		warn("failed to create cursor surface");
		return;
	}

	wl.cursor.cursor = wl_cursor_theme_get_cursor(wl.cursor.theme, "xterm");

set:
	if (!wl.cursor.cursor) return;
	frame = wl_cursor_frame(wl.cursor.cursor, 0);

	struct wl_buffer *buffer;
	struct wl_cursor_image *image;

	image  = wl.cursor.cursor->images[frame];
	buffer = wl_cursor_image_get_buffer(image);
	wl_surface_attach(wl.cursor.surface, buffer, 0, 0);
	wl_surface_damage(wl.cursor.surface, 0, 0, image->width, image->height);
	wl_surface_commit(wl.cursor.surface);
	wl_pointer_set_cursor(wl.pointer, wl.cursor.serial, wl.cursor.surface,
			      image->hotspot_x, image->hotspot_y);
}

void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
		      uint32_t serial, struct wl_surface *surface)
{
	(void)data;
	(void)wl_pointer;
	(void)serial;
	(void)surface;
	if (wl.cursor.callback) {
		wl_callback_destroy(wl.cursor.callback);
		wl.cursor.callback = NULL;
	}
	wl.cursor.x = 0;
	wl.cursor.y = 0;
	xkb.mods_mask &= ~(Button1Mask | Button2Mask | Button3Mask |
			   Button4Mask | Button5Mask);
}

void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
		       wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	(void)data;
	(void)time;
	if (!wl_pointer) return;
	wl.cursor.x = wl_fixed_to_int(surface_x) * wl.scale;
	wl.cursor.y = wl_fixed_to_int(surface_y) * wl.scale;

	bmotion();
}

void noop() {}

void wl_registry_global(void *data, struct wl_registry *wl_registry,
			uint32_t name, const char *interface, uint32_t version)
{
	if (!strcmp(interface, wl_compositor_interface.name)) {
		wl.compositor = wl_registry_bind(
		    wl_registry, name, &wl_compositor_interface, version);
	} else if (!strcmp(interface, wl_shm_interface.name)) {
		wl.shm = wl_registry_bind(wl_registry, name, &wl_shm_interface,
					  version);
		wl_shm_add_listener(wl.shm, &listener.wl_shm, data);
	} else if (!strcmp(interface, wl_seat_interface.name)) {
		wl.seat = wl_registry_bind(wl_registry, name,
					   &wl_seat_interface, version);
		wl_seat_add_listener(wl.seat, &listener.wl_seat, NULL);
	} else if (!strcmp(interface, wl_output_interface.name)) {
		wl.scale  = 1;
		wl.output = wl_registry_bind(wl_registry, name,
					     &wl_output_interface, version);
		wl_output_add_listener(wl.output, &listener.wl_output, NULL);
	} else if (!strcmp(interface, "xdg_wm_base")) {
		xdg.wm_base = wl_registry_bind(wl_registry, name,
					       &xdg_wm_base_interface, version);
		xdg_wm_base_add_listener(xdg.wm_base, &listener.xdg_wm_base,
					 NULL);
	}
}

void wl_seat_capabilities(void *data, struct wl_seat *wl_seat,
			  uint32_t capabilities)
{
	(void)data;

	if (!xkb.context)
		if (!(xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS)))
			die("xkb_context_new:");

	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD && !wl.keyboard) {
		wl.keyboard = wl_seat_get_keyboard(wl_seat);
		wl_keyboard_add_listener(wl.keyboard, &listener.wl_keyboard,
					 NULL);
	} else if (~capabilities & WL_SEAT_CAPABILITY_KEYBOARD && wl.keyboard) {
		wl_keyboard_release(wl.keyboard);
		wl.keyboard = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !wl.pointer) {
		wl.pointer = wl_seat_get_pointer(wl.seat);
		wl_pointer_add_listener(wl.pointer, &listener.wl_pointer, NULL);
	} else if (~capabilities & WL_SEAT_CAPABILITY_POINTER && wl.pointer) {
		wl_pointer_release(wl.pointer);
		wl.pointer = NULL;
	}
}

void wl_shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	(void)wl_shm;
	if (format == WL_SHM_FORMAT_ARGB8888) *(bool *)data = true;
}

void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
			   uint32_t serial)
{
	(void)data;

	xdg_surface_ack_configure(xdg_surface, serial);
}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	(void)data;
	(void)xdg_toplevel;
	swt.running = false;
}

void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
			    int32_t width, int32_t height,
			    struct wl_array *states)
{
	(void)data;
	(void)xdg_toplevel;
	(void)states;

	swt.need_draw = false;

	width *= wl.scale;
	height *= wl.scale;

	if (width == win.w && height == win.h) return;

	cresize(width, height);

	swt.need_draw = true;
}

void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
		      uint32_t serial)
{
	(void)data;
	xdg_wm_base_pong(xdg_wm_base, serial);
}
/* }}} */

void xinit(int cols, int rows)
{
	/* font */
	usedfont = (opt_font == NULL) ? (char *)font : opt_font;
	/* TODO: remove fontsize from config */
	defaultfontsize = fontsize;
	xloadfonts(font, fontsize);

	/* colors */
	xloadcols();

	/* adjust fixed window geometry */
	win.w = 2 * borderpx + cols * win.cw;
	win.h = 2 * borderpx + rows * win.ch;
}

void setup(void)
{
	sigset_t mask;
	bool argb;

	wl.display = wl_display_connect(NULL);
	if (!wl.display) die("wl_display_connect:");

	wl.registry = wl_display_get_registry(wl.display);
	if (!wl.registry) die("wl_display_get_registry:");
	wl_registry_add_listener(wl.registry, &listener.wl_registry, &argb);
	wl_display_roundtrip(wl.display);
	wl_display_roundtrip(wl.display);

	if (!wl.compositor) die("no wayland compositor registered");
	if (!wl.shm) die("no wayland shm registered");
	if (!xdg.wm_base) die("no xdg wm base registered");
	if (!argb) die("ARGB format is not supported");

	wl.surface = wl_compositor_create_surface(wl.compositor);
	if (!wl.surface) die("wl_compositor_create_surface:");

	xdg.surface = xdg_wm_base_get_xdg_surface(xdg.wm_base, wl.surface);
	if (!xdg.surface) die("xdg_wm_base_get_xdg_surface:");
	xdg_surface_add_listener(xdg.surface, &listener.xdg_surface, NULL);

	xdg.toplevel = xdg_surface_get_toplevel(xdg.surface);
	if (!xdg.toplevel) die("xdg_surface_get_toplevel:");

	xdg_toplevel_add_listener(xdg.toplevel, &listener.xdg_toplevel, NULL);
	xdg_toplevel_set_title(xdg.toplevel, opt_title);
	xdg_toplevel_set_app_id(xdg.toplevel, app_id);

	wl_surface_commit(wl.surface);
	wl_display_roundtrip(wl.display);

	if (sigemptyset(&mask) < 0) die("sigemptyset:");
	if (sigaddset(&mask, SIGINT)) die("sigaddset:");
	if (sigaddset(&mask, SIGTERM)) die("sigaddset:");
	if (sigprocmask(SIG_BLOCK, &mask, NULL)) die("sigprocmask:");

	swt.fd.display = wl_display_get_fd(wl.display);
	swt.fd.pty     = ttynew(opt_line, (char *)shell, opt_io, opt_cmd);
	swt.fd.signal  = signalfd(-1, &mask, 0);
	swt.fd.repeat  = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

	if (swt.fd.display < 0) die("wl_display_get_fd:");
	if (swt.fd.signal < 0) die("signalfd:");
	if (swt.fd.repeat < 0) die("timerfd:");
}

void run(void)
{
	ssize_t n;
	size_t i;
	uint64_t r;
	struct signalfd_siginfo si;
	struct pollfd pfds[] = {
	    /* fd            events  revents */
	    {swt.fd.display, POLLIN, 0},
	    {swt.fd.pty,     POLLIN, 0},
	    {swt.fd.repeat,  POLLIN, 0},
	    {swt.fd.signal,  POLLIN, 0},
	};
	bool drawing   = false;
	double timeout = -1;
	struct timespec now, trigger, lastblink = {0};

	swt.running = true;
	while (swt.running) {
		do {
			n = poll(pfds, LEN(pfds), timeout);
			if (n < 0 && errno != EAGAIN && errno != EINTR)
				die("poll:");
		} while (n < 0);
		clock_gettime(CLOCK_MONOTONIC, &now);

		for (i = 0; i < LEN(pfds); i++)
			if (pfds[i].revents & (POLLNVAL | POLLERR)) return;

		if (pfds[0].revents & POLLIN) {
			if (wl_display_dispatch(wl.display) < 0) return;
		}
		if (pfds[1].revents & POLLIN) {
			swt.need_draw = true;
			ttyread();
		}
		if (pfds[2].revents & POLLIN) {
			if (read(swt.fd.repeat, &r, sizeof(r)) >= 0)
				kpress(xkb.repeat.key, xkb.repeat.keysym);
		}
		if (pfds[3].revents & POLLIN) {
			n = read(swt.fd.signal, &si, sizeof(si));
			if (n != sizeof(si)) die("signalfd/read:");
			if (si.ssi_signo == SIGINT || si.ssi_signo == SIGTERM)
				return;
		}

		/*
		 * To reduce flicker and tearing, when new content or event
		 * triggers drawing, we first wait a bit to ensure we got
		 * everything, and if nothing new arrives - we draw.
		 * We start with trying to wait minlatency ms. If more content
		 * arrives sooner, we retry with shorter and shorter periods,
		 * and eventually draw even without idle after maxlatency ms.
		 * Typically this results in low latency while interacting,
		 * maximum latency intervals during `cat huge.txt`, and perfect
		 * sync with periodic updates from animations/key-repeats/etc.
		 */
		if (n > 0) {
			if (!drawing) {
				trigger = now;
				drawing = true;
			}
			timeout = (maxlatency - TIMEDIFF(now, trigger)) /
				  maxlatency * minlatency;
			if (timeout > 0)
				continue; /* we have time, try to find idle */
		}

		/* idle detected or maxlatency exhausted -> draw */
		timeout = -1;
		if (blinktimeout && tattrset(ATTR_BLINK)) {
			timeout = blinktimeout - TIMEDIFF(now, lastblink);
			if (timeout <= 0) {
				swt.need_draw = true;
				if (-timeout > blinktimeout) /* start visible */
					win.mode |= MODE_BLINK;
				win.mode ^= MODE_BLINK;
				tsetdirtattr(ATTR_BLINK);
				lastblink = now;
				timeout   = blinktimeout;
			}
		}

		draw();
		wl_display_flush(wl.display);
		drawing = false;
	}
}

void usage(void)
{
	die("usage: %s [-aiv] [-c class] [-f font] [-g geometry]"
	    " [-n name] [-o file]\n"
	    "          [-T title] [-t title] [-w windowid]"
	    " [[-e] command [args ...]]\n"
	    "       %s [-aiv] [-c class] [-f font] [-g geometry]"
	    " [-n name] [-o file]\n"
	    "          [-T title] [-t title] [-w windowid] -l line"
	    " [stty_args ...]",
	    argv0, argv0);
}

void cleanup(void)
{
#define s(f, o)                                                                \
	do {                                                                   \
		if (o != NULL) f(o);                                           \
	} while (0)

	close(swt.fd.signal);
	close(swt.fd.repeat);

	tfree();
	s(free, dc.col);
	xunloadfonts();
	fcft_fini();
	bufpool_cleanup(&swt.pool);

	s(xkb_keymap_unref, xkb.keymap);
	s(xkb_state_unref, xkb.state);
	s(xkb_context_unref, xkb.context);

	s(xdg_wm_base_destroy, xdg.wm_base);
	s(xdg_toplevel_destroy, xdg.toplevel);
	s(xdg_surface_destroy, xdg.surface);

	s(wl_callback_destroy, wl.callback);
	s(wl_surface_destroy, wl.cursor.surface);
	s(wl_cursor_theme_destroy, wl.cursor.theme);
	s(wl_callback_destroy, wl.cursor.callback);
	s(wl_pointer_release, wl.pointer);
	s(wl_output_release, wl.output);
	s(wl_keyboard_release, wl.keyboard);
	s(wl_seat_release, wl.seat);
	s(wl_shm_release, wl.shm);
	s(wl_compositor_destroy, wl.compositor);
	s(wl_surface_destroy, wl.surface);
	s(wl_registry_destroy, wl.registry);
	s(wl_display_disconnect, wl.display);
}

int main(int argc, char *argv[])
{
	xsetcursor(cursorshape);

	ARGBEGIN
	{
	case 'a': allowaltscreen = 0; break;
	case 'c': opt_class = EARGF(usage()); break;
	case 'e':
		if (argc > 0) --argc, ++argv;
		goto run;
	case 'f': opt_font = EARGF(usage()); break;
	case 'g': warn("TODO: geometry"); break;
	case 'i': warn("TODO: fixed"); break;
	case 'o': opt_io = EARGF(usage()); break;
	case 'l': opt_line = EARGF(usage()); break;
	case 'n': opt_name = EARGF(usage()); break;
	case 't':
	case 'T': opt_title = EARGF(usage()); break;
	case 'w': opt_embed = EARGF(usage()); break;
	case 'v': die("%s " VERSION, argv0); break;
	default:  usage();
	}
	ARGEND;

run:
	if (argc > 0) /* eat all remaining arguments */
		opt_cmd = argv;

	if (!opt_title) opt_title = (opt_line || !opt_cmd) ? "swt" : opt_cmd[0];

	setlocale(LC_CTYPE, "");
	cols = MAX(cols, 1);
	rows = MAX(rows, 1);
	tnew(cols, rows);
	xinit(cols, rows);
	setup();
	run();
	cleanup();

	return 0;
}
