#include <shell.h>
#include <console.h>
#include <scheduler.h>
#include <usb_cdc.h>
#include <string.h>
#include <pwm.h>
#include <telemetry.h>

extern void task_breathe_led(void);
extern void task_display(void);
extern void task_hello(void);
extern uint32_t display_get_tick(void);
extern char display_get_spinner(void);

typedef struct {
	const char *name;
	void (*entry)(void);
	uint8_t bg;
	uint8_t is_oneshot;
	const char *description;
} split_app_reg_t;

/* Original Application Registry Table */
static const split_app_reg_t app_registry[] = {
	{ "breathe", task_breathe_led, 1, 0, "Hardware PWM breathing LED on GPIO 25" },
	{ "display", task_display,     0, 0, "Interactive counter display (Ctrl+C to stop)" },
	{ "hello",   task_hello,       0, 1, "One-shot Hello World task (runs once and exits)" },
};
#define NUM_APPS  (sizeof(app_registry) / sizeof(app_registry[0]))

#define SHELL_MAX_LINE 128

static volatile uint32_t foreground_pid = 0;
static char shell_line[SHELL_MAX_LINE];
static uint32_t shell_len = 0;

static int count_active_bg_tasks(void)
{
	int count = 0;
	for (int i = 1; i < MAX_TASKS; i++) {
		if (task[i].state != t_quit && task[i].name[0] != '\0' && task[i].mode == MODE_BACKGROUND) {
			count++;
		}
	}
	return count;
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

void shell_prompt(void)
{
	console_puts("halo> ");
}

static void cmd_help(void)
{
	console_puts("Available commands:\r\n");
	console_puts("  help        - Display this command menu\r\n");
	console_puts("  ls          - List all applications in the App Registry\r\n");
	console_puts("  run <app> [&]- Run an app (& forces background execution)\r\n");
	console_puts("  ps          - List active tasks, states, modes, stack pointers & telemetry\r\n");
	console_puts("  kill <app..>- Terminate task(s) by name, or 'kill all'\r\n");
	console_puts("  resize      - Re-detect terminal dimensions and adapt canvas\r\n");
	console_puts("  clear       - Clear the screen\r\n");
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

static void cmd_ps(void)
{
	console_puts("PID     NAME       STATE    MODE        STACK_SP    TELEMETRY\r\n");
	console_puts("----------------------------------------------------------------------\r\n");
	for (uint8_t i = 0; i < MAX_TASKS; i++) {
		if (task[i].state == t_quit || task[i].name[0] == '\0') {
			continue;
		}

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
		console_puts("  ");

		if (strcmp(task[i].name, "shell") == 0) {
			console_puts("Active Shell");
		} else if (strcmp(task[i].name, "breathe") == 0) {
			console_puts("PWM Breathing (2 kHz) [");
			print_uint((uint32_t)g_telemetry.breathe_duty);
			console_puts("%]");
		} else if (strcmp(task[i].name, "display") == 0) {
			console_puts("Tick: ");
			print_uint(display_get_tick());
			console_puts(" [");
			console_putc(display_get_spinner());
			console_puts("]");
		} else if (strcmp(task[i].name, "hello") == 0) {
			console_puts("One-Shot Done");
		}
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

	const split_app_reg_t *app = 0;
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
		console_puts("[ERROR] Failed to spawn: task pool is full.\r\n");
		return;
	}

	if (is_bg) {
		if (!app->is_oneshot) {
			if (!console_split_is_active()) {
				console_split_activate();
			} else {
				console_split_render_telemetry();
			}
		}

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

static int kill_single_task(const char *name)
{
	if (strcmp(name, "shell") == 0 || strcmp(name, "0") == 0) {
		console_puts("[ERROR] Cannot kill root system shell task!\r\n");
		return -1;
	}

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

	if (strcmp(name, "breathe") == 0) {
		pwm_set(0);
		g_telemetry.breathe_duty = 0;
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

	if (strcmp(arg, "all") == 0) {
		int killed_count = 0;
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
	} else {
		const char *p = arg;
		char token[32];
		while (*p) {
			while (*p == ' ' || *p == '\t') p++;
			if (*p == '\0') break;

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

	int remaining = count_active_bg_tasks();
	if (remaining == 0 && console_split_is_active()) {
		console_split_deactivate();
		console_puts("\r\n>>> CONNECTED TO STRATAHALO OS <<<\r\n");
		console_puts("[INFO] All background tasks stopped. Restored full-screen shell.\r\n\r\n");
	} else if (console_split_is_active()) {
		console_split_render_telemetry();
	}
}

static void execute_command(char *cmd_line)
{
	while (*cmd_line == ' ' || *cmd_line == '\t') cmd_line++;
	if (*cmd_line == '\0') return;

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
	} else if (strcmp(cmd_line, "resize") == 0) {
		console_split_resize();
	} else if (strcmp(cmd_line, "clear") == 0) {
		console_split_clear_left();
	} else {
		console_puts("[ERROR] Unknown command '");
		console_puts(cmd_line);
		console_puts("'. Type 'help' for available commands.\r\n");
	}
}

void shell_task(void)
{
	int welcomed = 0;

	while (1) {
		if (!usb_cdc_is_connected()) {
			if (welcomed) {
				pwm_set(0); /* Hardware Fail-Safe: Force PWM off on USB disconnect */
				g_telemetry.breathe_duty = 0;
				console_split_deactivate();
				welcomed = 0;
			}
			foreground_pid = 0;
			mdelay(50);
			continue;
		}

		if (!welcomed) {
			welcomed = 1;
			console_split_deactivate();
			console_puts("\r\n>>> CONNECTED TO STRATAHALO OS <<<\r\n");
			console_puts("Type 'help' or 'ls' to see applications.\r\n\r\n");
			shell_len = 0;
			shell_prompt();
		}

		if (foreground_pid > 0) {
			tcb *fg = task_get_by_pid(foreground_pid);
			if (!fg || fg->state == t_quit || fg->name[0] == '\0') {
				foreground_pid = 0;
				shell_len = 0;
				shell_prompt();
			}
		}

		/* Auto-deactivate split screen if all background tasks have stopped */
		if (console_split_is_active() && count_active_bg_tasks() == 0 && foreground_pid == 0) {
			console_split_deactivate();
			console_puts("\r\n>>> CONNECTED TO STRATAHALO OS <<<\r\n");
			console_puts("[INFO] All background tasks completed. Restored full-screen shell.\r\n\r\n");
			shell_prompt();
			if (shell_len > 0) {
				for (uint32_t i = 0; i < shell_len; i++) {
					console_putc(shell_line[i]);
				}
			}
		}

		/* If telemetry in memory changed, update right pane strictly from Shell task (throttled to 80ms to prevent cursor flicker) */
		static uint32_t last_telemetry_render = 0;
		if (console_split_is_active() && g_telemetry.dirty && foreground_pid == 0) {
			if ((system_millis - last_telemetry_render) >= 80) {
				last_telemetry_render = system_millis;
				g_telemetry.dirty = 0;
				console_split_render_telemetry();
			}
		}

		int ch = console_getc();
		if (ch < 0) {
			mdelay(10);
			continue;
		}

		if (ch == 0x03) { /* Ctrl+C */
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

					if (strcmp(killed_name, "breathe") == 0) {
						pwm_set(0);
						g_telemetry.breathe_duty = 0;
					}

					task_kill(fg->slot_id);
				}
				foreground_pid = 0;
				shell_len = 0;
				shell_prompt();
			} else {
				console_puts("^C\r\n");
				shell_len = 0;
				shell_prompt();
			}
			continue;
		}

		if (foreground_pid > 0) {
			continue;
		}

		if (ch == '\r' || ch == '\n') {
			console_lock();
			console_puts("\r\n");
			shell_line[shell_len] = '\0';
			execute_command(shell_line);
			shell_len = 0;
			if (foreground_pid == 0) {
				shell_prompt();
			}
			console_unlock();
		} else if (ch == '\b' || ch == 0x7F) {
			if (shell_len > 0) {
				console_lock();
				shell_len--;
				console_putc('\b');
				console_unlock();
			}
		} else if (ch >= 32 && ch <= 126) {
			if (shell_len < SHELL_MAX_LINE - 1) {
				console_lock();
				shell_line[shell_len++] = (char)ch;
				console_putc((char)ch);
				console_unlock();
			}
		}
	}
}
