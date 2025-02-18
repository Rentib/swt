#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/wait.h>

#include <fcft/fcft.h>
#include <pixman-1/pixman.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-protocol.h"

#include "st.h"
#include "util.h"
#include "win.h"

#define IS_SET(flag) ((win.mode & (flag)) != 0)
#define TRUERED(x)   (((x) & 0xff0000) >> 8)
#define TRUEGREEN(x) (((x) & 0xff00))
#define TRUEBLUE(x)  (((x) & 0xff) << 8)

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

struct buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	pixman_image_t *pix;
	ssize_t size;
	bool busy;
};

struct swt_window {
	int tw, th; /* tty width and height */
	int w, h;   /* window width and height */
	int ch;     /* char height */
	int cw;     /* char width  */
	int mode;   /* window state/mode flags */
	int cursor; /* cursor style */
};

struct swt_wl {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_surface *surface;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_pointer *pointer;
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
	struct xkb_compose_table *compose_table;
	struct xkb_compose_state *compose_state;
	uint mods_mask;
};

struct swt {
	pid_t pid;

	struct {
		int display;
		int signal;
		int pty;
		int repeat;
	} fd;

	struct buffer buf;

	struct {
		bool running  : 1;
		bool can_draw : 1;
	} flags;

	struct {
		uint32_t key;
		xkb_keysym_t keysym;
		struct itimerspec ts;
	} repeat;

	struct {
		wl_fixed_t x;
		wl_fixed_t y;

		struct wl_cursor_theme *theme;
		struct wl_cursor *text;
		struct wl_cursor *current;
		struct wl_surface *surface;
		struct wl_callback *callback;
		long long anim_start;
		uint32_t enter_serial;
	} ptr;
};

struct swt_dc {
	pixman_color_t *col;
	size_t collen;
	struct fcft_font *fonts[4];
};

typedef struct {
	xkb_keysym_t k;
	uint mask;
	char *s;
	/* three-valued logic variables: 0 indifferent, 1 on, -1 off */
	signed char appkey;    /* application keypad */
	signed char appcursor; /* application cursor */
} Key;

static void buffer_init(struct buffer *buf);
static void buffer_release(void *data, struct wl_buffer *wl_buffer);
static void buffer_unmap(struct buffer *buf);

static void cleanup_font(void);
static void cleanup(void);

static long long cursor_now(void);
static void cursor_draw(int frame);
static void cursor_frame_callback(void *data, struct wl_callback *cb,
				  uint32_t time);
static void cursor_request_frame_callback(void);
static void cursor_unset(void);
static void cursor_set(struct wl_cursor *cursor);
static void cursor_init(void);
static void cursor_free(void);

static void frame_done(void *data, struct wl_callback *wl_callback,
		       uint32_t callback_data);

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
			   uint32_t serial, struct wl_surface *surface,
			   struct wl_array *keys);
static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
			    uint32_t format, int32_t fd, uint32_t size);
static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
			 uint32_t serial, uint32_t time, uint32_t key,
			 uint32_t state);
static void keyboard_keypress(uint32_t key, xkb_keysym_t keysym);
static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
			   uint32_t serial, struct wl_surface *surface);
static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
			       uint32_t serial, uint32_t mods_depressed,
			       uint32_t mods_latched, uint32_t mods_locked,
			       uint32_t group);
static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
				 int32_t rate, int32_t delay);

static void pointer_enter(void *data, struct wl_pointer *wl_pointer,
			  uint32_t serial, struct wl_surface *surface,
			  wl_fixed_t surface_x, wl_fixed_t surface_y);
static void pointer_leave(void *data, struct wl_pointer *wl_pointer,
			  uint32_t serial, struct wl_surface *surface);
static void pointer_motion(void *data, struct wl_pointer *wl_pointer,
			   uint32_t time, wl_fixed_t surface_x,
			   wl_fixed_t surface_y);
static void pointer_button(void *data, struct wl_pointer *wl_pointer,
			   uint32_t serial, uint32_t time, uint32_t button,
			   uint32_t state);
static void pointer_axis(void *data, struct wl_pointer *wl_pointer,
			 uint32_t time, uint32_t axis, wl_fixed_t value);

static void noop();

static void registry_global(void *data, struct wl_registry *wl_registry,
			    uint32_t name, const char *interface,
			    uint32_t version);

static void repeat_cancel(void);

static void run(void);

static void seat_capabilities(void *data, struct wl_seat *wl_seat,
			      uint32_t capabilities);

static void setup_font(void);
static void setup(void);

static void shm_format(void *data, struct wl_shm *wl_shm, uint32_t format);

static void surface_configure(void *data, struct xdg_surface *xdg_surface,
			      uint32_t serial);

static void toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel);
static void toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
			       int32_t width, int32_t height,
			       struct wl_array *states);

static void wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
			 uint32_t serial);

/* globals */
static struct swt swt;
static struct swt_dc dc;
static struct swt_window win;
static struct swt_wl wl;
static struct swt_xdg xdg;
static struct swt_xkb xkb;

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static const struct wl_keyboard_listener keyboard_listener = {
    .enter       = keyboard_enter,
    .key         = keyboard_key,
    .keymap      = keyboard_keymap,
    .leave       = keyboard_leave,
    .modifiers   = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

static const struct wl_pointer_listener pointer_listener = {
    .enter                   = pointer_enter,
    .leave                   = pointer_leave,
    .motion                  = pointer_motion,
    .button                  = pointer_button,
    .axis                    = pointer_axis,
    .frame                   = noop,
    .axis_source             = noop,
    .axis_stop               = noop,
    .axis_discrete           = noop,
    .axis_value120           = noop,
    .axis_relative_direction = noop,
};

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name         = noop,
};

static const struct wl_shm_listener shm_listener = {
    .format = shm_format,
};

static const struct xdg_surface_listener surface_listener = {
    .configure = surface_configure,
};

static const struct xdg_toplevel_listener toplevel_listener = {
    .close            = toplevel_close,
    .configure_bounds = noop,
    .configure        = toplevel_configure,
    .wm_capabilities  = noop,
};

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

static const struct wl_callback_listener cursor_frame_listener = {
    .done = cursor_frame_callback,
};

static struct wl_callback_listener frame_listener = {
    .done = frame_done,
};

static struct wl_registry_listener registry_listener = {
    .global        = registry_global,
    .global_remove = noop,
};

#include "config.h"

void buffer_init(struct buffer *buf)
{
	struct wl_shm_pool *pool;
	char shm_name[14];
	int fd, stride;
	int max_tries = 100;

	stride = win.w * sizeof(uint32_t);
	if (buf->size == stride * win.h) return;

	buffer_unmap(buf);
	buf->size = stride * win.h;

	srand(time(NULL));
	do {
		sprintf(shm_name, "/swt-%d", rand() % 1000000);
		fd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	} while (fd < 0 && errno == EEXIST && --max_tries);

	if (fd < 0) die("shm_open:");
	if (shm_unlink(shm_name) < 0) die("shm_unlink:");
	if (ftruncate(fd, buf->size) < 0) die("ftruncate:");
	if ((buf->data = mmap(NULL, buf->size, PROT_READ | PROT_WRITE,
			      MAP_SHARED, fd, 0)) == MAP_FAILED)
		die("mmap:");

	pool           = wl_shm_create_pool(wl.shm, fd, buf->size);
	buf->wl_buffer = wl_shm_pool_create_buffer(
	    pool, 0, win.w, win.h, stride, WL_SHM_FORMAT_ARGB8888);
	wl_buffer_add_listener(buf->wl_buffer, &buffer_listener, buf);
	wl_shm_pool_destroy(pool);
	if (close(fd) < 0) die("close:");

	buf->pix = pixman_image_create_bits(PIXMAN_a8r8g8b8, win.w, win.h,
					    buf->data, stride);
	if (!buf->pix) die("pixman_image_create_bits:");

	/* NOTE: st does some weird shit like "intelligent cleaning up of the
	 * borders", but idk what for */
	pixman_image_fill_boxes(PIXMAN_OP_SRC, buf->pix, &dc.col[defaultbg], 1,
				&(pixman_box32_t){0, 0, win.w, win.h});
}

void buffer_release(void *data, struct wl_buffer *wl_buffer)
{
	(void)wl_buffer;
	((struct buffer *)data)->busy = false;
}

void buffer_unmap(struct buffer *buf)
{
	if (!buf->wl_buffer) return;
	wl_buffer_destroy(buf->wl_buffer);
	if (!pixman_image_unref(buf->pix)) die("pixman_image_unref:");
	if (munmap(buf->data, buf->size) < 0) die("munmap:");
}

void cleanup_font(void)
{
	unsigned i;
	for (i = 0; i < 4; i++) {
		if (dc.fonts[i]) fcft_destroy(dc.fonts[i]);
	}
}

void cleanup(void)
{
	buffer_unmap(&swt.buf);

	if (wl.callback) wl_callback_destroy(wl.callback);

	xdg_toplevel_destroy(xdg.toplevel);
	xdg_surface_destroy(xdg.surface);
	xdg_wm_base_destroy(xdg.wm_base);

	wl_keyboard_destroy(wl.keyboard);
	wl_surface_destroy(wl.surface);
	wl_compositor_destroy(wl.compositor);
	wl_shm_destroy(wl.shm);
	wl_seat_destroy(wl.seat);
	wl_registry_destroy(wl.registry);
	wl_display_flush(wl.display);
	wl_display_disconnect(wl.display);

	cursor_free();

	xkb_context_unref(xkb.context);
	xkb_compose_table_unref(xkb.compose_table);
	xkb_compose_state_unref(xkb.compose_state);
	xkb_keymap_unref(xkb.keymap);
	xkb_state_unref(xkb.state);

	/* NOTE: swt.fd.display closes automatically */
	if (close(swt.fd.signal) < 0) die("close:");
	if (close(swt.fd.pty) < 0) die("close:");
	if (close(swt.fd.repeat) < 0) die("close:");

	free(dc.col);
	cleanup_font();
	fcft_fini();
}

/* {{{ cursor stolen from havoc */
long long cursor_now(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void cursor_draw(int frame)
{
	struct wl_buffer *buffer;
	struct wl_cursor_image *image;

	if ((int)swt.ptr.current->image_count <= frame) {
		fprintf(stderr, "cursor frame index out of range\n");
		return;
	}

	image  = swt.ptr.current->images[frame];
	buffer = wl_cursor_image_get_buffer(image);
	wl_surface_attach(swt.ptr.surface, buffer, 0, 0);
	wl_surface_damage(swt.ptr.surface, 0, 0, image->width, image->height);
	wl_surface_commit(swt.ptr.surface);
	wl_pointer_set_cursor(wl.pointer, swt.ptr.enter_serial, swt.ptr.surface,
			      image->hotspot_x, image->hotspot_y);
}

void cursor_frame_callback(void *data, struct wl_callback *cb, uint32_t time)
{
	(void)data;
	(void)time;
	int frame =
	    wl_cursor_frame(swt.ptr.current, cursor_now() - swt.ptr.anim_start);

	if (cb != swt.ptr.callback) die("wrong callback");
	wl_callback_destroy(swt.ptr.callback);
	cursor_request_frame_callback();
	cursor_draw(frame);
}

void cursor_request_frame_callback(void)
{
	swt.ptr.callback = wl_surface_frame(swt.ptr.surface);
	wl_callback_add_listener(swt.ptr.callback, &cursor_frame_listener,
				 NULL);
}

void cursor_unset(void)
{
	if (swt.ptr.callback) {
		wl_callback_destroy(swt.ptr.callback);
		swt.ptr.callback = NULL;
	}
	swt.ptr.current = NULL;
}

void cursor_set(struct wl_cursor *cursor)
{
	uint32_t duration;
	int frame;

	cursor_unset();

	if (wl.pointer == NULL) return;

	if (cursor == NULL) goto hide;

	swt.ptr.current = cursor;

	frame = wl_cursor_frame_and_duration(swt.ptr.current, 0, &duration);
	if (duration) {
		swt.ptr.anim_start = cursor_now();
		cursor_request_frame_callback();
	}
	cursor_draw(frame);

	return;
hide:
	wl_pointer_set_cursor(wl.pointer, swt.ptr.enter_serial, NULL, 0, 0);
}

void cursor_init(void)
{
	int size               = 32;
	char *size_str         = getenv("XCURSOR_SIZE");
	struct wl_cursor *text = NULL;

	if (size_str && *size_str) {
		char *end;
		long s;

		errno = 0;
		s     = strtol(size_str, &end, 10);
		if (errno == 0 && *end == '\0' && s > 0) size = s;
	}

	swt.ptr.theme =
	    wl_cursor_theme_load(getenv("XCURSOR_THEME"), size, wl.shm);
	if (swt.ptr.theme == NULL) return;

	text = wl_cursor_theme_get_cursor(swt.ptr.theme, "text");
	if (text == NULL)
		text = wl_cursor_theme_get_cursor(swt.ptr.theme, "ibeam");
	if (text == NULL)
		text = wl_cursor_theme_get_cursor(swt.ptr.theme, "xterm");

	swt.ptr.surface = wl_compositor_create_surface(wl.compositor);
	if (swt.ptr.surface == NULL) {
		wl_cursor_theme_destroy(swt.ptr.theme);
		swt.ptr.theme = NULL;
		return;
	}

	swt.ptr.text = text;
}

void cursor_free(void)
{
	if (swt.ptr.callback) wl_callback_destroy(swt.ptr.callback);
	if (swt.ptr.surface) wl_surface_destroy(swt.ptr.surface);
	if (swt.ptr.theme) wl_cursor_theme_destroy(swt.ptr.theme);
}
/* }}} */

void frame_done(void *data, struct wl_callback *wl_callback,
		uint32_t callback_data)
{
	(void)data;
	(void)callback_data;

	wl_callback_destroy(wl_callback);
	wl.callback        = NULL;
	swt.flags.can_draw = true;
}

void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
		    uint32_t serial, struct wl_surface *surface,
		    struct wl_array *keys)
{
	(void)data;
	(void)wl_keyboard;
	(void)serial;
	(void)surface;
	(void)keys;

	win.mode |= MODE_FOCUSED;
	ttywrite("\033[I", 3, 0);
}

void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
		     uint32_t format, int32_t fd, uint32_t size)
{
	struct xkb_keymap *keymap;
	struct xkb_state *state;
	struct xkb_compose_table *compose_table;
	struct xkb_compose_state *compose_state;
	char *map, *lang;

	(void)data;
	(void)wl_keyboard;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) {
		close(fd);
		return;
	}

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

	lang = getenv("LANG");
	if (lang == NULL) return;

	compose_table = xkb_compose_table_new_from_locale(
	    xkb.context, lang, XKB_COMPOSE_COMPILE_NO_FLAGS);
	if (!compose_table) die("xkb_compose_table_new_from_locale:");

	compose_state =
	    xkb_compose_state_new(compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
	if (!compose_state) die("xkb_compose_state_new:");

	xkb_compose_table_unref(xkb.compose_table);
	xkb_compose_state_unref(xkb.compose_state);
	xkb.compose_table = compose_table;
	xkb.compose_state = compose_state;
}

void keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
		  uint32_t time, uint32_t key, uint32_t state)
{
	xkb_keysym_t keysym;

	(void)data;
	(void)wl_keyboard;
	(void)serial;
	(void)time;

	if (!xkb.keymap || !xkb.state) return;

	key += 8; /* NOTE: I dont even know what this shit is */

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		if (key == swt.repeat.key) repeat_cancel();
		return;
	}

	keysym = xkb_state_key_get_one_sym(xkb.state, key);
	keysym = xkb_keysym_to_lower(keysym);
	if (xkb.compose_state &&
	    xkb_compose_state_feed(xkb.compose_state, keysym) ==
		XKB_COMPOSE_FEED_ACCEPTED) {
		switch (xkb_compose_state_get_status(xkb.compose_state)) {
		case XKB_COMPOSE_COMPOSED:
			keysym =
			    xkb_compose_state_get_one_sym(xkb.compose_state);
			break;
		case XKB_COMPOSE_COMPOSING:
		case XKB_COMPOSE_CANCELLED: keysym = XKB_KEY_NoSymbol; break;
		case XKB_COMPOSE_NOTHING:   break;
		}
	}
	if (keysym == XKB_KEY_NoSymbol) return;

	if (xkb_keymap_key_repeats(xkb.keymap, key)) {
		swt.repeat.key    = key;
		swt.repeat.keysym = keysym;
		timerfd_settime(swt.fd.repeat, 0, &swt.repeat.ts, NULL);
	}

	keyboard_keypress(key, keysym);
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

void keyboard_keypress(uint32_t key, xkb_keysym_t keysym)
{
	int len;
	char buf[64], *customkey;
	Rune c;

	/* 1. TODO: user defined shortcuts */

	/* 2. custom keys from config.h */
	if ((customkey = kmap(keysym, xkb.mods_mask))) {
		ttywrite(customkey, strlen(customkey), 1);
		return;
	}

	/* 3. composed string from input method */
	len = xkb_state_key_get_utf8(xkb.state, key, buf, sizeof(buf));
	if (!len) return;
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
}

void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
		    uint32_t serial, struct wl_surface *surface)
{
	(void)data;
	(void)wl_keyboard;
	(void)serial;
	(void)surface;

	repeat_cancel();
	win.mode &= ~MODE_FOCUSED;
	ttywrite("\033[O", 3, 0);
}

void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
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

	repeat_cancel();

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

void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
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

	swt.repeat.ts = (struct itimerspec){interval, value};
}

void pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
		   struct wl_surface *surface, wl_fixed_t surface_x,
		   wl_fixed_t surface_y)
{
	(void)data;
	(void)wl_pointer;
	(void)surface;

	swt.ptr.x = surface_x;
	swt.ptr.y = surface_y;

	swt.ptr.enter_serial = serial;
	cursor_set(swt.ptr.text);
}

void pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
		   struct wl_surface *surface)
{
	(void)data;
	(void)wl_pointer;
	(void)serial;
	(void)surface;
	cursor_unset();
}

void pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
		    wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	(void)data;
	(void)wl_pointer;
	(void)time;

	swt.ptr.x = surface_x;
	swt.ptr.y = surface_y;

	/* TODO: selection */

	if (swt.ptr.current == NULL && swt.ptr.text) cursor_set(swt.ptr.text);
}

void pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
		    uint32_t time, uint32_t button, uint32_t state)
{
	(void)data;
	(void)wl_pointer;
	(void)serial;
	(void)time;
	(void)button;
	(void)state;

	/* TODO: selection */

	if (swt.ptr.current == NULL && swt.ptr.text) cursor_set(swt.ptr.text);
}

void pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
		  uint32_t axis, wl_fixed_t value)
{
	(void)data;
	(void)wl_pointer;
	(void)time;
	(void)axis;
	(void)value;

	/* TODO: send csi */
}

void noop() {}

void registry_global(void *data, struct wl_registry *wl_registry, uint32_t name,
		     const char *interface, uint32_t version)
{
	if (!strcmp(interface, "wl_compositor")) {
		wl.compositor = wl_registry_bind(
		    wl_registry, name, &wl_compositor_interface, version);
	} else if (!strcmp(interface, "wl_shm")) {
		wl.shm = wl_registry_bind(wl_registry, name, &wl_shm_interface,
					  version);
		wl_shm_add_listener(wl.shm, &shm_listener, data);
	} else if (!strcmp(interface, "xdg_wm_base")) {
		xdg.wm_base = wl_registry_bind(wl_registry, name,
					       &xdg_wm_base_interface, version);
		xdg_wm_base_add_listener(xdg.wm_base, &wm_base_listener, NULL);
	} else if (!strcmp(interface, "wl_seat")) {
		wl.seat = wl_registry_bind(wl_registry, name,
					   &wl_seat_interface, version);
		wl_seat_add_listener(wl.seat, &seat_listener, NULL);
	} else {
		/* TODO: maybe register more stuff */
	}
}

void repeat_cancel(void)
{
	timerfd_settime(swt.fd.repeat, 0, &(struct itimerspec){0}, NULL);
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

	swt.flags.running = true;
	while (swt.flags.running) {
		do {
			n = poll(pfds, LEN(pfds), -1);
			if (n < 0 && errno != EAGAIN && errno != EINTR)
				die("poll:");
		} while (n <= 0);

		for (i = 0; i < LEN(pfds); i++)
			if (pfds[i].revents & (POLLNVAL | POLLERR)) return;

		if (pfds[0].revents & POLLIN) {
			if (wl_display_dispatch(wl.display) < 0) return;
			/* FIXME: this triggers infinitely */
		}
		if (pfds[1].revents & POLLIN) ttyread();
		if (pfds[2].revents & POLLIN) {
			if (read(swt.fd.repeat, &r, sizeof(r)) >= 0)
				keyboard_keypress(swt.repeat.key,
						  swt.repeat.keysym);
		}
		if (pfds[3].revents & POLLIN) {
			n = read(swt.fd.signal, &si, sizeof(si));
			if (n != sizeof(si)) die("signalfd/read:");
			if (si.ssi_signo == SIGINT || si.ssi_signo == SIGTERM)
				return;
		}

		draw();
		wl_display_flush(wl.display);
	}
}

void seat_capabilities(void *data, struct wl_seat *wl_seat,
		       uint32_t capabilities)
{
	(void)data;

	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD && !wl.keyboard) {
		wl.keyboard = wl_seat_get_keyboard(wl_seat);
		wl_keyboard_add_listener(wl.keyboard, &keyboard_listener, NULL);
	} else if (~capabilities & WL_SEAT_CAPABILITY_KEYBOARD && wl.keyboard) {
		wl_keyboard_release(wl.keyboard);
		wl.keyboard = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !wl.pointer) {
		wl.pointer = wl_seat_get_pointer(wl.seat);
		wl_pointer_add_listener(wl.pointer, &pointer_listener, NULL);
	} else if (~capabilities & WL_SEAT_CAPABILITY_POINTER && wl.pointer) {
		wl_pointer_release(wl.pointer);
		wl.pointer = NULL;
	}
}

void setup_font(void)
{
	/* TODO: do scaling by using wl_output_istener */
	uint32_t scale = 1;
	/* NOTE: this expects the length of the font name to be less than 256 */
	char f[256];
	char dpi[12];
	unsigned i;
	char attrs[64];

	fcft_set_scaling_filter(FCFT_SCALING_FILTER_LANCZOS3);
	if (!fcft_init(FCFT_LOG_COLORIZE_AUTO, false, FCFT_LOG_CLASS_ERROR))
		die("fcft_init:");
	snprintf(dpi, sizeof(dpi), "dpi=%d", scale * 96);

	for (i = 0; i < 4; i++) {
		sprintf(f, "%s:size=%d:antialias=%s:autohint=%s", font,
			font_size, antialias ? "true" : "false",
			autohint ? "true" : "false");
		sprintf(attrs, "%s%s", dpi,
			(const char *[]){"", ":weight=bold", ":slant=italic",
					 ":weight=bold:slant=italic"}[i]);
		dc.fonts[i] = fcft_from_name(1, (const char *[]){f}, attrs);
	}

	/* NOTE: only works for monospace fonts */
	win.cw = dc.fonts[0]->max_advance.x;
	win.ch = dc.fonts[0]->height;
}

void setup(void)
{
	sigset_t mask;
	bool argb;

	/* TODO: add args */
	setlocale(LC_CTYPE, "");

	tnew(cols, rows);
	swt.fd.pty = ttynew(NULL, (char *)shell, NULL, NULL);

	xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!xkb.context) die("xkb_context_new:");

	wl.display = wl_display_connect(NULL);
	if (!wl.display) die("wl_display_connect:");
	wl.registry = wl_display_get_registry(wl.display);
	if (!wl.registry) die("wl_display_get_registry:");

	wl_registry_add_listener(wl.registry, &registry_listener, &argb);
	wl_display_roundtrip(wl.display);
	wl_display_roundtrip(wl.display);

	if (!wl.compositor) die("no wayland compositor registered");
	if (!xdg.wm_base) die("no xdg wm base registered");
	if (!argb) die("ARGB format is not supported");

	cursor_init();

	wl.surface = wl_compositor_create_surface(wl.compositor);
	if (!wl.surface) die("wl_compositor_create_surface:");

	xdg.surface = xdg_wm_base_get_xdg_surface(xdg.wm_base, wl.surface);
	if (!xdg.surface) die("xdg_wm_base_get_xdg_surface:");
	xdg_surface_add_listener(xdg.surface, &surface_listener, NULL);

	xdg.toplevel = xdg_surface_get_toplevel(xdg.surface);
	if (!xdg.toplevel) die("xdg_surface_get_toplevel:");

	xdg_toplevel_add_listener(xdg.toplevel, &toplevel_listener, NULL);
	xdg_toplevel_set_title(xdg.toplevel, title);
	xdg_toplevel_set_app_id(xdg.toplevel, app_id);

	wl_surface_commit(wl.surface);

	if (sigemptyset(&mask) < 0) die("sigemptyset:");
	if (sigaddset(&mask, SIGINT)) die("sigaddset:");
	if (sigaddset(&mask, SIGTERM)) die("sigaddset:");
	if (sigprocmask(SIG_BLOCK, &mask, NULL)) die("sigprocmask:");

	swt.fd.display = wl_display_get_fd(wl.display);
	swt.fd.signal  = signalfd(-1, &mask, 0);
	swt.fd.repeat  = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

	if (swt.fd.display < 0) die("wl_display_get_fd:");
	if (swt.fd.signal < 0) die("signalfd:");
	if (swt.fd.repeat < 0) die("timerfd:");

	setup_font();
	xloadcols();

	win.cursor = cursorshape;
	win.w      = 2 * borderpx + cols * win.cw;
	win.h      = 2 * borderpx + rows * win.ch;
}

void shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	(void)wl_shm;
	if (format == WL_SHM_FORMAT_ARGB8888) *(bool *)data = true;
}

void surface_configure(void *data, struct xdg_surface *xdg_surface,
		       uint32_t serial)
{
	(void)data;

	xdg_surface_ack_configure(xdg_surface, serial);

	swt.flags.can_draw = true;
}

void toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	(void)data;
	(void)xdg_toplevel;
	swt.flags.running = false;
}

void toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
			int32_t width, int32_t height, struct wl_array *states)
{
	int col, row;

	(void)data;
	(void)xdg_toplevel;
	(void)states;

	swt.flags.can_draw = false;

	if (width == win.w && height == win.h) return;

	if (width) win.w = width;
	if (height) win.h = height;

	col = MAX(1, (win.w - 2 * borderpx) / win.cw);
	row = MAX(1, (win.h - 2 * borderpx) / win.ch);

	tresize(col, row);

	win.tw = col * win.cw;
	win.th = row * win.ch;

	ttyresize(win.tw, win.th);
}

void wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	(void)data;
	xdg_wm_base_pong(xdg_wm_base, serial);
}

int main(void)
{
	setup();
	run();
	cleanup();
	return 0;
}

/* {{{ win.h */
void xbell(void) { warn("TODO: xbell"); }

void xclipcopy(void) { warn("TODO: xclipcopy"); }

#define GETPIXMANCOLOR(c)                                                      \
	(pixman_color_t *)(IS_TRUECOL(c)                                       \
			       ? &(pixman_color_t){TRUERED(c), TRUEGREEN(c),   \
						   TRUEBLUE(c), 0xFFFF}        \
			       : &(pixman_color_t){                            \
				     dc.col[c].red, dc.col[c].green,           \
				     dc.col[c].blue, dc.col[c].alpha})

static void drawunderline(Glyph g, int x, int y, struct fcft_font *f,
			  pixman_color_t *fg)
{
	pixman_image_t *pix = swt.buf.pix, *fill;
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

void xdrawglyph(Glyph g, int x, int y)
{
	pixman_image_t *pix = swt.buf.pix, *fg_pix;
	pixman_color_t *fg, *bg, *tmp;
	const struct fcft_glyph *glyph;
	struct fcft_font *f =
	    dc.fonts[(!!(g.mode & ATTR_BOLD)) + (!!(g.mode & ATTR_ITALIC)) * 2];
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

	pixman_image_fill_rectangles(
	    PIXMAN_OP_SRC, pix, bg, 1,
	    &(pixman_rectangle16_t){
		.x = winx, .y = winy, .width = win.cw, .height = win.ch});

	glyph = fcft_rasterize_char_utf32(f, g.u, FCFT_SUBPIXEL_DEFAULT);

	if (!glyph) return;

	fg_pix = pixman_image_create_solid_fill(fg);
	pixman_image_composite32(PIXMAN_OP_OVER, fg_pix, glyph->pix, pix, 0, 0,
				 0, 0, winx + glyph->x,
				 winy + win.ch - f->descent - glyph->y,
				 glyph->width, glyph->height);
	pixman_image_unref(fg_pix);

	drawunderline(g, winx, winy, f, fg);

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

void xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og)
{
	unsigned int thicc  = cursorthickness;
	pixman_image_t *pix = swt.buf.pix;
	pixman_color_t *drawcol;
	uint32_t tmp;
	int x = borderpx + cx * win.cw, y = borderpx + cy * win.ch;

	if (selected(ox, oy)) og.mode ^= ATTR_REVERSE;
	xdrawglyph(og, ox, oy);

	g.mode &=
	    ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE | ATTR_STRUCK | ATTR_WIDE;

	if (!selected(cx, cy)) {
		tmp  = g.fg;
		g.fg = g.bg;
		g.bg = tmp;
	}

	drawcol = GETPIXMANCOLOR(g.bg);

	if (IS_SET(MODE_FOCUSED)) {
		switch (win.cursor) {
		case 0: /* Blinking Block */
		case 1: /* Blinking Block (Default) */
		case 2: /* Steady Block */ xdrawglyph(g, cx, cy); break;
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

void xdrawline(Line line, int x1, int y1, int x2)
{
	Glyph g;
	for (; x1 < x2; x1++) {
		g = line[x1];
		if (g.mode == ATTR_WDUMMY) continue;
		xdrawglyph(g, x1, y1);
	}
}

void xfinishdraw(void)
{
	wl_surface_damage(wl.surface, 0, 0, win.w, win.h);
	swt.flags.can_draw = false;
	wl.callback        = wl_surface_frame(wl.surface);
	wl_callback_add_listener(wl.callback, &frame_listener, NULL);
	wl_surface_commit(wl.surface);
}

void xloadcols(void)
{
	uint i;
	static int loaded;

	if (!loaded) {
		dc.collen = MAX(LEN(colorpalette), 256);
		dc.col    = xmalloc(dc.collen * sizeof(pixman_color_t));
	}

	for (i = 0; i < dc.collen; i++) {
		dc.col[i].red   = TRUERED(colorpalette[i]);
		dc.col[i].green = TRUEGREEN(colorpalette[i]);
		dc.col[i].blue  = TRUEBLUE(colorpalette[i]);
		dc.col[i].alpha = 0xFFFF;
	}

	loaded = 1;
}

int xsetcolorname(int x, const char *name)
{
	(void)x;
	(void)name;
	warn("TODO: xsetcolorname");
	return 0;
}

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b)
{
	(void)x;
	(void)r;
	(void)g;
	(void)b;
	warn("TODO: xgetcolor");
	return 0;
}

void xseticontitle(char *p)
{
	/* FIXME: this might not be the correct way to set the icon
	 * title */
	if (!p || !*p) return;
	xdg_toplevel_set_title(xdg.toplevel, p);
}

void xsettitle(char *p)
{
	/* FIXME: this might not be the correct way to set the title */
	if (!p || !*p) return;
	xdg_toplevel_set_title(xdg.toplevel, p);
}

int xsetcursor(int cursor)
{
	if (!BETWEEN(cursor, 0, 6)) return 1;
	win.cursor = cursor;
	return 0;
}

void xsetmode(int set, unsigned int flags)
{
	int mode = win.mode;
	MODBIT(win.mode, set, flags);
	if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE)) redraw();
}

void xsetpointermotion(int set) { (void)set; }

void xsetsel(char *str)
{
	(void)str;
	warn("TODO: xsetsel");
}

int xstartdraw(void)
{
	struct buffer *buf = &swt.buf;

	/* TODO: ensure window is visible */
	if (!swt.flags.can_draw || buf->busy) return 0;

	buf->busy = true;
	buffer_init(&swt.buf);
	wl_surface_attach(wl.surface, buf->wl_buffer, 0, 0);

	return 1;
}

void xximspot(int x, int y)
{
	(void)x;
	(void)y;
}
/* }}} */
