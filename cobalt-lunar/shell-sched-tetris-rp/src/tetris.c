#include <tetris.h>
#include <console.h>
#include <shell.h>
#include <scheduler.h>
#include <systick.h>
#include <string.h>
#include <stdbool.h>

#define BOARD_W  10
#define BOARD_H  20

/* Tetromino Piece Coordinates */
typedef struct {
	int8_t x, y;
} coord_t;

/* 7 Tetrominoes (I, O, T, S, Z, J, L), 4 rotations, 4 blocks each */
static const coord_t TETROMINOES[7][4][4] = {
	/* 0: I Piece (Cyan) */
	{
		{ {0, 1}, {1, 1}, {2, 1}, {3, 1} },
		{ {2, 0}, {2, 1}, {2, 2}, {2, 3} },
		{ {0, 2}, {1, 2}, {2, 2}, {3, 2} },
		{ {1, 0}, {1, 1}, {1, 2}, {1, 3} }
	},
	/* 1: O Piece (Yellow) */
	{
		{ {1, 0}, {2, 0}, {1, 1}, {2, 1} },
		{ {1, 0}, {2, 0}, {1, 1}, {2, 1} },
		{ {1, 0}, {2, 0}, {1, 1}, {2, 1} },
		{ {1, 0}, {2, 0}, {1, 1}, {2, 1} }
	},
	/* 2: T Piece (Magenta) */
	{
		{ {1, 0}, {0, 1}, {1, 1}, {2, 1} },
		{ {1, 0}, {1, 1}, {2, 1}, {1, 2} },
		{ {0, 1}, {1, 1}, {2, 1}, {1, 2} },
		{ {1, 0}, {0, 1}, {1, 1}, {1, 2} }
	},
	/* 3: S Piece (Green) */
	{
		{ {1, 0}, {2, 0}, {0, 1}, {1, 1} },
		{ {1, 0}, {1, 1}, {2, 1}, {2, 2} },
		{ {1, 1}, {2, 1}, {0, 2}, {1, 2} },
		{ {0, 0}, {0, 1}, {1, 1}, {1, 2} }
	},
	/* 4: Z Piece (Red) */
	{
		{ {0, 0}, {1, 0}, {1, 1}, {2, 1} },
		{ {2, 0}, {1, 1}, {2, 1}, {1, 2} },
		{ {0, 1}, {1, 1}, {1, 2}, {2, 2} },
		{ {1, 0}, {0, 1}, {1, 1}, {0, 2} }
	},
	/* 5: J Piece (Blue) */
	{
		{ {0, 0}, {0, 1}, {1, 1}, {2, 1} },
		{ {1, 0}, {2, 0}, {1, 1}, {1, 2} },
		{ {0, 1}, {1, 1}, {2, 1}, {2, 2} },
		{ {1, 0}, {1, 1}, {0, 2}, {1, 2} }
	},
	/* 6: L Piece (Orange) */
	{
		{ {2, 0}, {0, 1}, {1, 1}, {2, 1} },
		{ {1, 0}, {1, 1}, {1, 2}, {2, 2} },
		{ {0, 1}, {1, 1}, {2, 1}, {0, 2} },
		{ {0, 0}, {1, 0}, {1, 1}, {1, 2} }
	}
};

/* Solid Color Block Strings for Pieces 0..6:
 * Uses dual ANSI background + bright foreground with Unicode Full Block (██)
 * Ensures 100% solid, filled, gapless vibrant color blocks on all terminal emulators!
 */
static const char *const PIECE_BLOCKS[7] = {
	"\033[46;96m██\033[0m",            /* 0: I (Bright Cyan solid block) */
	"\033[43;93m██\033[0m",            /* 1: O (Bright Yellow solid block) */
	"\033[45;95m██\033[0m",            /* 2: T (Bright Magenta solid block) */
	"\033[42;92m██\033[0m",            /* 3: S (Bright Green solid block) */
	"\033[41;91m██\033[0m",            /* 4: Z (Bright Red solid block) */
	"\033[44;94m██\033[0m",            /* 5: J (Bright Blue solid block) */
	"\033[48;5;208;38;5;208m██\033[0m" /* 6: L (Bright Orange solid block) */
};

/* Session High Score (persists in RAM across game runs) */
static uint32_t session_high_score = 0;

/* PRNG State for 7-bag randomizer */
static uint32_t rng_state = 0x1A2B3C4D;

static uint32_t tetris_prng(void)
{
	rng_state = rng_state * 1664525UL + 1013904223UL;
	return (rng_state >> 16) & 0x7FFF;
}

/* 7-Bag Randomizer */
static int bag[7];
static int bag_index = 7;

static void refill_bag(void)
{
	for (int i = 0; i < 7; i++) {
		bag[i] = i;
	}
	for (int i = 6; i > 0; i--) {
		int j = tetris_prng() % (i + 1);
		int tmp = bag[i];
		bag[i] = bag[j];
		bag[j] = tmp;
	}
	bag_index = 0;
}

static int get_next_bag_piece(void)
{
	if (bag_index >= 7) {
		refill_bag();
	}
	return bag[bag_index++];
}

/* Board Matrix */
static uint8_t board[BOARD_H][BOARD_W];

/* Collision Checking */
static bool can_place(int piece, int rot, int px, int py)
{
	for (int i = 0; i < 4; i++) {
		int x = px + TETROMINOES[piece][rot][i].x;
		int y = py + TETROMINOES[piece][rot][i].y;

		if (x < 0 || x >= BOARD_W || y >= BOARD_H) {
			return false;
		}
		if (y >= 0 && board[y][x] != 0) {
			return false;
		}
	}
	return true;
}

/* Calculate Drop Shadow (Ghost Piece) Y coordinate */
static int get_ghost_y(int piece, int rot, int px, int py)
{
	int gy = py;
	while (can_place(piece, rot, px, gy + 1)) {
		gy++;
	}
	return gy;
}

/* Clear Completed Lines and return count */
static int clear_full_lines(void)
{
	int cleared = 0;
	for (int y = BOARD_H - 1; y >= 0; y--) {
		bool full = true;
		for (int x = 0; x < BOARD_W; x++) {
			if (board[y][x] == 0) {
				full = false;
				break;
			}
		}
		if (full) {
			cleared++;
			for (int sy = y; sy > 0; sy--) {
				for (int x = 0; x < BOARD_W; x++) {
					board[sy][x] = board[sy - 1][x];
				}
			}
			for (int x = 0; x < BOARD_W; x++) {
				board[0][x] = 0;
			}
			y++; /* Re-evaluate same row index */
		}
	}
	return cleared;
}

/* Speed scaling in milliseconds */
static uint32_t get_drop_interval_ms(int level)
{
	if (level <= 1) return 750;
	if (level == 2) return 650;
	if (level == 3) return 550;
	if (level == 4) return 450;
	if (level == 5) return 380;
	if (level == 6) return 300;
	if (level == 7) return 230;
	if (level == 8) return 160;
	if (level == 9) return 110;
	return 70;
}

/* Render Frame to Serial Console */
static void render_screen(int cur_piece, int cur_rot, int cur_x, int cur_y,
                          int next_piece, uint32_t score, uint32_t lines, int level,
                          bool game_over)
{
	int ghost_y = get_ghost_y(cur_piece, cur_rot, cur_x, cur_y);

	/* Home cursor to top-left to eliminate flicker */
	console_puts("\033[H");

	/* Row 0: Top borders */
	console_puts("\033[37m+--------------------+    +----------------------+\033[0m\r\n");

	/* Rows 1 to 20: Board rows (y = 0..19) + Side Panel */
	for (int y = 0; y < BOARD_H; y++) {
		console_puts("\033[37m|\033[0m");

		/* Board Columns */
		for (int x = 0; x < BOARD_W; x++) {
			/* Check active piece */
			bool is_active = false;
			for (int i = 0; i < 4; i++) {
				if ((cur_x + TETROMINOES[cur_piece][cur_rot][i].x == x) &&
				    (cur_y + TETROMINOES[cur_piece][cur_rot][i].y == y)) {
					is_active = true;
					break;
				}
			}

			if (is_active) {
				console_puts(PIECE_BLOCKS[cur_piece]);
				continue;
			}

			/* Check ghost piece projection (dim outline) */
			bool is_ghost = false;
			if (ghost_y != cur_y) {
				for (int i = 0; i < 4; i++) {
					if ((cur_x + TETROMINOES[cur_piece][cur_rot][i].x == x) &&
					    (ghost_y + TETROMINOES[cur_piece][cur_rot][i].y == y)) {
						is_ghost = true;
						break;
					}
				}
			}

			if (is_ghost) {
				console_puts("\033[90m::\033[0m");
				continue;
			}

			/* Check fixed landed blocks */
			if (board[y][x] != 0) {
				int p_id = board[y][x] - 1;
				if (p_id >= 0 && p_id < 7) {
					console_puts(PIECE_BLOCKS[p_id]);
				} else {
					console_puts("  ");
				}
			} else {
				console_puts("  ");
			}
		}

		console_puts("\033[37m|    \033[0m");

		/* Side Panel Content */
		switch (y) {
		case 0:
			console_puts("\033[36m|     NEXT PIECE       |\033[0m");
			break;
		case 1:
			console_puts("|                      |");
			break;
		case 2:
		case 3:
		case 4:
		case 5: {
			/* Draw 4x4 preview for next piece (py = 0..3) */
			int py = y - 2;
			console_puts("|       ");
			for (int px = 0; px < 4; px++) {
				bool p_blk = false;
				for (int i = 0; i < 4; i++) {
					if (TETROMINOES[next_piece][0][i].x == px &&
					    TETROMINOES[next_piece][0][i].y == py) {
						p_blk = true;
						break;
					}
				}
				if (p_blk) {
					console_puts(PIECE_BLOCKS[next_piece]);
				} else {
					console_puts("  ");
				}
			}
			console_puts("       |");
			break;
		}
		case 6:
			console_puts("\033[36m+----------------------+\033[0m");
			break;
		case 7:
			console_puts("SCORE:      \033[93m");
			console_print_dec(score);
			console_puts("\033[0m");
			break;
		case 8:
			console_puts("HIGH SCORE: \033[92m");
			console_print_dec(session_high_score);
			console_puts("\033[0m");
			break;
		case 9:
			console_puts("LINES:      ");
			console_print_dec(lines);
			break;
		case 10:
			console_puts("LEVEL:      \033[95m");
			console_print_dec((uint32_t)level);
			console_puts("\033[0m");
			break;
		case 11:
			if (game_over) {
				console_puts("\033[91m*** GAME OVER ***      \033[0m");
			} else {
				console_puts("                       ");
			}
			break;
		case 12:
			console_puts("\033[33m+----------------------+\033[0m");
			break;
		case 13:
			console_puts("\033[33m|       CONTROLS       |\033[0m");
			break;
		case 14:
			console_puts("| \033[97m<- / ->\033[0m   : Move     |");
			break;
		case 15:
			console_puts("| \033[97mUp Arrow\033[0m  : Rotate   |");
			break;
		case 16:
			console_puts("| \033[97mDown Arrow\033[0m: Soft Drop|");
			break;
		case 17:
			console_puts("| \033[97mSpacebar\033[0m  : Hard Drop|");
			break;
		case 18:
			console_puts("| \033[97mCtrl+Z\033[0m   : Bg Suspend|");
			break;
		case 19:
			console_puts("| \033[97mQ / q\033[0m    : Quit OS   |");
			break;
		default:
			break;
		}

		console_puts("\r\n");
	}

	/* Row 21: Bottom board border */
	console_puts("\033[37m+--------------------+    RP2040 OS (GPIO 15 Audio | Ctrl+Z: BG)\033[0m\r\n");
}

/* Game Over Overlay and Clean Exit to OS Shell */
static void end_game_and_exit(uint32_t score, uint32_t lines, int level)
{
	/* 1. Dramatic Red GAME OVER banner centered on board */
	console_puts("\033[9;2H\033[41;97;1m  ==================  \033[0m");
	console_puts("\033[10;2H\033[41;97;1m    G A M E   O V E R   \033[0m");
	console_puts("\033[11;2H\033[41;97;1m  ==================  \033[0m");
	console_puts("\033[13;3H\033[93;1mFinal Score: ");
	console_print_dec(score);
	console_puts("\033[0m");
	console_puts("\033[14;3H\033[96mLines: ");
	console_print_dec(lines);
	console_puts("  Level: ");
	console_print_dec((uint32_t)level);
	console_puts("\033[0m");
	console_puts("\033[16;3H\033[90mReturning to OS...\033[0m");

	/* 2. Pause 2.5 seconds so player can see their stats (or skip on any key) */
	uint32_t start_wait = system_millis;
	while ((system_millis - start_wait) < 2500) {
		int k = fg_key_get();
		if (k >= 0) break;
		mdelay(50);
	}

	/* 3. Restore terminal cursor and clear screen */
	console_puts("\033[?25h\033[0m\033[2J\033[H");

	/* 4. Print clean post-game summary in the terminal */
	console_puts("============================================================\r\n");
	console_puts("  >>> TETRIS: GAME OVER <<<\r\n");
	console_puts("  Cause of Exit : Blocks reached ceiling (Top-Out)\r\n");
	console_puts("  Final Score   : ");
	console_print_dec(score);
	if (score >= session_high_score && score > 0) {
		console_puts("  \033[92;1m[NEW HIGH SCORE!]\033[0m");
	}
	console_puts("\r\n  High Score    : ");
	console_print_dec(session_high_score);
	console_puts("\r\n  Lines Cleared : ");
	console_print_dec(lines);
	console_puts("\r\n  Level Achieved: ");
	console_print_dec((uint32_t)level);
	console_puts("\r\n============================================================\r\n");
	console_puts("Task exited cleanly. Type 'run tetris' to start a new game!\r\n\r\n");
}

/* Main Tetris Task Entry Point */
void task_tetris(void)
{
	/* Mix system tick into PRNG seed */
	rng_state ^= system_millis;
	refill_bag();

	/* Initialize Board */
	memset(board, 0, sizeof(board));

	uint32_t score = 0;
	uint32_t lines = 0;
	int level = 1;

	int cur_piece = get_next_bag_piece();
	int cur_rot = 0;
	int cur_x = 3;
	int cur_y = 0;

	int next_piece = get_next_bag_piece();

	bool was_fg = false;
	uint32_t last_drop_time = system_millis;

	while (1) {
		/* Background Check: If we are not the foreground task, pause quietly without touching the terminal! */
		if (!shell_is_foreground()) {
			was_fg = false;
			/* Yield to cooperative OS scheduler while waiting in background */
			mdelay(50);
			continue;
		}

		/* We are in foreground! If we just switched from background to foreground, take over and redraw screen */
		if (!was_fg) {
			was_fg = true;
			console_puts("\033[?25l\033[2J\033[H");
			render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
			last_drop_time = system_millis;
		}

		/* Non-blocking key retrieval from shell FIFO */
		int key = fg_key_get();
		while (key >= 0) {
			if (key == 'q' || key == 'Q') {
				/* Graceful exit to OS shell */
				console_puts("\033[?25h\033[0m\033[2J\033[H");
				console_puts("[TETRIS] Game exited by player.\r\n\r\n");
				return;
			}

			if (key == KEY_LEFT) {
				if (can_place(cur_piece, cur_rot, cur_x - 1, cur_y)) {
					cur_x--;
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
				}
			} else if (key == KEY_RIGHT) {
				if (can_place(cur_piece, cur_rot, cur_x + 1, cur_y)) {
					cur_x++;
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
				}
			} else if (key == KEY_UP) {
				int next_rot = (cur_rot + 1) % 4;
				/* Basic Wall Kick Tests */
				if (can_place(cur_piece, next_rot, cur_x, cur_y)) {
					cur_rot = next_rot;
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
				} else if (can_place(cur_piece, next_rot, cur_x + 1, cur_y)) {
					cur_x++;
					cur_rot = next_rot;
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
				} else if (can_place(cur_piece, next_rot, cur_x - 1, cur_y)) {
					cur_x--;
					cur_rot = next_rot;
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
				} else if (cur_piece == 0 && can_place(cur_piece, next_rot, cur_x + 2, cur_y)) {
					cur_x += 2;
					cur_rot = next_rot;
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
				} else if (cur_piece == 0 && can_place(cur_piece, next_rot, cur_x - 2, cur_y)) {
					cur_x -= 2;
					cur_rot = next_rot;
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
				}
			} else if (key == KEY_DOWN) {
				/* Soft Drop (+1 point) */
				if (can_place(cur_piece, cur_rot, cur_x, cur_y + 1)) {
					cur_y++;
					score += 1;
					if (score > session_high_score) session_high_score = score;
					last_drop_time = system_millis;
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
				}
			} else if (key == ' ') {
				/* Hard Drop / Slam */
				int gy = get_ghost_y(cur_piece, cur_rot, cur_x, cur_y);
				int dropped = gy - cur_y;
				cur_y = gy;
				score += dropped * 2;
				if (score > session_high_score) session_high_score = score;

				/* Lock active piece into board */
				bool top_out = false;
				for (int i = 0; i < 4; i++) {
					int bx = cur_x + TETROMINOES[cur_piece][cur_rot][i].x;
					int by = cur_y + TETROMINOES[cur_piece][cur_rot][i].y;
					if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
						board[by][bx] = (uint8_t)(cur_piece + 1);
					}
					if (by <= 1) {
						top_out = true;
					}
				}

				/* Line clear and scoring */
				int clr = clear_full_lines();
				if (clr > 0) {
					static const uint32_t line_scores[5] = {0, 100, 300, 500, 800};
					lines += clr;
					score += line_scores[clr] * level;
					level = 1 + (lines / 10);
					if (score > session_high_score) session_high_score = score;
					top_out = false;
				}

				/* Spawn next piece */
				cur_piece = next_piece;
				next_piece = get_next_bag_piece();
				cur_rot = 0;
				cur_x = 3;
				cur_y = 0;
				last_drop_time = system_millis;

				if (top_out || !can_place(cur_piece, cur_rot, cur_x, cur_y)) {
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, true);
					end_game_and_exit(score, lines, level);
					return;
				}

				render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
			}

			key = fg_key_get();
		}

		/* Gravity Step */
		uint32_t now = system_millis;
		uint32_t drop_interval = get_drop_interval_ms(level);

		if ((now - last_drop_time) >= drop_interval) {
			last_drop_time = now;
			if (can_place(cur_piece, cur_rot, cur_x, cur_y + 1)) {
				cur_y++;
				render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
			} else {
				/* Lock piece */
				bool top_out = false;
				for (int i = 0; i < 4; i++) {
					int bx = cur_x + TETROMINOES[cur_piece][cur_rot][i].x;
					int by = cur_y + TETROMINOES[cur_piece][cur_rot][i].y;
					if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
						board[by][bx] = (uint8_t)(cur_piece + 1);
					}
					if (by <= 1) {
						top_out = true;
					}
				}

				/* Check lines */
				int clr = clear_full_lines();
				if (clr > 0) {
					static const uint32_t line_scores[5] = {0, 100, 300, 500, 800};
					lines += clr;
					score += line_scores[clr] * level;
					level = 1 + (lines / 10);
					if (score > session_high_score) session_high_score = score;
					top_out = false;
				}

				/* Spawn next piece */
				cur_piece = next_piece;
				next_piece = get_next_bag_piece();
				cur_rot = 0;
				cur_x = 3;
				cur_y = 0;

				if (top_out || !can_place(cur_piece, cur_rot, cur_x, cur_y)) {
					render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, true);
					end_game_and_exit(score, lines, level);
					return;
				}

				render_screen(cur_piece, cur_rot, cur_x, cur_y, next_piece, score, lines, level, false);
			}
		}

		/* Yield 20 ms to OS kernel for smooth cooperative multitasking */
		mdelay(20);
	}
}
