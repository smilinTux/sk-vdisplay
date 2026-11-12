// sk-vdisplay: plug a virtual monitor into X via the evdi kernel module.
// Usage: sk-vdisplay <WIDTH>x<HEIGHT>[@HZ] [NAME]
// Keeps running; the monitor stays "connected" until this process exits.
// Each instance grabs the first free evdi device (load evdi with
// initial_device_count=N to have N slots).
#include <evdi_lib.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t stop;
static void on_sig(int s) { (void)s; stop = 1; }

// Frame pump. evdi only completes a page flip once a client has grabbed the
// frame, so without this loop X blocks on "flip_done timed out" and the
// whole desktop freezes. We keep one buffer registered at the current mode
// and keep requesting + grabbing updates.
#define MAX_RECTS 16
static struct {
	evdi_handle h;
	int registered, w, h_px, stride, want_grab;
	void *pix;
	struct evdi_rect rects[MAX_RECTS];
} st;

static void unregister_buf(void) {
	if (st.registered) { evdi_unregister_buffer(st.h, 1); st.registered = 0; }
	free(st.pix); st.pix = NULL;
}

static void register_buf(int w, int h) {
	unregister_buf();
	st.w = w; st.h_px = h; st.stride = w * 4;
	st.pix = calloc((size_t)st.stride * h, 1);
	if (!st.pix) return;
	struct evdi_buffer b = {.id = 1, .buffer = st.pix, .width = w, .height = h,
				.stride = st.stride, .rects = st.rects, .rect_count = 0};
	evdi_register_buffer(st.h, b);
	st.registered = 1;
}

static void grab(void) {
	int n = MAX_RECTS;
	evdi_grab_pixels(st.h, st.rects, &n);
}

static void update_ready(int buf, void *u) { (void)buf; (void)u; grab(); }

static void mode_changed(struct evdi_mode m, void *u) {
	(void)u;
	fprintf(stderr, "mode set: %dx%d@%d\n", m.width, m.height, m.refresh_rate);
	register_buf(m.width, m.height);
}
static void dpms(int mode, void *u) { (void)u; fprintf(stderr, "dpms %d\n", mode); }

// Build a 128-byte EDID 1.4 with one CVT reduced-blanking detailed timing.
static void build_edid(unsigned char e[128], int w, int h, int hz, const char *name, int idx) {
	memset(e, 0, 128);
	const unsigned char hdr[8] = {0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0};
	memcpy(e, hdr, 8);
	uint16_t mfg = (('S' - '@') << 10) | (('K' - '@') << 5) | ('V' - '@');
	e[8] = mfg >> 8; e[9] = mfg & 0xff;
	e[10] = 0x01 + idx; e[11] = 0x5d;          // product code
	e[12] = 0x01 + idx;                         // serial
	e[16] = 1; e[17] = 2026 - 1990;
	e[18] = 1; e[19] = 4;                       // EDID 1.4
	e[20] = 0xa5;                               // digital, 8bpc, DisplayPort
	e[21] = 53; e[22] = 30;                     // cm
	e[23] = 0x78;                               // gamma 2.2
	e[24] = 0x06;                               // sRGB, preferred = native
	const unsigned char chroma[10] = {0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26, 0x0f, 0x50, 0x54};
	memcpy(e + 25, chroma, 10);
	for (int i = 38; i < 54; i++) e[i] = 0x01;  // no standard timings

	// CVT-RB v1 timing
	int hblank = 160, hfp = 48, hsync = 32;
	int vfp = 3, vsync = (w * 9 == h * 16) ? 5 : (w * 10 == h * 16) ? 6 : (w * 3 == h * 4) ? 4 : 7;
	double line_est = (1.0 / hz - 460e-6) / h;  // seconds per line, rough
	int vblank = (int)ceil(460e-6 / line_est) + 1;
	if (vblank < vfp + vsync + 6) vblank = vfp + vsync + 6;
	int htot = w + hblank, vtot = h + vblank;
	int clk10k = (int)floor((double)htot * vtot * hz / 250000.0) * 25;  // 0.25 MHz steps, in 10 kHz
	unsigned char *d = e + 54;
	d[0] = clk10k & 0xff; d[1] = clk10k >> 8;
	d[2] = w & 0xff; d[3] = hblank & 0xff; d[4] = ((w >> 8) << 4) | (hblank >> 8);
	d[5] = h & 0xff; d[6] = vblank & 0xff; d[7] = ((h >> 8) << 4) | (vblank >> 8);
	d[8] = hfp & 0xff; d[9] = hsync & 0xff;
	d[10] = ((vfp & 0xf) << 4) | (vsync & 0xf);
	d[11] = ((hfp >> 8) << 6) | ((hsync >> 8) << 4) | ((vfp >> 4) << 2) | (vsync >> 4);
	d[12] = 530 & 0xff; d[13] = 300 & 0xff; d[14] = ((530 >> 8) << 4) | (300 >> 8);
	d[17] = 0x1a;                               // digital separate, H+ V-

	// monitor name
	d = e + 72;
	d[3] = 0xfc;
	int n = 0;
	for (; name[n] && n < 13; n++) d[5 + n] = name[n];
	if (n < 13) { d[5 + n++] = 0x0a; for (; n < 13; n++) d[5 + n] = 0x20; }

	// range limits
	d = e + 90;
	d[3] = 0xfd;
	d[5] = 24; d[6] = hz > 75 ? hz + 5 : 75; d[7] = 30; d[8] = 160;
	d[9] = (unsigned char)((clk10k / 1000) + 2);
	d[10] = 0x01; d[11] = 0x0a; for (int i = 12; i < 18; i++) d[i] = 0x20;

	e[108 + 3] = 0x10;                          // dummy descriptor
	unsigned sum = 0;
	for (int i = 0; i < 127; i++) sum += e[i];
	e[127] = (256 - (sum & 0xff)) & 0xff;
}

int main(int argc, char **argv) {
	int w, h, hz = 60;
	if (argc < 2 || sscanf(argv[1], "%dx%d@%d", &w, &h, &hz) < 2) {
		fprintf(stderr, "usage: %s WIDTHxHEIGHT[@HZ] [NAME]\n", argv[0]);
		return 2;
	}
	const char *name = argc > 2 ? argv[2] : "SK Virtual";

	evdi_handle hnd = EVDI_INVALID_HANDLE;
	int dev = -1;
	for (int i = 0; i < 32 && hnd == EVDI_INVALID_HANDLE; i++) {
		if (evdi_check_device(i) != AVAILABLE) continue;
		// evdi_check_device does not know whether another sk-vdisplay already
		// drives this device, so claim it with a per-device lock file.
		char lp[128];
		const char *rt = getenv("XDG_RUNTIME_DIR");
		snprintf(lp, sizeof lp, "%s/sk-vdisplay-card%d.lock", rt ? rt : "/tmp", i);
		int lfd = open(lp, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
		if (lfd < 0 || flock(lfd, LOCK_EX | LOCK_NB) != 0) {
			if (lfd >= 0) close(lfd);
			continue;
		}
		hnd = evdi_open(i);
		if (hnd != EVDI_INVALID_HANDLE) dev = i;  // lock fd stays open for our lifetime
		else close(lfd);
	}
	if (hnd == EVDI_INVALID_HANDLE) {
		fprintf(stderr, "no free evdi device (load evdi with initial_device_count=N)\n");
		return 1;
	}

	unsigned char edid[128];
	build_edid(edid, w, h, hz, name, dev);
	evdi_connect2(hnd, edid, sizeof edid, (uint32_t)w * h, (uint32_t)w * h * hz);
	fprintf(stderr, "connected %s %dx%d@%d on /dev/dri/card%d\n", name, w, h, hz, dev);

	signal(SIGINT, on_sig);
	signal(SIGTERM, on_sig);
	st.h = hnd;
	struct evdi_event_context ctx = {0};
	ctx.mode_changed_handler = mode_changed;
	ctx.update_ready_handler = update_ready;
	ctx.dpms_handler = dpms;
	struct pollfd p = {.fd = evdi_get_event_ready(hnd), .events = POLLIN};
	while (!stop) {
		// ask for the next frame; true = ready now, else update_ready fires later
		if (st.registered && evdi_request_update(hnd, 1)) grab();
		if (poll(&p, 1, 16) > 0) evdi_handle_events(hnd, &ctx);
	}
	unregister_buf();
	evdi_disconnect(hnd);
	evdi_close(hnd);
	fprintf(stderr, "disconnected\n");
	return 0;
}
