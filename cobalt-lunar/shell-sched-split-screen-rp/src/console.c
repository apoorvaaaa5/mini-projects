#include <console.h>
#include <usb_cdc.h>
#include <string.h>
#include <scheduler.h>
#include <telemetry.h>

#define MAX_PANE_ROWS  70
#define MAX_PANE_COLS  160
#define TOP_ROW        2

/* Dynamic 65:35 split-screen geometry */
static int term_rows = 40;
static int term_cols = 160;
static int div_col   = 104;  /* 65% of width */
static int right_col = 106;
static int box_width = 52;   /* 35% of width */
static int pane_rows = 37;
static int pane_cols = 102;

static volatile int split_active = 0;

static char left_lines[MAX_PANE_ROWS][MAX_PANE_COLS + 1];
static int cur_row = 0;
static int cur_col = 0;

static volatile int tx_lock = 0;

static int breathe_base_row = 0;
static int display_base_row = 0;

void console_lock(void)   { tx_lock = 1; }
void console_unlock(void) { tx_lock = 0; }
int  console_is_locked(void) { return tx_lock; }

int console_split_is_active(void)
{
	return split_active;
}

void console_write(const char *buf, uint32_t len)
{
	if (!usb_cdc_is_connected()) return;

	uint32_t sent = 0;
	uint32_t retries = 0;
	while (sent < len && retries < 10000) {
		uint32_t chunk = len - sent;
		if (chunk > 64) chunk = 64;

		int written = usb_cdc_write((const uint8_t *)(buf + sent), chunk);
		if (written > 0) {
			sent += written;
			retries = 0;
		} else {
			retries++;
		}
		usb_poll();
	}
}

static void console_goto_rc(int row, int col)
{
	char seq[16];
	int i = 0;
	seq[i++] = '\033';
	seq[i++] = '[';

	if (row >= 10) seq[i++] = '0' + (row / 10);
	seq[i++] = '0' + (row % 10);
	seq[i++] = ';';

	if (col >= 100) {
		seq[i++] = '0' + (col / 100);
		col %= 100;
	}
	if (col >= 10) seq[i++] = '0' + (col / 10);
	seq[i++] = '0' + (col % 10);
	seq[i++] = 'H';

	console_write(seq, i);
}

static void print_num_raw(uint32_t val)
{
	char buf[12];
	int i = 0;
	if (val == 0) {
		console_write("0", 1);
		return;
	}
	while (val > 0) {
		buf[i++] = '0' + (val % 10);
		val /= 10;
	}
	while (i > 0) {
		console_write(&buf[--i], 1);
	}
}

static void console_probe_term_size(void)
{
	uint8_t dummy;
	for (int d = 0; d < 64; d++) {
		if (usb_cdc_read(&dummy, 1) != 1) break;
	}

	console_write("\033[999;999H\033[6n", 14);

	uint32_t start = system_millis;
	char resp[40];
	int idx = 0;

	while ((system_millis - start) < 350 && idx < (int)(sizeof(resp) - 1)) {
		usb_poll();
		uint8_t ch;
		if (usb_cdc_read(&ch, 1) == 1) {
			resp[idx++] = (char)ch;
			if (ch == 'R') break;
		}
	}
	resp[idx] = '\0';

	int rows = 0;
	int cols = 0;
	char *bracket = 0;
	for (int i = 0; i < idx; i++) {
		if (resp[i] == '[') {
			bracket = &resp[i];
			break;
		}
	}

	if (bracket) {
		char *p = bracket + 1;
		while (*p >= '0' && *p <= '9') {
			rows = rows * 10 + (*p - '0');
			p++;
		}
		if (*p == ';') p++;
		while (*p >= '0' && *p <= '9') {
			cols = cols * 10 + (*p - '0');
			p++;
		}
	}

	if (rows >= 12 && cols >= 40) {
		term_rows = rows;
		term_cols = cols;
	} else {
		term_rows = 40;
		term_cols = 160;
	}

	if (term_cols > 210) term_cols = 210;
	if (term_rows > 55)  term_rows = 55;

	/* STRICT 65:35 PROPORTION REQUESTED BY USER */
	div_col = (term_cols * 65) / 100;
	pane_cols = div_col - 2;
	if (pane_cols > MAX_PANE_COLS) pane_cols = MAX_PANE_COLS;
	div_col = pane_cols + 2;

	right_col = div_col + 2;
	box_width = term_cols - right_col;
	if (box_width < 25) box_width = 25;

	pane_rows = term_rows - 3;
	if (pane_rows > MAX_PANE_ROWS) pane_rows = MAX_PANE_ROWS;
}

static void repaint_left_pane(void)
{
	console_write("\033[?25l", 6);
	for (int r = 0; r < pane_rows; r++) {
		console_goto_rc(TOP_ROW + r, 1);
		console_write(left_lines[r], pane_cols);
	}
	console_goto_rc(TOP_ROW + cur_row, 1 + cur_col);
	console_write("\033[?25h", 6);
}

void console_putc(char c)
{
	if (!split_active) {
		if (c == '\r') {
			console_write("\r", 1);
			return;
		}
		if (c == '\n') {
			console_write("\r\n", 2);
			return;
		}
		if (c == '\b' || c == 0x7F) {
			console_write("\b \b", 3);
			return;
		}
		console_write(&c, 1);
		return;
	}

	if (c == '\r') {
		cur_col = 0;
		console_goto_rc(TOP_ROW + cur_row, 1);
		return;
	}
	if (c == '\n') {
		cur_col = 0;
		cur_row++;
		if (cur_row >= pane_rows) {
			for (int r = 0; r < pane_rows - 1; r++) {
				memcpy(left_lines[r], left_lines[r + 1], pane_cols + 1);
			}
			memset(left_lines[pane_rows - 1], ' ', pane_cols);
			left_lines[pane_rows - 1][pane_cols] = '\0';
			cur_row = pane_rows - 1;
			repaint_left_pane();
		} else {
			console_goto_rc(TOP_ROW + cur_row, 1);
		}
		return;
	}
	if (c == '\b' || c == 0x7F) {
		if (cur_col > 0) {
			cur_col--;
			left_lines[cur_row][cur_col] = ' ';
			console_goto_rc(TOP_ROW + cur_row, 1 + cur_col);
			console_write(" ", 1);
			console_goto_rc(TOP_ROW + cur_row, 1 + cur_col);
		}
		return;
	}
	if (c >= 32 && c <= 126) {
		if (cur_col >= pane_cols) {
			cur_col = 0;
			cur_row++;
			if (cur_row >= pane_rows) {
				for (int r = 0; r < pane_rows - 1; r++) {
					memcpy(left_lines[r], left_lines[r + 1], pane_cols + 1);
				}
				memset(left_lines[pane_rows - 1], ' ', pane_cols);
				left_lines[pane_rows - 1][pane_cols] = '\0';
				cur_row = pane_rows - 1;
				repaint_left_pane();
			}
		}
		left_lines[cur_row][cur_col] = c;
		console_goto_rc(TOP_ROW + cur_row, 1 + cur_col);
		console_write(&c, 1);
		cur_col++;
	}
}

void console_puts(const char *str)
{
	if (!str) return;
	while (*str) {
		console_putc(*str++);
	}
}

int console_getc(void)
{
	uint8_t ch = 0;
	usb_poll();
	if (usb_cdc_read(&ch, 1) != 1) {
		return -1;
	}

	if (ch == 0x1B) {
		int count = 0;
		uint32_t start_poll = system_millis;
		while ((system_millis - start_poll) < 25 && count < 28) {
			usb_poll();
			uint8_t next_ch;
			if (usb_cdc_read(&next_ch, 1) == 1) {
				count++;
				if (next_ch == 'R' || (next_ch >= 'A' && next_ch <= 'Z') || next_ch == '~') {
					break;
				}
				start_poll = system_millis;
			}
		}
		if (count == 0) return 0x1B;
		return -1;
	}

	return (int)ch;
}

int console_has_char(void)
{
	usb_poll();
	return 0;
}

static void raw_puts(const char *str)
{
	if (!str) return;
	console_write(str, strlen(str));
}

static void draw_box_border(int row)
{
	console_goto_rc(row, right_col);
	console_write("\033[90m+", 6);
	for (int c = 0; c < box_width - 2; c++) {
		console_write("-", 1);
	}
	console_write("+\033[0m", 5);
}

static void draw_box_row(int row, const char *colored_text, int visible_len)
{
	console_goto_rc(row, right_col);
	console_write("\033[90m|\033[0m ", 10);
	console_write(colored_text, strlen(colored_text));
	int inner_width = box_width - 4;
	int pad = inner_width - visible_len;
	while (pad-- > 0) console_write(" ", 1);
	console_write(" \033[90m|\033[0m", 10);
}

static void clear_right_pane(void)
{
	for (int r = 2; r <= term_rows - 1; r++) {
		console_goto_rc(r, right_col);
		for (int c = 0; c < box_width; c++) console_write(" ", 1);
	}
}

/* Render all right-pane telemetry cards strictly from the Shell task */
void console_split_render_telemetry(void)
{
	if (!usb_cdc_is_connected() || !split_active) return;
	console_lock();
	console_write("\033[?25l", 6);

	int bg_slots[MAX_TASKS];
	int bg_count = 0;
	for (int i = 1; i < MAX_TASKS; i++) {
		if (task[i].state != t_quit && task[i].name[0] != '\0' && task[i].mode == MODE_BACKGROUND) {
			bg_slots[bg_count++] = i;
		}
	}

	if (bg_count > 0) {
		int current_row = 2;
		for (int b = 0; b < bg_count && b < 2; b++) {
			int slot = bg_slots[b];
			if (strcmp(task[slot].name, "breathe") == 0) {
				breathe_base_row = current_row;
				draw_box_border(current_row);
				draw_box_row(current_row + 1, "\033[1;36m[BG: breathe]\033[0m  (PID 1)", 22);
				draw_box_row(current_row + 2, "Hardware PWM breathing on GPIO 25", 33);
				draw_box_row(current_row + 3, "Carrier: 2 kHz | Step: 10ms", 27);
				draw_box_row(current_row + 4, "Cadence: Continuous (10ms steps)", 32);

				/* Dynamic Duty Progress Bar */
				int inner_width = box_width - 4;
				int bar_slots = (inner_width * 45) / 100;
				if (bar_slots < 12) bar_slots = 12;
				if (bar_slots > 30) bar_slots = 30;

				int duty = g_telemetry.breathe_duty;
				int bars = (duty * bar_slots) / 100;

				console_goto_rc(current_row + 5, right_col);
				raw_puts("\033[90m|\033[0m Duty: [");
				raw_puts("\033[1;32m");
				for (int s = 0; s < bar_slots; s++) {
					if (s < bars) console_write("=", 1);
					else console_write(" ", 1);
				}
				raw_puts("\033[0m] ");
				if (duty < 10) console_write("  ", 2);
				else if (duty < 100) console_write(" ", 1);
				print_num_raw((uint32_t)duty);
				console_write("%", 1);
				int used = 8 + bar_slots + 2 + 5;
				int pad = inner_width - used;
				while (pad-- > 0) console_write(" ", 1);
				raw_puts(" \033[90m|\033[0m");

				draw_box_row(current_row + 6, "Status: \033[1;32mACTIVE (BG)\033[0m", 19);
				draw_box_border(current_row + 7);
				current_row += 9;
			} else if (strcmp(task[slot].name, "display") == 0) {
				display_base_row = current_row;
				draw_box_border(current_row);
				draw_box_row(current_row + 1, "\033[1;36m[BG: display]\033[0m  (PID 2)", 22);
				draw_box_row(current_row + 2, "Interactive counter display", 27);
				draw_box_row(current_row + 3, "Cadence: 2000 ms periodic", 25);

				/* Live Tick and Spinner */
				console_goto_rc(current_row + 4, right_col);
				raw_puts("\033[90m|\033[0m Tick: ");
				uint32_t ticks = g_telemetry.display_ticks;
				char spin = g_telemetry.display_spinner;
				print_num_raw(ticks);

				uint32_t temp = ticks;
				int digits = (temp == 0) ? 1 : 0;
				while (temp > 0) { digits++; temp /= 10; }
				int pad_tick = 6 - digits;
				if (pad_tick < 0) pad_tick = 0;
				while (pad_tick-- > 0) console_write(" ", 1);

				raw_puts("  Spinner: [\033[1;33m");
				console_write(&spin, 1);
				raw_puts("\033[0m]");
				int inner_width = box_width - 4;
				int used = 26;
				int pad = inner_width - used;
				while (pad-- > 0) console_write(" ", 1);
				raw_puts(" \033[90m|\033[0m");

				draw_box_row(current_row + 5, "Status: \033[1;32mACTIVE (BG)\033[0m", 19);
				draw_box_border(current_row + 6);
				current_row += 8;
			} else {
				draw_box_border(current_row);
				draw_box_row(current_row + 1, "\033[1;36m[BG: Generic Task]\033[0m", 18);
				draw_box_row(current_row + 2, task[slot].name, strlen(task[slot].name));
				draw_box_row(current_row + 3, "Mode: BACKGROUND", 16);
				draw_box_row(current_row + 4, "Status: \033[1;32mACTIVE (BG)\033[0m", 19);
				draw_box_border(current_row + 5);
				current_row += 7;
			}
		}

		/* System Summary card filling remaining vertical height */
		if (current_row <= term_rows - 8) {
			draw_box_border(current_row);
			draw_box_row(current_row + 1, "\033[1;37m[SYSTEM ARCHITECTURE & HARDWARE]\033[0m", 32);
			draw_box_row(current_row + 2, "MCU: RP2040 Dual ARM Cortex-M0+ @ 125 MHz", 41);
			draw_box_row(current_row + 3, "SRAM: 264 KB Multi-Bank | Flash: 2MB QSPI XIP", 45);
			draw_box_row(current_row + 4, "Scheduler: Preemptive Priority Round-Robin", 42);
			draw_box_row(current_row + 5, "Context Switch: SysTick Interrupt (1 ms)", 40);
			draw_box_row(current_row + 6, "Serial Bus: USB CDC Virtual COM (12 Mbps)", 41);
			draw_box_border(current_row + 7);
			current_row += 9;
		}

		/* Live Peripherals & Safety Card filling tall terminal screens */
		if (current_row <= term_rows - 7) {
			draw_box_border(current_row);
			draw_box_row(current_row + 1, "\033[1;35m[PERIPHERALS & FAIL-SAFE TELEMETRY]\033[0m", 35);
			draw_box_row(current_row + 2, "PWM Slice: Channel 4B (GPIO 25 Green LED)", 41);
			draw_box_row(current_row + 3, "Stack Guard: 1024 Bytes Isolated per Task", 41);
			draw_box_row(current_row + 4, "TUI Mode: Responsive Dual-Pane Canvas", 37);
			draw_box_row(current_row + 5, "Fail-Safe: Auto pwm_set(0) on Disconnect", 40);
			draw_box_border(current_row + 6);
			current_row += 8;
		}
	}

	console_goto_rc(TOP_ROW + cur_row, 1 + cur_col);
	console_write("\033[?25h", 6);
	console_unlock();
}

void console_split_activate(void)
{
	console_lock();
	split_active = 1;

	console_probe_term_size();

	console_write("\033[?1049h\033[2J\033[?25l", 15);

	for (int r = 0; r < MAX_PANE_ROWS; r++) {
		memset(left_lines[r], ' ', pane_cols);
		left_lines[r][pane_cols] = '\0';
	}
	cur_row = 0;
	cur_col = 0;

	/* Header bar */
	console_goto_rc(1, 1);
	raw_puts("\033[1;36m=== StrataHalo: Interactive Shell ===\033[0m");
	for (int c = 38; c < div_col - 1; c++) console_write(" ", 1);
	raw_puts("\033[90m|\033[0m ");
	raw_puts("\033[1;36m=== LIVE SYSTEM TELEMETRY ===\033[0m");
	for (int c = div_col + 30; c < term_cols; c++) console_write(" ", 1);

	/* Vertical divider at 65% width */
	for (int r = 2; r <= term_rows - 1; r++) {
		console_goto_rc(r, div_col);
		console_write("\033[90m|\033[0m", 9);
	}

	/* Footer bar at bottom */
	console_goto_rc(term_rows, 1);
	console_write("\033[90mType 'help' | 'ls' | 'run <app> [&]' | 'ps' | 'kill <app>' | 'resize' | 'clear'\033[0m", 85);
	for (int c = 77; c < term_cols; c++) console_write(" ", 1);

	clear_right_pane();
	console_unlock();

	console_split_render_telemetry();

	console_goto_rc(TOP_ROW + cur_row, 1 + cur_col);
	console_write("\033[?25h", 6);
}

void console_split_resize(void)
{
	if (!split_active) return;
	console_split_activate();
}

void console_split_deactivate(void)
{
	console_lock();
	split_active = 0;
	breathe_base_row = 0;
	display_base_row = 0;

	console_write("\033[?1049l\033[2J\033[H\033[?25h", 18);

	console_unlock();
}

void console_split_clear_left(void)
{
	if (!split_active) {
		console_write("\033[2J\033[H", 7);
		return;
	}
	console_lock();
	console_write("\033[?25l", 6);
	for (int r = 0; r < pane_rows; r++) {
		memset(left_lines[r], ' ', pane_cols);
		left_lines[r][pane_cols] = '\0';
		console_goto_rc(TOP_ROW + r, 1);
		console_write(left_lines[r], pane_cols);
	}
	cur_row = 0;
	cur_col = 0;
	console_goto_rc(TOP_ROW + cur_row, 1 + cur_col);
	console_write("\033[?25h", 6);
	console_unlock();
}
