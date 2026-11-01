#include <shell.h>
#include <console.h>
#include <scheduler.h>
#include <usb_cdc.h>
#include <string.h>
#include <pwm.h>

extern void task_breathe_led(void);
extern void task_display(void);
extern void task_hello(void);

/* Application Registry Table */
static const app_reg_t app_registry[] = {
	{ "breathe", task_breathe_led, 1, 0, "Hardware PWM breathing LED on GPIO 25" },
	{ "display", task_display,     0, 0, "Interactive counter display (Ctrl+C to stop)" },
	{ "hello",   task_hello,       0, 1, "One-shot Hello World task (runs once and exits)" },
};
#define NUM_APPS  (sizeof(app_registry) / sizeof(app_registry[0]))

#define SHELL_MAX_LINE 64

/* Active foreground task PID (0 = no foreground task, shell has focus) */
static volatile uint32_t foreground_pid = 0;

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
	case t_wait:  return "WAITING";
	case t_quit:  return "QUIT   ";
	default:      return "UNKNOWN";
	}
}

static const char *mode_to_str(task_mode_t m)
{
	switch (m) {
	case MODE_SYSTEM:     return "SYSTEM    ";
	case MODE_FOREGROUND: return "FOREGROUND";
	case MODE_BACKGROUND: return "BACKGROUND";
	default:              return "UNKNOWN   ";
	}
}

static void cmd_help(void)
{
	console_puts("Available commands:\r\n");
	console_puts("  help        - Display this command menu\r\n");
	console_puts("  ls          - List all applications in the App Registry\r\n");
	console_puts("  run <app> [&]- Run an app (& forces background execution)\r\n");
	console_puts("  ps          - List active tasks, states, modes, and stack pointers\r\n");
	console_puts("  kill <app..>- Terminate task(s) by name, or 'kill all'\r\n");
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

	/* Copy argument to local buffer to parse optional '&' */
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

	/* Check if already running */
	for (uint8_t i = 1; i < MAX_TASKS; i++) {
		if (strcmp(task[i].name, app->name) == 0 && task[i].state != t_quit && task[i].name[0] != '\0') {
			console_puts("[BUSY] '");
			console_puts(app->name);
			console_puts("' is already running as PID ");
			print_uint(task[i].pid);
			console_puts("! Use 'kill ");
			console_puts(app->name);
			console_puts("' to stop it first.\r\n");
			return;
		}
	}

	int is_bg = app->bg || force_bg;
	task_mode_t mode = is_bg ? MODE_BACKGROUND : MODE_FOREGROUND;
	int pid = task_create_named(app->name, app->entry, mode);
	if (pid < 0) {
		console_puts("[ERROR] Failed to spawn: task pool is full (max ");
		print_uint(MAX_APPS);
		console_puts(" apps running).\r\n");
		return;
	}

	if (is_bg) {
		console_puts("[OK] Spawned '");
		console_puts(app->name);
		console_puts("' in background with PID ");
		print_uint((uint32_t)pid);
		if (force_bg && !app->bg) {
			console_puts(" (forced by '&')");
		}
		console_puts(".\r\n");
	} else {
		foreground_pid = (uint32_t)pid;
		console_puts("[RUN] Running '");
		console_puts(app->name);
		if (app->is_oneshot) {
			console_puts("'...\r\n");
		} else {
			console_puts("' in foreground. Press Ctrl+C to stop...\r\n");
		}
	}
}

static void cmd_ps(void)
{
	console_puts("PID     NAME       STATE    MODE        STACK_SP\r\n");
	console_puts("------------------------------------------------\r\n");
	for (uint8_t i = 0; i < MAX_TASKS; i++) {
		if (task[i].state == t_quit || task[i].name[0] == '\0') {
			continue; /* Skip terminated / wiped slots */
		}

		/* Print PID with dynamic alignment */
		print_uint(task[i].pid);
		uint32_t temp = task[i].pid;
		int digits = (temp == 0) ? 1 : 0;
		while (temp > 0) {
			digits++;
			temp /= 10;
		}
		int pad_pid = 8 - digits;
		if (pad_pid < 1) pad_pid = 1;
		while (pad_pid-- > 0) console_putc(' ');

		console_puts(task[i].name);
		int pad = 11 - strlen(task[i].name);
		while (pad-- > 0) console_putc(' ');

		console_puts(state_to_str(task[i].state));
		console_puts("  ");
		console_puts(mode_to_str(task[i].mode));
		console_puts("  ");
		print_hex(task[i].stack_pointer_end);
		console_puts("\r\n");
	}
}

/* Helper to terminate a single active task by name */
static int kill_single_task(const char *name)
{
	/* Prevent root shell suicide */
	if (strcmp(name, "shell") == 0 || strcmp(name, "0") == 0) {
		console_puts("[ERROR] Cannot kill root system shell task!\r\n");
		return -1;
	}

	/* Search for active task by App Name */
	int target_slot = -1;
	for (int i = 1; i < MAX_TASKS; i++) {
		if (strcmp(task[i].name, name) == 0 && task[i].state != t_quit && task[i].name[0] != '\0') {
			target_slot = i;
			break;
		}
	}

	if (target_slot == -1) {
		console_puts("[ERROR] Task '");
		console_puts(name);
		console_puts("' is not currently running!\r\n");
		return -1;
	}

	/* Hardware peripheral cleanup: Turn off PWM if breathe task is killed */
	if (strcmp(name, "breathe") == 0) {
		pwm_set(0);
	}

	uint32_t killed_pid = task[target_slot].pid;
	task_kill(target_slot);

	console_puts("[OK] Terminated task '");
	console_puts(name);
	console_puts("' (PID ");
	print_uint(killed_pid);
	console_puts(").\r\n");

	return 0;
}

static void cmd_kill(const char *arg)
{
	while (*arg == ' ' || *arg == '\t') arg++;
	if (*arg == '\0') {
		console_puts("[ERROR] Usage: kill <app1> [app2 ...] | kill all\r\n");
		return;
	}

	/* Special case: 'kill all' terminates all active non-shell tasks */
	if (strcmp(arg, "all") == 0) {
		int killed_count = 0;
		/* Strictly scan slots 1 to MAX_TASKS - 1: Slot 0 (shell) is 100% PROTECTED! */
		for (int i = 1; i < MAX_TASKS; i++) {
			if (task[i].state != t_quit && task[i].name[0] != '\0') {
				char app_name[16];
				strncpy(app_name, task[i].name, sizeof(app_name) - 1);
				app_name[sizeof(app_name) - 1] = '\0';
				kill_single_task(app_name);
				killed_count++;
			}
		}
		if (killed_count == 0) {
			console_puts("[INFO] No background tasks currently running.\r\n");
		}
		return;
	}

	/* Tokenize multiple arguments separated strictly by whitespace (spaces or tabs) */
	const char *p = arg;
	char token[32];

	while (*p) {
		/* Skip whitespace delimiters */
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '\0') break;

		/* Extract next app name token */
		int tlen = 0;
		while (*p && *p != ' ' && *p != '\t' && tlen < (int)(sizeof(token) - 1)) {
			token[tlen++] = *p++;
		}
		token[tlen] = '\0';

		if (tlen > 0) {
			kill_single_task(token);
		}
	}
}

static void execute_command(char *cmd_line)
{
	/* Skip leading whitespace */
	while (*cmd_line == ' ' || *cmd_line == '\t') cmd_line++;
	if (*cmd_line == '\0') return;

	/* Find first argument space */
	char *space = cmd_line;
	while (*space && *space != ' ' && *space != '\t') space++;

	char *arg = space;
	if (*space != '\0') {
		*space = '\0';
		arg++;
	}

	if (strcmp(cmd_line, "help") == 0) {
		cmd_help();
	} else if (strcmp(cmd_line, "ls") == 0) {
		cmd_ls();
	} else if (strcmp(cmd_line, "run") == 0) {
		cmd_run(arg);
	} else if (strcmp(cmd_line, "ps") == 0) {
		cmd_ps();
	} else if (strcmp(cmd_line, "kill") == 0) {
		cmd_kill(arg);
	} else {
		console_puts("[ERROR] Unknown command '");
		console_puts(cmd_line);
		console_puts("'. Type 'help' for available commands.\r\n");
	}
}

void shell_task(void)
{
	char line[SHELL_MAX_LINE];
	uint32_t len = 0;
	int welcomed = 0;

	while (1) {
		/* Wait for active USB connection */
		if (!usb_cdc_is_connected()) {
			welcomed = 0;
			foreground_pid = 0;
			mdelay(50);
			continue;
		}

		if (!welcomed) {
			welcomed = 1;
			console_puts("\r\n>>> CONNECTED TO STRATAHALO OS <<<\r\n");
			console_puts("Type 'help' or 'ls' to see applications.\r\n\r\n");
			console_puts("halo> ");
			len = 0;
		}

		/* If a foreground task was running, check if it has exited on its own */
		if (foreground_pid > 0) {
			tcb *fg = task_get_by_pid(foreground_pid);
			if (!fg || fg->state == t_quit || fg->name[0] == '\0') {
				foreground_pid = 0;
				console_puts("halo> ");
				len = 0;
			}
		}

		/* Non-blocking read */
		int ch = console_getc();
		if (ch < 0) {
			mdelay(10);
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

					console_puts("^C\r\n[STOP] Terminated foreground task '");
					console_puts(killed_name);
					console_puts("' (PID ");
					print_uint(foreground_pid);
					console_puts(").\r\n");

					/* Reset hardware if needed */
					if (strcmp(killed_name, "breathe") == 0) {
						pwm_set(0);
					}

					task_kill(fg->slot_id);
				}
				foreground_pid = 0;
				console_puts("halo> ");
				len = 0;
			} else {
				console_puts("^C\r\nhalo> ");
				len = 0;
			}
			continue;
		}

		/* If a foreground task is actively running, ignore normal shell typing */
		if (foreground_pid > 0) {
			continue;
		}

		/* Normal shell line editing */
		if (ch == '\r' || ch == '\n') {
			console_puts("\r\n");
			line[len] = '\0';
			execute_command(line);
			if (foreground_pid == 0) {
				console_puts("halo> ");
			}
			len = 0;
		} else if (ch == '\b' || ch == 0x7F) {
			/* Backspace */
			if (len > 0) {
				len--;
				console_puts("\b \b");
			}
		} else if (ch >= 32 && ch <= 126) {
			/* Printable character */
			if (len < SHELL_MAX_LINE - 1) {
				line[len++] = (char)ch;
				console_putc((char)ch);
			}
		}
	}
}
