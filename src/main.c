#include "os.h"
#include "os_io_seproxyhal.h"

// This needs to go after os.h
#include "cx.h"

#define WIDTH 128
#define HEIGHT 32
// Apparently, there is a limit to icon size. Use a set of reasonable ones
// instead of one giant one, at significant added complexity.
#define ICON_WIDTH 16
#define ICON_HEIGHT 16
#define COLUMNS (WIDTH / ICON_WIDTH)
#define ROWS (HEIGHT / ICON_HEIGHT)
#define BPP 1
#define CHAR_BIT 8
#define ICON_BUFFER_SIZE (ICON_HEIGHT * ICON_WIDTH * BPP / CHAR_BIT)

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];
ux_state_t ux;

unsigned char bitmap[ROWS][COLUMNS][ICON_BUFFER_SIZE];
unsigned char next_bitmap[ROWS][COLUMNS][ICON_HEIGHT * ICON_WIDTH * BPP / CHAR_BIT];

const unsigned int colors[1 << BPP] = {0, 0xFFFFFF};
static bagl_element_t elements[1 + ROWS * COLUMNS];
bagl_icon_details_t icon_details[ROWS][COLUMNS];

const static unsigned int intervals[] = {64, 128, 256, 512, 1024};
int interval;
int randomize;

unsigned int seed;

unsigned int fast_rng() {
	// Stolen from the internet
	return seed = (seed * 1103515245 + 12345) & ((1U << (sizeof(seed) * CHAR_BIT - 1)) - 1);
}

static unsigned int get_pixel(unsigned char bitmap[ROWS][COLUMNS][ICON_BUFFER_SIZE], int row, int column) {
	int r = row / ICON_HEIGHT;
	int c = column / ICON_WIDTH;
	int i = (row % ICON_HEIGHT) * ICON_WIDTH + (column % ICON_WIDTH) * BPP;
	int i1 = i / CHAR_BIT;
	int i2 = i % CHAR_BIT;
	return (bitmap[r][c][i1] & (((1 << BPP) - 1) << i2)) >> i2;
}

static void set_pixel(unsigned char bitmap[ROWS][COLUMNS][ICON_BUFFER_SIZE], int row, int column, unsigned int value) {
	int r = row / ICON_HEIGHT;
	int c = column / ICON_WIDTH;
	int i = (row % ICON_HEIGHT) * ICON_WIDTH + (column % ICON_WIDTH) * BPP;
	int i1 = i / CHAR_BIT;
	int i2 = i % CHAR_BIT;
	bitmap[r][c][i1] = (bitmap[r][c][i1] & ~(((1 << BPP) - 1) << i2)) | value << (i2 * BPP);
}

static unsigned int elements_button(unsigned int button_mask, unsigned int button_mask_counter) {
	switch (button_mask) {
	case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT:
		os_sched_exit(0);
		break;
	case BUTTON_EVT_RELEASED | BUTTON_LEFT:
		randomize = !randomize;
		break;
	case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
		++interval;
		interval %= sizeof(intervals) / sizeof(*intervals);
		break;
	default:
		return 0;
	}
	return 0;
}

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
	return 0;
}

// Returns [0, b) even for negative a
static inline int modulo(int a, int b) {
	return (a % b + b) % b;
}

static int get_neighbor_count(int r, int c) {
	return !!get_pixel(bitmap, modulo(r - 1, HEIGHT), modulo(c - 1, WIDTH)) +
	       !!get_pixel(bitmap, modulo(r - 1, HEIGHT), modulo(c, WIDTH)) +
	       !!get_pixel(bitmap, modulo(r - 1, HEIGHT), modulo(c + 1, WIDTH)) +
	       !!get_pixel(bitmap, modulo(r, HEIGHT), modulo(c - 1, WIDTH)) +
	       !!get_pixel(bitmap, modulo(r, HEIGHT), modulo(c + 1, WIDTH)) +
	       !!get_pixel(bitmap, modulo(r + 1, HEIGHT), modulo(c - 1, WIDTH)) +
	       !!get_pixel(bitmap, modulo(r + 1, HEIGHT), modulo(c, WIDTH)) +
	       !!get_pixel(bitmap, modulo(r + 1, HEIGHT), modulo(c + 1, WIDTH));
}

static void ui_redraw(void) {
	for (int r = 0; r < HEIGHT; ++r) {
		for (int c = 0; c < WIDTH; ++c) {
			int count = get_neighbor_count(r, c);
			set_pixel(next_bitmap, r, c, count == 3 || (get_pixel(bitmap, r, c) && count == 2) || (randomize && count && !get_pixel(bitmap, r, c) && !(fast_rng() % 0xfff)));
		}
	}
	for (int r = 0; r < ROWS; ++r) {
		for (int c = 0; c < COLUMNS; ++c) {
			os_memcpy(bitmap[r][c], next_bitmap[r][c], ICON_BUFFER_SIZE);
		}
	}
	UX_DISPLAY(elements, NULL);
}

void app_main(void) {
	UX_CALLBACK_SET_INTERVAL(intervals[interval]);

	for (;;) {
		BEGIN_TRY {
			TRY {
				io_exchange(CHANNEL_APDU, 0);
			}
			CATCH_ALL {
			}
			FINALLY {
			}
		}
		END_TRY;
	}
}

void io_seproxyhal_display(const bagl_element_t *element) {
	io_seproxyhal_display_default((bagl_element_t *)element);
}

unsigned char io_event(unsigned char channel) {
	switch (*G_io_seproxyhal_spi_buffer) {
	case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
		UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
		break;
	case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
		UX_DISPLAYED_EVENT();
		break;
	case SEPROXYHAL_TAG_TICKER_EVENT:
		UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {
			UX_CALLBACK_SET_INTERVAL(intervals[interval]);
			ui_redraw();
		});
		break;
	default:
		break;
	}

	if (!io_seproxyhal_spi_is_status_sent()) {
		io_seproxyhal_general_status();
	}

	return 1;
}

void init_ui(void) {
	*elements = (bagl_element_t){
	    {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0},
	    NULL,
	    0,
	    0,
	    0,
	    NULL,
	    NULL,
	    NULL,

	};

	for (int r = 0; r < ROWS; ++r) {
		for (int c = 0; c < COLUMNS; ++c) {
			int index = r * COLUMNS + c;
			icon_details[r][c] = (bagl_icon_details_t){
			    ICON_WIDTH,
			    ICON_HEIGHT,
			    1,
			    colors,
			    bitmap[r][c]};
			elements[1 + index] = (bagl_element_t){
			    {BAGL_ICON, 0x00, ICON_WIDTH * c, ICON_HEIGHT * r, ICON_WIDTH, ICON_HEIGHT, 0, 0, BAGL_FILL, 0xFFFFFF, 0x000000, 0, 0},
			    (const char *)&icon_details[r][c],
			    0,
			    0,
			    0,
			    NULL,
			    NULL,
			    NULL,
			};
		}
	}

	for (int r = 0; r < HEIGHT; ++r) {
		for (int c = 0; c < WIDTH; ++c) {
			set_pixel(bitmap, r, c, cx_rng_u8() < 0x7f);
		}
	}

	interval = 0;
	randomize = 0;
	cx_rng((unsigned char *)&seed, sizeof(seed));
}

__attribute__((section(".boot"))) int main(void) {
	asm volatile("cpsie i");

	UX_INIT();

	os_boot();

	BEGIN_TRY {
		TRY {
			io_seproxyhal_init();

			init_ui();

			ui_redraw();

			app_main();
		}
		CATCH_ALL {
		}
		FINALLY {
		}
	}
	END_TRY;
}
