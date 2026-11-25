#include <shell.h>
#include <console.h>
#include <scheduler.h>
#include <heap.h>
#include <usb_cdc.h>
#include <string.h>
#include <pwm.h>
#include <task.h>

/* Application Registry Table */
static const app_reg_t app_registry[] = {
	{ "breathe",  task_breathe_led, true,  "Hardware PWM breathing LED on GPIO 25" },
	{ "display",  task_display,     false, "Interactive counter display (Ctrl+C to stop)" },
	{ "hello",    task_hello,       false, "One-shot Hello World task (runs once and exits)" },
	{ "coremark", task_coremark,    false, "EEMBC CoreMark 1.0 benchmark (2000 iters)" },
	{ "tetris",   task_tetris,      false, "Classic ANSI Color Tetris Game (Arrows + Space)" },
};
#define NUM_APPS  (sizeof(app_registry) / sizeof(app_registry[0]))

#define SHELL_MAX_LINE 64

/* Active foreground task PID (0 = no foreground task, shell has focus) */
static volatile uint32_t foreground_pid = 0;

/* Foreground Key Input FIFO Buffer */
static int fg_key_fifo[16];
static volatile uint8_t fg_key_head = 0;
static volatile uint8_t fg_key_tail = 0;

void fg_key_put(int key)
{
	uint8_t next = (fg_key_head + 1) % 16;
	if (next != fg_key_tail) {
		fg_key_fifo[fg_key_head] = key;
		fg_key_head = next;
	}
}

int fg_key_get(void)
{
	if (fg_key_head == fg_key_tail) {
		return -1;
	}
	int key = fg_key_fifo[fg_key_tail];
	fg_key_tail = (fg_key_tail + 1) % 16;
	return key;
}

/* Active shell input line buffer and telemetry tracking */
static char shell_line[SHELL_MAX_LINE];
static uint32_t shell_len = 0;
static uint32_t cursor_idx = 0;
static volatile uint8_t shell_executing = 0;
static volatile uint8_t bg_telemetry_rendered = 0;

/* Command History Ring Buffer (16 previous commands) */
#define SHELL_HISTORY_MAX 16
static char shell_history[SHELL_HISTORY_MAX][SHELL_MAX_LINE];
static uint8_t history_count = 0;
static uint8_t history_head = 0;
static int8_t  history_nav_idx = -1;
static char    saved_line_buf[SHELL_MAX_LINE];

/* Registered Shell Commands for Tab Auto-Completion */
static const char *const shell_commands[] = {
	"help",
	"ls",
	"run",
	"fg",
	"ps",
	"kill",
	"heap",
	"clear",
	"history"
};
#define NUM_COMMANDS (sizeof(shell_commands) / sizeof(shell_commands[0]))

static void print_uint(uint32_t val);

/* Replace current terminal line on screen and update line buffer */
static void shell_replace_line(const char *new_str)
{
	/* Move cursor to end of current line if needed */
	while (cursor_idx < shell_len) {
		console_puts("\033[C");
		cursor_idx++;
	}
	/* Erase line backwards */
	while (shell_len > 0) {
		console_puts("\b \b");
		shell_len--;
	}
	cursor_idx = 0;

	/* Copy new string into line buffer */
	uint32_t len = strlen(new_str);
	if (len >= SHELL_MAX_LINE) len = SHELL_MAX_LINE - 1;
	memcpy(shell_line, new_str, len);
	shell_line[len] = '\0';
	shell_len = len;
	cursor_idx = len;
	console_puts(shell_line);
}

/* Save a command line into the history ring buffer */
static void history_add(const char *cmd)
{
	while (*cmd == ' ' || *cmd == '\t') cmd++;
	if (*cmd == '\0') return;

	/* Suppress consecutive duplicates */
	if (history_count > 0) {
		uint8_t last = (history_head - 1 + SHELL_HISTORY_MAX) % SHELL_HISTORY_MAX;
		if (strcmp(shell_history[last], cmd) == 0) {
			history_nav_idx = -1;
			return;
		}
	}

	strncpy(shell_history[history_head], cmd, SHELL_MAX_LINE - 1);
	shell_history[history_head][SHELL_MAX_LINE - 1] = '\0';
	history_head = (history_head + 1) % SHELL_HISTORY_MAX;
	if (history_count < SHELL_HISTORY_MAX) {
		history_count++;
	}
	history_nav_idx = -1;
}

/* Cycle backwards to older commands */
static void handle_history_up(void)
{
	if (history_count == 0) return;

	if (history_nav_idx == -1) {
		/* Save user's in-progress typing buffer */
		shell_line[shell_len] = '\0';
		strncpy(saved_line_buf, shell_line, SHELL_MAX_LINE - 1);
		saved_line_buf[SHELL_MAX_LINE - 1] = '\0';
		history_nav_idx = 0;
	} else if (history_nav_idx < (int8_t)(history_count - 1)) {
		history_nav_idx++;
	} else {
		return; /* Already at oldest command */
	}

	uint8_t slot = (history_head - 1 - history_nav_idx + SHELL_HISTORY_MAX * 2) % SHELL_HISTORY_MAX;
	shell_replace_line(shell_history[slot]);
}

/* Cycle forwards to newer commands */
static void handle_history_down(void)
{
	if (history_nav_idx == -1) return;

	if (history_nav_idx > 0) {
		history_nav_idx--;
		uint8_t slot = (history_head - 1 - history_nav_idx + SHELL_HISTORY_MAX * 2) % SHELL_HISTORY_MAX;
		shell_replace_line(shell_history[slot]);
	} else {
		/* Reached newest entry: restore user's in-progress typing buffer */
		history_nav_idx = -1;
		shell_replace_line(saved_line_buf);
	}
}

/* Print command history list */
static void cmd_history(void)
{
	if (history_count == 0) {
		console_puts("Command history is empty.\r\n");
		return;
	}
	console_puts("COMMAND HISTORY:\r\n");
	uint8_t start = (history_head - history_count + SHELL_HISTORY_MAX) % SHELL_HISTORY_MAX;
	for (uint8_t i = 0; i < history_count; i++) {
		uint8_t idx = (start + i) % SHELL_HISTORY_MAX;
		console_puts("  ");
		print_uint((uint32_t)(i + 1));
		if ((i + 1) < 10) console_puts("   ");
		else console_puts("  ");
		console_puts(shell_history[idx]);
		console_puts("\r\n");
	}
}

/* Handle Tab Auto-Completion */
static void handle_tab_completion(void)
{
	shell_line[shell_len] = '\0';

	/* 1. Sub-command completion for 'run <app>' */
	if (strncmp(shell_line, "run ", 4) == 0) {
		const char *prefix = shell_line + 4;
		while (*prefix == ' ') prefix++;
		uint32_t prefix_len = strlen(prefix);

		const char *matches[NUM_APPS];
		uint32_t match_count = 0;
		for (uint32_t i = 0; i < NUM_APPS; i++) {
			if (strncmp(app_registry[i].name, prefix, prefix_len) == 0) {
				matches[match_count++] = app_registry[i].name;
			}
		}

		if (match_count == 1) {
			char new_cmd[SHELL_MAX_LINE];
			strcpy(new_cmd, "run ");
			strcat(new_cmd, matches[0]);
			strcat(new_cmd, " ");
			shell_replace_line(new_cmd);
		} else if (match_count > 1) {
			console_puts("\r\n");
			for (uint32_t i = 0; i < match_count; i++) {
				console_puts("  ");
				console_puts(matches[i]);
			}
			console_puts("\r\n");
			shell_prompt();
			console_puts(shell_line);
		}
		return;
	}

	/* 2. Sub-command completion for 'fg <app>' */
	if (strncmp(shell_line, "fg ", 3) == 0) {
		const char *prefix = shell_line + 3;
		while (*prefix == ' ') prefix++;
		uint32_t prefix_len = strlen(prefix);

		const char *matches[NUM_APPS];
		uint32_t match_count = 0;
		for (uint32_t i = 0; i < NUM_APPS; i++) {
			if (strncmp(app_registry[i].name, prefix, prefix_len) == 0) {
				matches[match_count++] = app_registry[i].name;
			}
		}

		if (match_count == 1) {
			char new_cmd[SHELL_MAX_LINE];
			strcpy(new_cmd, "fg ");
			strcat(new_cmd, matches[0]);
			strcat(new_cmd, " ");
			shell_replace_line(new_cmd);
		} else if (match_count > 1) {
			console_puts("\r\n");
			for (uint32_t i = 0; i < match_count; i++) {
				console_puts("  ");
				console_puts(matches[i]);
			}
			console_puts("\r\n");
			shell_prompt();
			console_puts(shell_line);
		}
		return;
	}

	/* 3. Sub-command completion for 'heap audit' */
	if (strncmp(shell_line, "heap ", 5) == 0) {
		const char *prefix = shell_line + 5;
		while (*prefix == ' ') prefix++;
		if (strncmp("audit", prefix, strlen(prefix)) == 0) {
			shell_replace_line("heap audit");
		}
		return;
	}

	/* 4. Primary Command completion */
	if (strchr(shell_line, ' ') == NULL) {
		const char *matches[NUM_COMMANDS];
		uint32_t match_count = 0;
		for (uint32_t i = 0; i < NUM_COMMANDS; i++) {
			if (strncmp(shell_commands[i], shell_line, shell_len) == 0) {
				matches[match_count++] = shell_commands[i];
			}
		}

		if (match_count == 1) {
			char new_cmd[SHELL_MAX_LINE];
			strcpy(new_cmd, matches[0]);
			strcat(new_cmd, " ");
			shell_replace_line(new_cmd);
		} else if (match_count > 1) {
			console_puts("\r\n");
			for (uint32_t i = 0; i < match_count; i++) {
				console_puts("  ");
				console_puts(matches[i]);
			}
			console_puts("\r\n");
			shell_prompt();
			console_puts(shell_line);
		}
	}
}

bool shell_is_foreground(void)
{
	return (foreground_pid != 0 && current_task != NULL && current_task->pid == foreground_pid);
}

static bool is_task_bg_running(const char *name)
{
	tcb *head = task_get_queue_head();
	if (head == NULL) return false;
	tcb *curr = head;
	do {
		if (strcmp(curr->name, name) == 0 && curr->pid != foreground_pid && curr->state != t_quit) {
			return true;
		}
		curr = curr->next;
	} while (curr != head && curr != NULL);
	return false;
}

void shell_prompt(void)
{
	if (is_task_bg_running("display")) {
		console_puts("[LIVE TELEMETRY] display: Hello from display task! [");
		console_putc(display_get_spinner());
		console_puts("] Tick: ");
		print_uint(display_get_tick());
		if (is_task_bg_running("breathe")) {
			console_puts(" | breathe: PWM (2 kHz)");
		}
		console_puts("\r\n");
		bg_telemetry_rendered = 1;
	} else if (is_task_bg_running("breathe")) {
		console_puts("[LIVE TELEMETRY] breathe: Hardware PWM Breathing on GPIO 25 (2 kHz)\r\n");
		bg_telemetry_rendered = 1;
	} else {
		bg_telemetry_rendered = 0;
	}
	console_puts("halo> ");
}

void shell_update_bg_telemetry(char spinner, uint32_t ticks)
{
	if (!usb_cdc_is_connected() || console_is_locked()) return;
	if (foreground_pid > 0 || shell_executing) return;
	if (!is_task_bg_running("display")) return;

	console_lock();

	if (bg_telemetry_rendered) {
		/* Move to column 0 of prompt line and clear it */
		console_puts("\r\033[K");
		/* Move UP 1 line to the telemetry line */
		console_puts("\033[1A");
		/* Move to column 0 of telemetry line and clear it */
		console_puts("\r\033[K");
	} else {
		/* First render: clear prompt line */
		console_puts("\r\033[K");
	}

	/* Draw updated telemetry */
	console_puts("[LIVE TELEMETRY] display: Hello from display task! [");
	console_putc(spinner);
	console_puts("] Tick: ");
	print_uint(ticks);
	if (is_task_bg_running("breathe")) {
		console_puts(" | breathe: PWM (2 kHz)");
	}
	console_puts("\r\n");

	/* Re-render prompt and restore user input buffer */
	console_puts("halo> ");
	for (uint32_t i = 0; i < shell_len; i++) {
		console_putc(shell_line[i]);
	}
	for (uint32_t i = cursor_idx; i < shell_len; i++) {
		console_puts("\033[D");
	}

	bg_telemetry_rendered = 1;

	console_unlock();
}

static void print_uint(uint32_t val)
{
	char buf[12];
	int i = 0;
	if (val == 0) {
		console_putc('0');
		return;
	}
	while (val > 0) {
		buf[i++] = '0' + (val % 10);
		val /= 10;
	}
	while (i > 0) {
		console_putc(buf[--i]);
	}
}

static void print_hex(uint32_t val)
{
	static const char hex[] = "0123456789ABCDEF";
	console_puts("0x");
	for (int i = 7; i >= 0; i--) {
		console_putc(hex[(val >> (i * 4)) & 0xF]);
	}
}

static const char *state_to_str(status s)
{
	switch (s) {
	case t_ready: return "READY  ";
	case t_busy:  return "RUNNING";
	case t_quit:  return "QUIT   ";
	default:      return "UNKNOWN";
	}
}

static const char *get_task_mode_str(uint32_t pid)
{
	if (pid == 0) return "SYSTEM    ";
	if (pid == foreground_pid) return "FOREGROUND";
	return "BACKGROUND";
}

static void cmd_help(void)
{
	console_puts("Available commands:\r\n");
	console_puts("  help        - Display this command menu\r\n");
	console_puts("  ls          - List all applications in the App Registry\r\n");
	console_puts("  run <app> [&]- Dynamically spawn app on heap (& forces background execution)\r\n");
	console_puts("  fg [id|app] - Bring background task to foreground (Ctrl+Z: bg, Ctrl+C: stop)\r\n");
	console_puts("  ps          - Traverse dynamic task queue, stack frames & heap memory\r\n");
	console_puts("  kill <id|app>- Terminate task by PID or Name, or 'kill all'\r\n");
	console_puts("  heap        - Display dynamic heap allocation statistics\r\n");
	console_puts("  heap audit  - Full physical block audit and memory payload dump\r\n");
	console_puts("  history     - Display command execution history\r\n");
	console_puts("  clear       - Clear terminal screen\r\n");
}

static void cmd_ls(void)
{
	console_puts("AVAILABLE APPLICATIONS IN REGISTRY:\r\n");
	console_puts("NAME       MODE         DESCRIPTION\r\n");
	console_puts("-----------------------------------------------------------\r\n");
	for (uint32_t i = 0; i < NUM_APPS; i++) {
		console_puts(app_registry[i].name);
		int pad = 11 - strlen(app_registry[i].name);
		while (pad-- > 0) console_putc(' ');

		if (app_registry[i].bg) {
			console_puts("BACKGROUND   ");
		} else {
			console_puts("FOREGROUND   ");
		}
		console_puts(app_registry[i].description);
		console_puts("\r\n");
	}
}

static void cmd_run(const char *arg)
{
	while (*arg == ' ' || *arg == '\t') arg++;
	if (*arg == '\0') {
		console_puts("[ERROR] Usage: run <app_name> [&]\r\n");
		console_puts("Type 'ls' to see available applications in the registry.\r\n");
		return;
	}

	char name_buf[32];
	strncpy(name_buf, arg, sizeof(name_buf) - 1);
	name_buf[sizeof(name_buf) - 1] = '\0';

	/* Detect and strip trailing '&' */
	int force_bg = 0;
	int len = strlen(name_buf);
	while (len > 0 && (name_buf[len - 1] == ' ' || name_buf[len - 1] == '\t')) {
		name_buf[--len] = '\0';
	}
	if (len > 0 && name_buf[len - 1] == '&') {
		force_bg = 1;
		name_buf[--len] = '\0';
		while (len > 0 && (name_buf[len - 1] == ' ' || name_buf[len - 1] == '\t')) {
			name_buf[--len] = '\0';
		}
	}

	/* Search for application in the registry */
	const app_reg_t *app = 0;
	for (uint32_t i = 0; i < NUM_APPS; i++) {
		if (strcmp(name_buf, app_registry[i].name) == 0) {
			app = &app_registry[i];
			break;
		}
	}

	if (!app) {
		console_puts("[ERROR] Application '");
		console_puts(name_buf);
		console_puts("' not found in registry. Type 'ls' to list apps.\r\n");
		return;
	}

	/* Check if already running in dynamic queue */
	tcb *head = task_get_queue_head();
	if (head) {
		tcb *curr = head;
		do {
			if (strcmp(curr->name, app->name) == 0 && curr->state != t_quit) {
				console_puts("[BUSY] '");
				console_puts(app->name);
				console_puts("' is already running as PID ");
				print_uint(curr->pid);
				console_puts("! Use 'kill ");
				console_puts(app->name);
				console_puts("' to stop it first.\r\n");
				return;
			}
			curr = curr->next;
		} while (curr != head);
	}

	bool is_bg = app->bg || force_bg;

	/* Dynamically allocate TCB and Stack on the Heap! (3072 B for CoreMark, 2048 B for Tetris, 1024 B default) */
	uint32_t stack_sz = DEFAULT_STACK_SIZE_BYTES;
	if (strcmp(app->name, "coremark") == 0) {
		stack_sz = 3 * 1024;
	} else if (strcmp(app->name, "tetris") == 0) {
		stack_sz = 2048;
	}
	int pid = task_create_dynamic(app->name, app->entry, stack_sz);
	if (pid < 0) {
		console_puts("[ERROR] Failed to spawn: Heap exhausted! Free heap: ");
		print_uint((uint32_t)heap_get_free_bytes());
		console_puts(" bytes.\r\n");
		return;
	}

	if (is_bg) {
		console_puts("[OK] Spawned '");
		console_puts(app->name);
		console_puts("' dynamically in background with PID ");
		print_uint((uint32_t)pid);
		if (force_bg && !app->bg) {
			console_puts(" (forced by '&')");
		}
		console_puts(".\r\n");
		if (strcmp(app->name, "tetris") == 0) {
			console_puts("Type 'fg' or 'fg tetris' to bring to foreground.\r\n");
		}
	} else {
		foreground_pid = (uint32_t)pid;
		console_puts("[RUN] Running '");
		console_puts(app->name);
		console_puts("' in foreground. (Ctrl+Z: bg, Ctrl+C: stop)...\r\n");
	}
}

static void cmd_fg(const char *arg)
{
	while (*arg == ' ' || *arg == '\t') arg++;

	tcb *target = NULL;
	tcb *head = task_get_queue_head();
	if (!head) {
		console_puts("[ERROR] No running tasks found.\r\n");
		return;
	}

	if (*arg == '\0') {
		/* No argument: pick background task (prioritize interactive tasks like tetris) */
		tcb *curr = head->next; /* head is shell (PID 0) */
		while (curr && curr != head) {
			if (curr->pid != 0 && curr->pid != foreground_pid && curr->state != t_quit) {
				target = curr;
				if (strcmp(curr->name, "tetris") == 0 || strcmp(curr->name, "display") == 0) {
					break;
				}
			}
			curr = curr->next;
		}
	} else if (arg[0] >= '0' && arg[0] <= '9') {
		/* Search by PID */
		uint32_t pid = 0;
		const char *p = arg;
		while (*p >= '0' && *p <= '9') {
			pid = pid * 10 + (*p - '0');
			p++;
		}
		if (pid == 0) {
			console_puts("[ERROR] Shell (PID 0) is already the interactive shell!\r\n");
			return;
		}
		target = task_get_by_pid(pid);
	} else {
		/* Search by Name */
		tcb *curr = head->next;
		while (curr && curr != head) {
			if (strcmp(curr->name, arg) == 0 && curr->pid != 0 && curr->state != t_quit) {
				target = curr;
				break;
			}
			curr = curr->next;
		}
	}

	if (!target || target->state == t_quit || target->pid == 0) {
		console_puts("[ERROR] No background task matching '");
		console_puts(*arg ? arg : "(any)");
		console_puts("' found to bring to foreground.\r\n");
		return;
	}

	/* Bring target task to foreground */
	foreground_pid = target->pid;
	fg_key_head = fg_key_tail = 0;

	console_puts("[FG] Switched task '");
	console_puts(target->name);
	console_puts("' (PID ");
	print_uint(target->pid);
	console_puts(") to FOREGROUND. (Ctrl+Z: bg, Ctrl+C: stop)\r\n");
}

static void cmd_ps(void)
{
	console_puts("PID     NAME       STATE    MODE        STACK_SP    TELEMETRY\r\n");
	console_puts("--------------------------------------------------------------------------------\r\n");

	tcb *head = task_get_queue_head();
	if (head != NULL) {
		tcb *curr = head;
		do {
			if (curr->state != t_quit) {
				print_uint(curr->pid);
				uint32_t temp = curr->pid;
				int digits = (temp == 0) ? 1 : 0;
				while (temp > 0) { digits++; temp /= 10; }
				int pad_pid = 8 - digits;
				if (pad_pid < 1) pad_pid = 1;
				while (pad_pid-- > 0) console_putc(' ');

				console_puts(curr->name);
				int pad = 11 - strlen(curr->name);
				while (pad-- > 0) console_putc(' ');

				console_puts(state_to_str(curr->state));
				console_puts("  ");
				console_puts(get_task_mode_str(curr->pid));
				console_puts("  ");
				print_hex(curr->stack_pointer_end);
				console_puts("  ");

				if (strcmp(curr->name, "shell") == 0) {
					console_puts("Active Shell (Queue Head)");
				} else if (strcmp(curr->name, "breathe") == 0) {
					console_puts("PWM Breathing (2 kHz)");
				} else if (strcmp(curr->name, "display") == 0) {
					console_puts("Tick: ");
					print_uint(display_get_tick());
					console_puts(" [");
					console_putc(display_get_spinner());
					console_puts("]");
				} else if (strcmp(curr->name, "hello") == 0) {
					console_puts("One-Shot Done");
				} else if (strcmp(curr->name, "coremark") == 0) {
					console_puts("CoreMark Benchmark");
				} else if (strcmp(curr->name, "tetris") == 0) {
					console_puts("ANSI Tetris Engine");
				}
				console_puts("\r\n");
			}
			curr = curr->next;
		} while (curr != head && curr != NULL);
	}
	console_puts("--------------------------------------------------------------------------------\r\n");
	console_puts("Heap Status: ");
	print_uint((uint32_t)heap_get_free_bytes());
	console_puts(" / ");
	print_uint((uint32_t)heap_get_total_bytes());
	console_puts(" bytes free (Active Queue Tasks: ");
	print_uint(task_get_count());
	console_puts(")\r\n");
}

/* Helper to terminate a single active task by Name or PID */
static int kill_single_task(const char *arg)
{
	if (strcmp(arg, "shell") == 0 || strcmp(arg, "0") == 0) {
		console_puts("[ERROR] Cannot kill root system shell task!\r\n");
		return -1;
	}

	uint32_t target_pid = 0;
	int is_pid_num = 1;
	for (const char *p = arg; *p; p++) {
		if (*p < '0' || *p > '9') {
			is_pid_num = 0;
			break;
		}
	}

	tcb *target = NULL;
	tcb *head = task_get_queue_head();
	if (head) {
		tcb *curr = head;
		do {
			if (curr->state != t_quit) {
				if (is_pid_num) {
					uint32_t val = (uint32_t)atoi(arg);
					if (curr->pid == val) {
						target = curr;
						break;
					}
				} else {
					if (strcmp(curr->name, arg) == 0) {
						target = curr;
						break;
					}
				}
			}
			curr = curr->next;
		} while (curr != head);
	}

	if (!target) {
		console_puts("[ERROR] Task '");
		console_puts(arg);
		console_puts("' is not currently running!\r\n");
		return -1;
	}

	/* Reset hardware if needed */
	if (strcmp(target->name, "breathe") == 0) {
		pwm_set(0);
	}

	target_pid = target->pid;
	if (target_pid == foreground_pid) {
		foreground_pid = 0;
		fg_key_head = fg_key_tail = 0;
	}
	char target_name[16];
	strncpy(target_name, target->name, sizeof(target_name) - 1);
	target_name[sizeof(target_name) - 1] = '\0';

	task_kill(target_pid);

	console_puts("[OK] Terminated task '");
	console_puts(target_name);
	console_puts("' (PID ");
	print_uint(target_pid);
	console_puts("). Memory reclaimed.\r\n");

	return 0;
}

static void cmd_kill(const char *arg)
{
	while (*arg == ' ' || *arg == '\t') arg++;
	if (*arg == '\0') {
		console_puts("[ERROR] Usage: kill <name|pid> | kill all\r\n");
		return;
	}

	if (strcmp(arg, "all") == 0) {
		int killed_count = 0;
		tcb *head = task_get_queue_head();
		if (head) {
			tcb *curr = head->next; /* Head is shell (PID 0), strictly protected */
			while (curr && curr != head) {
				tcb *next = curr->next;
				if (curr->state != t_quit && curr->pid != 0) {
					char app_name[16];
					strncpy(app_name, curr->name, sizeof(app_name) - 1);
					app_name[sizeof(app_name) - 1] = '\0';
					kill_single_task(app_name);
					killed_count++;
				}
				curr = next;
			}
		}
		if (killed_count == 0) {
			console_puts("[INFO] No running tasks to terminate.\r\n");
		}
		return;
	}

	kill_single_task(arg);
}

static void cmd_heap(const char *arg)
{
	while (*arg == ' ' || *arg == '\t') arg++;
	if (strcmp(arg, "audit") == 0 || strcmp(arg, "dump") == 0) {
		heap_audit();
		return;
	}

	heap_stats_t st;
	heap_get_stats(&st);

	console_puts("DYNAMIC HEAP STATISTICS:\r\n");
	console_puts("--------------------------------------------------\r\n");
	console_puts("Total Pool Size   : ");
	console_print_dec((uint32_t)st.total_bytes);
	console_puts(" bytes (16.0 KB)\r\n");

	console_puts("Free Space        : ");
	console_print_dec((uint32_t)st.free_bytes);
	console_puts(" bytes\r\n");

	console_puts("Used Space        : ");
	console_print_dec((uint32_t)st.used_bytes);
	console_puts(" bytes\r\n");

	console_puts("Total Blocks      : ");
	console_print_dec((uint32_t)st.total_blocks);
	console_puts(" (Active: ");
	console_print_dec((uint32_t)st.used_blocks);
	console_puts(", Free: ");
	console_print_dec((uint32_t)st.free_blocks);
	console_puts(")\r\n");

	console_puts("Largest Free Block: ");
	console_print_dec((uint32_t)st.largest_free_block);
	console_puts(" bytes\r\n");
	console_puts("--------------------------------------------------\r\n");
	console_puts("Tip: Use 'heap audit' to dump block chains and payload bytes.\r\n");
}

static void execute_command(char *cmd_line)
{
	while (*cmd_line == ' ' || *cmd_line == '\t') cmd_line++;
	if (*cmd_line == '\0') return;

	char *arg = cmd_line;
	while (*arg != '\0' && *arg != ' ' && *arg != '\t') {
		arg++;
	}
	if (*arg != '\0') {
		*arg = '\0';
		arg++;
		while (*arg == ' ' || *arg == '\t') arg++;
	}

	if (strcmp(cmd_line, "help") == 0) {
		cmd_help();
	} else if (strcmp(cmd_line, "ls") == 0) {
		cmd_ls();
	} else if (strcmp(cmd_line, "run") == 0) {
		cmd_run(arg);
	} else if (strcmp(cmd_line, "fg") == 0) {
		cmd_fg(arg);
	} else if (strcmp(cmd_line, "ps") == 0) {
		cmd_ps();
	} else if (strcmp(cmd_line, "kill") == 0) {
		cmd_kill(arg);
	} else if (strcmp(cmd_line, "heap") == 0) {
		cmd_heap(arg);
	} else if (strcmp(cmd_line, "clear") == 0) {
		console_write("\033[2J\033[H", 7);
	} else if (strcmp(cmd_line, "history") == 0) {
		cmd_history();
	} else {
		console_puts("Unknown command: '");
		console_puts(cmd_line);
		console_puts("'. Type 'help' for available commands.\r\n");
	}
}

void shell_task(void)
{
	int welcomed = 0;

	while (1) {
		if (!usb_cdc_is_connected()) {
			welcomed = 0;
			foreground_pid = 0;
			mdelay(50);
			continue;
		}

		if (!welcomed) {
			welcomed = 1;
			mdelay(25); /* DTR settling delay */
			console_puts("\r\n>>> CONNECTED TO STRATAHALO OS (DYNAMIC QUEUE) <<<\r\n");
			console_puts("Type 'help' or 'ls' to see applications.\r\n\r\n");
			shell_len = 0;
			cursor_idx = 0;
			shell_prompt();
		}

		/* If a foreground task was running, check if it has exited on its own */
		if (foreground_pid > 0) {
			tcb *fg = task_get_by_pid(foreground_pid);
			if (!fg || fg->state == t_quit) {
				foreground_pid = 0;
				fg_key_head = fg_key_tail = 0;
				shell_len = 0;
				cursor_idx = 0;
				console_puts("\033[?25h\033[0m"); /* Restore cursor and normal attributes */
				shell_prompt();
			}
		}

		/* Non-blocking read */
		int ch = console_getc();
		if (ch < 0) {
			if (current_task != NULL && task_get_count() > 1) {
				task_yield();
			} else {
				mdelay(10);
			}
			continue;
		}

		/* Intercept Ctrl+C (ASCII 0x03) */
		if (ch == 0x03) {
			if (foreground_pid > 0) {
				tcb *fg = task_get_by_pid(foreground_pid);
				if (fg && fg->state != t_quit) {
					char killed_name[16];
					strncpy(killed_name, fg->name, sizeof(killed_name) - 1);
					killed_name[sizeof(killed_name) - 1] = '\0';

					console_puts("\033[?25h\033[0m\r\n^C\r\n[STOP] Terminated foreground task '");
					console_puts(killed_name);
					console_puts("' (PID ");
					print_uint(foreground_pid);
					console_puts(").\r\n");

					if (strcmp(killed_name, "breathe") == 0) {
						pwm_set(0);
					}

					task_kill(foreground_pid);
				}
				foreground_pid = 0;
				fg_key_head = fg_key_tail = 0;
				shell_len = 0;
				cursor_idx = 0;
				shell_prompt();
			} else {
				console_puts("^C\r\n");
				shell_len = 0;
				cursor_idx = 0;
				shell_prompt();
			}
			continue;
		}

		/* Intercept Ctrl+Z (ASCII 0x1A): Suspend/Background foreground task */
		if (ch == 0x1A) {
			if (foreground_pid > 0) {
				tcb *fg = task_get_by_pid(foreground_pid);
				if (fg && fg->state != t_quit) {
					char bg_name[16];
					strncpy(bg_name, fg->name, sizeof(bg_name) - 1);
					bg_name[sizeof(bg_name) - 1] = '\0';

					console_puts("\033[?25h\033[0m\r\n^Z\r\n[BG] Backgrounded task '");
					console_puts(bg_name);
					console_puts("' (PID ");
					print_uint(foreground_pid);
					console_puts("). Type 'fg' to resume.\r\n");
				}
				foreground_pid = 0;
				fg_key_head = fg_key_tail = 0;
				shell_len = 0;
				cursor_idx = 0;
				shell_prompt();
			}
			continue;
		}

		/* If a foreground task is actively running, pipe input key to foreground FIFO */
		if (foreground_pid > 0) {
			fg_key_put(ch);
			if (current_task != NULL && task_get_count() > 1) {
				task_yield();
			}
			continue;
		}

		/* Normal shell line editing with full ergonomics */
		if (ch == '\r' || ch == '\n') {
			console_lock();
			console_puts("\r\n");
			shell_line[shell_len] = '\0';
			if (shell_len > 0) {
				history_add(shell_line);
			}
			history_nav_idx = -1;
			shell_executing = 1;
			execute_command(shell_line);
			shell_len = 0;
			cursor_idx = 0;
			shell_executing = 0;
			if (foreground_pid == 0) {
				shell_prompt();
			}
			console_unlock();
		} else if (ch == '\b' || ch == 0x7F) {
			/* Backspace */
			if (cursor_idx > 0) {
				console_lock();
				if (cursor_idx == shell_len) {
					cursor_idx--;
					shell_len--;
					shell_line[shell_len] = '\0';
					console_puts("\b \b");
				} else {
					/* Middle-of-line backspace */
					memmove(&shell_line[cursor_idx - 1], &shell_line[cursor_idx], shell_len - cursor_idx);
					cursor_idx--;
					shell_len--;
					shell_line[shell_len] = '\0';
					console_puts("\b");
					console_puts(&shell_line[cursor_idx]);
					console_putc(' ');
					for (uint32_t i = 0; i <= (shell_len - cursor_idx); i++) {
						console_puts("\033[D");
					}
				}
				console_unlock();
			}
		} else if (ch == KEY_UP) {
			console_lock();
			handle_history_up();
			console_unlock();
		} else if (ch == KEY_DOWN) {
			console_lock();
			handle_history_down();
			console_unlock();
		} else if (ch == '\t') {
			console_lock();
			handle_tab_completion();
			console_unlock();
		} else if (ch == 0x0C) {
			/* Ctrl+L: Clear screen, redraw prompt and current line buffer */
			console_lock();
			console_write("\033[2J\033[H", 7);
			shell_prompt();
			shell_line[shell_len] = '\0';
			console_puts(shell_line);
			if (cursor_idx < shell_len) {
				for (uint32_t i = 0; i < (shell_len - cursor_idx); i++) {
					console_puts("\033[D");
				}
			}
			console_unlock();
		} else if (ch == 0x15) {
			/* Ctrl+U: Erase current line */
			console_lock();
			shell_replace_line("");
			history_nav_idx = -1;
			console_unlock();
		} else if (ch == 0x01) {
			/* Ctrl+A: Move cursor to beginning of line */
			console_lock();
			while (cursor_idx > 0) {
				console_puts("\033[D");
				cursor_idx--;
			}
			console_unlock();
		} else if (ch == 0x05) {
			/* Ctrl+E: Move cursor to end of line */
			console_lock();
			while (cursor_idx < shell_len) {
				console_puts("\033[C");
				cursor_idx++;
			}
			console_unlock();
		} else if (ch == KEY_LEFT) {
			/* Left arrow cursor movement */
			if (cursor_idx > 0) {
				console_lock();
				console_puts("\033[D");
				cursor_idx--;
				console_unlock();
			}
		} else if (ch == KEY_RIGHT) {
			/* Right arrow cursor movement */
			if (cursor_idx < shell_len) {
				console_lock();
				console_puts("\033[C");
				cursor_idx++;
				console_unlock();
			}
		} else if (ch >= 32 && ch <= 126) {
			/* Printable character */
			if (shell_len < SHELL_MAX_LINE - 1) {
				console_lock();
				if (cursor_idx == shell_len) {
					shell_line[shell_len++] = (char)ch;
					cursor_idx++;
					shell_line[shell_len] = '\0';
					console_putc((char)ch);
				} else {
					/* Middle-of-line insertion */
					memmove(&shell_line[cursor_idx + 1], &shell_line[cursor_idx], shell_len - cursor_idx);
					shell_line[cursor_idx] = (char)ch;
					shell_len++;
					cursor_idx++;
					shell_line[shell_len] = '\0';
					console_puts(&shell_line[cursor_idx - 1]);
					for (uint32_t i = 0; i < (shell_len - cursor_idx); i++) {
						console_puts("\033[D");
					}
				}
				console_unlock();
			}
		}
	}
}
