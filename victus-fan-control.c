#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_PATH_LEN 1024
#define MAX_LINE_LEN 512
#define MAX_CURVE_POINTS 32

typedef struct {
	int temp;
	int speed_percent;
} CurvePoint;

typedef struct {
	int check_interval;
	int heartbeat_interval;
	int default_speed;
	int temp_critical;
	bool restore_auto_on_exit;
	bool enable_colors;
	bool linear_interpolation;
	char override_fan_dir[MAX_PATH_LEN];
	char override_cpu_temp_file[MAX_PATH_LEN];
	CurvePoint curve[MAX_CURVE_POINTS];
	int curve_count;
} Config;

static void safe_copy(char *dst, const char *src, size_t dst_size) {
	if (!dst || !src || dst_size == 0) return;
	size_t len = strlen(src);
	if (len >= dst_size) len = dst_size - 1;
	memcpy(dst, src, len);
	dst[len] = '\0';
}

static void path_join(char *dst, size_t dst_size, const char *dir, const char *file) {
	if (!dst || dst_size == 0) return;
	int ret = snprintf(dst, dst_size, "%s/%s", dir ? dir : "", file ? file : "");
	if (ret < 0 || (size_t)ret >= dst_size) {
		dst[dst_size - 1] = '\0';
	}
}

static bool get_executable_dir(char *out_dir, size_t max_len) {
	if (!out_dir || max_len == 0) return false;
	char exe_path[MAX_PATH_LEN];
	ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (len <= 0) return false;
	exe_path[len] = '\0';

	char *last_slash = strrchr(exe_path, '/');
	if (!last_slash) return false;

	*last_slash = '\0';
	safe_copy(out_dir, exe_path, max_len);
	return true;
}

#define MAX_FANS 8

// Structure representing a single controllable hardware fan
typedef struct {
	int id;                         // Fan index (1 for fan1, 2 for fan2, etc.)
	int max_rpm;                    // Maximum RPM read from sysfs
	int cur_rpm;                    // Current RPM read from sysfs
	int target_rpm;                 // Calculated target RPM
	int last_target_rpm;            // Previous target RPM to detect changes
	char target_path[MAX_PATH_LEN]; // Path to fanX_target sysfs file
	char input_path[MAX_PATH_LEN];  // Path to fanX_input sysfs file
	char max_path[MAX_PATH_LEN];    // Path to fanX_max sysfs file
} FanDevice;

static volatile sig_atomic_t g_running = 1;
static bool g_dry_run = false;
static bool g_verbose = false;

static Config g_config;
static char g_fan_dir[MAX_PATH_LEN] = "";
static char g_cpu_temp_file[MAX_PATH_LEN] = "";
static FanDevice g_fans[MAX_FANS];
static int g_fan_count = 0;

static const char *C_RESET = "";
static const char *C_BOLD = "";
static const char *C_RED = "";
static const char *C_GREEN = "";
static const char *C_YELLOW = "";
static const char *C_CYAN = "";

static void init_colors(bool enable) {
	if (enable && isatty(STDOUT_FILENO)) {
		C_RESET  = "\033[0m";
		C_BOLD   = "\033[1m";
		C_RED    = "\033[31m";
		C_GREEN  = "\033[32m";
		C_YELLOW = "\033[33m";
		C_CYAN   = "\033[36m";
	} else {
		C_RESET = C_BOLD = C_RED = C_GREEN = C_YELLOW = C_CYAN = "";
	}
}

static void handle_signal(int sig) {
	(void)sig;
	g_running = 0;
}

static void setup_signals(void) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
}

static int compare_curve_points(const void *a, const void *b) {
	const CurvePoint *p1 = (const CurvePoint *)a;
	const CurvePoint *p2 = (const CurvePoint *)b;
	return p1->temp - p2->temp;
}

static void set_default_config(Config *cfg) {
	cfg->check_interval = 2;
	cfg->heartbeat_interval = 10;
	cfg->default_speed = 0;
	cfg->temp_critical = 85;
	cfg->restore_auto_on_exit = true;
	cfg->enable_colors = true;
	cfg->linear_interpolation = true;
	cfg->override_fan_dir[0] = '\0';
	cfg->override_cpu_temp_file[0] = '\0';

	cfg->curve[0] = (CurvePoint){50, 30};
	cfg->curve[1] = (CurvePoint){60, 40};
	cfg->curve[2] = (CurvePoint){70, 60};
	cfg->curve[3] = (CurvePoint){75, 75};
	cfg->curve[4] = (CurvePoint){80, 85};
	cfg->curve_count = 5;
}

static char *trim_whitespace(char *str) {
	while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') str++;
	if (*str == 0) return str;
	char *end = str + strlen(str) - 1;
	while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
	end[1] = '\0';
	return str;
}

static void parse_fan_curve(Config *cfg, const char *value) {
	char buf[512];
	safe_copy(buf, value, sizeof(buf));

	int count = 0;
	char *token = strtok(buf, " ,;\t");
	while (token && count < MAX_CURVE_POINTS) {
		char *colon = strchr(token, ':');
		if (colon) {
			*colon = '\0';
			int temp = atoi(token);
			int speed = atoi(colon + 1);
			if (temp >= 0 && speed >= 0 && speed <= 100) {
				cfg->curve[count++] = (CurvePoint){temp, speed};
			}
		}
		token = strtok(NULL, " ,;\t");
	}

	if (count > 0) {
		// Step 1: Sort strictly by temperature in ascending order
		qsort(cfg->curve, count, sizeof(CurvePoint), compare_curve_points);

		// Step 2: Deduplicate entries with identical temperature, keeping the one with higher fan speed
		int unique_count = 0;
		CurvePoint unique_curve[MAX_CURVE_POINTS];

		for (int i = 0; i < count; i++) {
			if (unique_count == 0 || cfg->curve[i].temp != unique_curve[unique_count - 1].temp) {
				unique_curve[unique_count++] = cfg->curve[i];
			} else {
				if (cfg->curve[i].speed_percent > unique_curve[unique_count - 1].speed_percent) {
					unique_curve[unique_count - 1].speed_percent = cfg->curve[i].speed_percent;
				}
			}
		}

		for (int i = 0; i < unique_count; i++) {
			cfg->curve[i] = unique_curve[i];
		}
		cfg->curve_count = unique_count;
	}
}

static bool load_config_file(const char *path, Config *cfg) {
	FILE *fp = fopen(path, "r");
	if (!fp) return false;

	char line[MAX_LINE_LEN];
	while (fgets(line, sizeof(line), fp)) {
		char *trimmed = trim_whitespace(line);
		if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') continue;

		char *eq = strchr(trimmed, '=');
		if (!eq) continue;
		*eq = '\0';
		char *key = trim_whitespace(trimmed);
		char *val = trim_whitespace(eq + 1);

		if (strcasecmp(key, "check_interval") == 0) {
			int v = atoi(val);
			if (v > 0) cfg->check_interval = v;
		} else if (strcasecmp(key, "heartbeat_interval") == 0) {
			int v = atoi(val);
			if (v > 0) cfg->heartbeat_interval = v;
		} else if (strcasecmp(key, "default_speed") == 0) {
			cfg->default_speed = atoi(val);
		} else if (strcasecmp(key, "temp_critical") == 0) {
			cfg->temp_critical = atoi(val);
		} else if (strcasecmp(key, "restore_auto_on_exit") == 0) {
			cfg->restore_auto_on_exit = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0);
		} else if (strcasecmp(key, "enable_colors") == 0) {
			cfg->enable_colors = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0);
		} else if (strcasecmp(key, "linear_interpolation") == 0) {
			cfg->linear_interpolation = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0);
		} else if (strcasecmp(key, "override_fan_dir") == 0) {
			safe_copy(cfg->override_fan_dir, val, sizeof(cfg->override_fan_dir));
		} else if (strcasecmp(key, "override_cpu_temp_file") == 0) {
			safe_copy(cfg->override_cpu_temp_file, val, sizeof(cfg->override_cpu_temp_file));
		} else if (strcasecmp(key, "fan_curve") == 0) {
			parse_fan_curve(cfg, val);
		}
	}

	fclose(fp);
	return true;
}

static bool file_exists(const char *path) {
	return access(path, F_OK) == 0;
}

static bool read_file_string(const char *path, char *buf, size_t max_len) {
	FILE *fp = fopen(path, "r");
	if (!fp) return false;
	if (!fgets(buf, max_len, fp)) {
		fclose(fp);
		return false;
	}
	fclose(fp);
	char *end = buf + strlen(buf) - 1;
	while (end >= buf && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
		*end = '\0';
		end--;
	}
	return true;
}

static int read_file_int(const char *path) {
	char buf[64];
	if (!read_file_string(path, buf, sizeof(buf))) return -1;
	return atoi(buf);
}

static bool write_file_int(const char *path, int value) {
	if (g_dry_run) return true;
	FILE *fp = fopen(path, "w");
	if (!fp) {
		fprintf(stderr, "%s[ERROR] fopen(%s) failed: %s (errno=%d)%s\n", C_RED, path, strerror(errno), errno, C_RESET);
		return false;
	}
	if (fprintf(fp, "%d\n", value) < 0) {
		fprintf(stderr, "%s[ERROR] fprintf to %s failed: %s (errno=%d)%s\n", C_RED, path, strerror(errno), errno, C_RESET);
		fclose(fp);
		return false;
	}
	if (fclose(fp) != 0) {
		fprintf(stderr, "%s[ERROR] sysfs write rejected on %s (val=%d): %s (errno=%d)%s\n",
				C_RED, path, value, strerror(errno), errno, C_RESET);
		return false;
	}
	return true;
}

static bool find_fan_hwmon(char *out_path, size_t max_len) {
	if (g_config.override_fan_dir[0] && access(g_config.override_fan_dir, F_OK) == 0) {
		safe_copy(out_path, g_config.override_fan_dir, max_len);
		return true;
	}

	// 1. Direct platform device path
	DIR *dir = opendir("/sys/devices/platform/hp-wmi/hwmon");
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strncmp(entry->d_name, "hwmon", 5) == 0) {
				char dev_dir[MAX_PATH_LEN];
				path_join(dev_dir, sizeof(dev_dir), "/sys/devices/platform/hp-wmi/hwmon", entry->d_name);
				char test_path[MAX_PATH_LEN];
				path_join(test_path, sizeof(test_path), dev_dir, "fan1_target");
				if (file_exists(test_path)) {
					safe_copy(out_path, dev_dir, max_len);
					closedir(dir);
					return true;
				}
			}
		}
		closedir(dir);
	}

	// 2. Search hwmon devices for name "hp"
	dir = opendir("/sys/class/hwmon");
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strncmp(entry->d_name, "hwmon", 5) == 0) {
				char dev_dir[MAX_PATH_LEN];
				path_join(dev_dir, sizeof(dev_dir), "/sys/class/hwmon", entry->d_name);
				char name_path[MAX_PATH_LEN];
				char name_buf[64];
				path_join(name_path, sizeof(name_path), dev_dir, "name");
				if (read_file_string(name_path, name_buf, sizeof(name_buf)) && strcmp(name_buf, "hp") == 0) {
					char test_path[MAX_PATH_LEN];
					path_join(test_path, sizeof(test_path), dev_dir, "fan1_target");
					if (file_exists(test_path)) {
						safe_copy(out_path, dev_dir, max_len);
						closedir(dir);
						return true;
					}
				}
			}
		}
		closedir(dir);
	}

	return false;
}

// Discover and initialize all available fans in the specified hwmon directory
static bool init_fans(const char *fan_dir) {
	g_fan_count = 0;

	for (int i = 1; i <= MAX_FANS; i++) {
		char target_path[MAX_PATH_LEN];
		char max_path[MAX_PATH_LEN];
		char input_path[MAX_PATH_LEN];

		char target_file[32], max_file[32], input_file[32];
		snprintf(target_file, sizeof(target_file), "fan%d_target", i);
		snprintf(max_file, sizeof(max_file), "fan%d_max", i);
		snprintf(input_file, sizeof(input_file), "fan%d_input", i);

		path_join(target_path, sizeof(target_path), fan_dir, target_file);
		path_join(max_path, sizeof(max_path), fan_dir, max_file);
		path_join(input_path, sizeof(input_path), fan_dir, input_file);

		if (file_exists(target_path)) {
			int max_rpm = read_file_int(max_path);
			if (max_rpm <= 0) {
				max_rpm = 5000;
				fprintf(stderr, "%s[ERROR] Failed to read maximum RPM for fan %d (%s)! Using fallback %d RPM.%s\n",
						C_RED, i, max_path, max_rpm, C_RESET);
			}

			FanDevice *f = &g_fans[g_fan_count++];
			f->id = i;
			f->max_rpm = max_rpm;
			f->cur_rpm = 0;
			f->target_rpm = 0;
			f->last_target_rpm = -1;
			safe_copy(f->target_path, target_path, sizeof(f->target_path));
			safe_copy(f->input_path, input_path, sizeof(f->input_path));
			safe_copy(f->max_path, max_path, sizeof(f->max_path));
		}
	}

	if (g_fan_count >= 4) {
		fprintf(stderr, "%s[WARNING] Unusual number of fans detected (%d fans). Please verify your hardware configuration.%s\n",
				C_YELLOW, g_fan_count, C_RESET);
	}

	return (g_fan_count > 0);
}

static bool find_cpu_temp_sensor(char *out_path, size_t max_len) {
	if (g_config.override_cpu_temp_file[0] && access(g_config.override_cpu_temp_file, F_OK) == 0) {
		safe_copy(out_path, g_config.override_cpu_temp_file, max_len);
		return true;
	}

	const char *known_drivers[] = {"k10temp", "coretemp", "zenpower", "cpu_thermal", "soc_thermal", NULL};

	DIR *dir = opendir("/sys/class/hwmon");
	if (!dir) return false;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "hwmon", 5) != 0) continue;

		char hwmon_dir[MAX_PATH_LEN];
		char name_path[MAX_PATH_LEN];
		char name_buf[64];
		path_join(hwmon_dir, sizeof(hwmon_dir), "/sys/class/hwmon", entry->d_name);
		path_join(name_path, sizeof(name_path), hwmon_dir, "name");

		if (!read_file_string(name_path, name_buf, sizeof(name_buf))) continue;

		for (int i = 0; known_drivers[i] != NULL; i++) {
			if (strcmp(name_buf, known_drivers[i]) == 0) {
				// Check labels for Tctl, Package, etc.
				for (int t = 1; t <= 8; t++) {
					char lbl_name[32];
					snprintf(lbl_name, sizeof(lbl_name), "temp%d_label", t);
					char label_path[MAX_PATH_LEN];
					char label_buf[64];
					path_join(label_path, sizeof(label_path), hwmon_dir, lbl_name);
					if (read_file_string(label_path, label_buf, sizeof(label_buf))) {
						if (strcasestr(label_buf, "Tctl") || strcasestr(label_buf, "Tdie") ||
							strcasestr(label_buf, "Package") || strcasestr(label_buf, "CPU")) {
							char inp_name[32];
							snprintf(inp_name, sizeof(inp_name), "temp%d_input", t);
							char input_path[MAX_PATH_LEN];
							path_join(input_path, sizeof(input_path), hwmon_dir, inp_name);
							if (file_exists(input_path)) {
								safe_copy(out_path, input_path, max_len);
								closedir(dir);
								return true;
							}
						}
					}
				}
				char fallback_path[MAX_PATH_LEN];
				path_join(fallback_path, sizeof(fallback_path), hwmon_dir, "temp1_input");
				if (file_exists(fallback_path)) {
					safe_copy(out_path, fallback_path, max_len);
					closedir(dir);
					return true;
				}
			}
		}
	}
	closedir(dir);

	// Fallback: search any hwmon with CPU/Tctl label
	dir = opendir("/sys/class/hwmon");
	if (dir) {
		while ((entry = readdir(dir)) != NULL) {
			if (strncmp(entry->d_name, "hwmon", 5) != 0) continue;
			char hwmon_dir[MAX_PATH_LEN];
			path_join(hwmon_dir, sizeof(hwmon_dir), "/sys/class/hwmon", entry->d_name);
			for (int t = 1; t <= 8; t++) {
				char lbl_name[32];
				snprintf(lbl_name, sizeof(lbl_name), "temp%d_label", t);
				char label_path[MAX_PATH_LEN];
				char label_buf[64];
				path_join(label_path, sizeof(label_path), hwmon_dir, lbl_name);
				if (read_file_string(label_path, label_buf, sizeof(label_buf))) {
					if (strcasestr(label_buf, "Tctl") || strcasestr(label_buf, "Tdie") ||
						strcasestr(label_buf, "Package") || strcasestr(label_buf, "CPU") || strcasestr(label_buf, "Core")) {
						char inp_name[32];
						snprintf(inp_name, sizeof(inp_name), "temp%d_input", t);
						char input_path[MAX_PATH_LEN];
						path_join(input_path, sizeof(input_path), hwmon_dir, inp_name);
						if (file_exists(input_path)) {
							safe_copy(out_path, input_path, max_len);
							closedir(dir);
							return true;
						}
					}
				}
			}
		}
		closedir(dir);
	}

	return false;
}

static int read_cpu_temp(const char *temp_file) {
	int raw = read_file_int(temp_file);
	if (raw <= 0) return -1;
	return raw / 1000;
}

static int g_stuck_temp = -999;
static int g_stuck_count = 0;
static bool g_stuck_logged = false;
static bool g_sensor_err_logged = false;
static bool g_error_occurred = false;

static void print_status_line(FILE *out, int cpu_temp, int speed_percent) {
	time_t rawtime;
	struct tm *timeinfo;
	char time_str[16];
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);

	const char *temp_color = C_GREEN;
	int max_defined_temp = g_config.curve_count > 0 ? g_config.curve[g_config.curve_count - 1].temp : 85;
	if (cpu_temp >= max_defined_temp) {
		temp_color = C_RED;
	} else if (speed_percent > g_config.default_speed) {
		temp_color = C_YELLOW;
	}

	fprintf(out, "[%s] CPU: %s%3d°C%s (%3d%%)", time_str, temp_color, cpu_temp, C_RESET, speed_percent);
	for (int i = 0; i < g_fan_count; i++) {
		fprintf(out, " | FAN%d: %4d RPM (Target: %4d)", g_fans[i].id, g_fans[i].cur_rpm, g_fans[i].target_rpm);
	}
	fprintf(out, "\n");
	fflush(out);
}

static int calculate_speed_percent(int temp_c) {
	// 1. Negative / invalid sensor reading error
	if (temp_c < 0) {
		if (!g_sensor_err_logged) {
			fprintf(stderr, "%s[ERROR] Failed to read CPU temperature sensor (temp=%d°C)! Engaging 100%% emergency fan speed.%s\n",
					C_RED, temp_c, C_RESET);
			g_sensor_err_logged = true;
			g_error_occurred = true;
		}
		return 100;
	} else {
		g_sensor_err_logged = false;
	}

	// 2. Stuck sensor check (5 consecutive checks with identical temperature <= 25°C)
	if (temp_c <= 25) {
		if (temp_c == g_stuck_temp) {
			g_stuck_count++;
		} else {
			g_stuck_temp = temp_c;
			g_stuck_count = 1;
			g_stuck_logged = false;
		}

		if (g_stuck_count >= 5) {
			if (!g_stuck_logged) {
				fprintf(stderr, "%s[ERROR] CPU temperature sensor stuck at %d°C for 5 consecutive checks! Engaging 100%% emergency fan speed.%s\n",
						C_RED, temp_c, C_RESET);
				g_stuck_logged = true;
				g_error_occurred = true;
			}
			return 100;
		}
	} else {
		g_stuck_temp = -999;
		g_stuck_count = 0;
		g_stuck_logged = false;
	}

	// 3. Critical threshold check
	if (temp_c >= g_config.temp_critical) {
		return 100;
	}

	if (g_config.curve_count <= 0) {
		return g_config.default_speed;
	}

	// 4. Below lowest curve point: default speed (quiet/idle mode)
	if (temp_c < g_config.curve[0].temp) {
		return g_config.default_speed;
	}

	// 5. Exceeding highest curve point: 100% Critical Protection mode
	if (temp_c > g_config.curve[g_config.curve_count - 1].temp) {
		return 100;
	}

	// 6. Between curve points
	if (g_config.linear_interpolation) {
		for (int i = 0; i < g_config.curve_count - 1; i++) {
			int t1 = g_config.curve[i].temp;
			int t2 = g_config.curve[i + 1].temp;
			if (temp_c >= t1 && temp_c <= t2) {
				int s1 = g_config.curve[i].speed_percent;
				int s2 = g_config.curve[i + 1].speed_percent;
				if (t2 == t1) return s1;
				int speed = s1 + ((s2 - s1) * (temp_c - t1)) / (t2 - t1);
				if (speed < 0) speed = 0;
				if (speed > 100) speed = 100;
				return speed;
			}
		}
		return g_config.curve[g_config.curve_count - 1].speed_percent;
	} else {
		// Step-wise (discrete) mode
		int selected = g_config.default_speed;
		for (int i = 0; i < g_config.curve_count; i++) {
			if (temp_c >= g_config.curve[i].temp) {
				selected = g_config.curve[i].speed_percent;
			}
		}
		return selected;
	}
}

static void print_curve_table(void) {
	if (g_config.linear_interpolation) {
		printf("Temperature Curve Mode: Linear Interpolation\n");
		if (g_config.curve_count <= 0) {
			printf("  (No curve points defined, using default: %d%%)\n", g_config.default_speed);
			return;
		}
		if (g_config.curve[0].temp > 0) {
			printf("  < %2d°C       -> %3d%% (Off/Default)\n",
				   g_config.curve[0].temp, g_config.default_speed);
		}
		for (int i = 0; i < g_config.curve_count - 1; i++) {
			printf("  %2d°C - %2d°C  -> %3d%% ~%3d%% (Linear)\n",
				   g_config.curve[i].temp, g_config.curve[i + 1].temp,
				   g_config.curve[i].speed_percent, g_config.curve[i + 1].speed_percent);
		}
		printf("  = %2d°C       -> %3d%%\n",
			   g_config.curve[g_config.curve_count - 1].temp,
			   g_config.curve[g_config.curve_count - 1].speed_percent);
		printf("  > %2d°C       -> 100%% (Critical Protection)\n",
			   g_config.curve[g_config.curve_count - 1].temp);
	} else {
		printf("Temperature Curve Mode: Step-wise (Discrete)\n");
		if (g_config.curve_count <= 0) {
			printf("  (No curve points defined, using default: %d%%)\n", g_config.default_speed);
			return;
		}
		int prev_temp = 0;
		int prev_speed = g_config.default_speed;
		for (int i = 0; i < g_config.curve_count; i++) {
			int th = g_config.curve[i].temp;
			int sp = g_config.curve[i].speed_percent;
			if (i == 0) {
				if (th > 0) {
					printf("  < %2d°C       -> %3d%% (Off/Default)\n", th, g_config.default_speed);
				}
			} else {
				printf("  %2d°C - %2d°C   -> %3d%%\n", prev_temp, th, prev_speed);
			}
			prev_temp = th;
			prev_speed = sp;
		}
		printf("  = %2d°C       -> %3d%%\n", prev_temp, prev_speed);
		printf("  > %2d°C       -> 100%% (Critical Protection)\n", prev_temp);
	}
}

static void cleanup(void) {
	if (!g_dry_run && g_config.restore_auto_on_exit && g_fan_dir[0]) {
		printf("\n%s[INFO] Restoring fan control to BIOS / automatic mode...%s\n", C_CYAN, C_RESET);
		char pwm_path[MAX_PATH_LEN];
		path_join(pwm_path, sizeof(pwm_path), g_fan_dir, "pwm1_enable");
		if (file_exists(pwm_path)) {
			write_file_int(pwm_path, 2);
		}
		for (int i = 0; i < g_fan_count; i++) {
			write_file_int(g_fans[i].target_path, 0);
		}
	}
	printf("%s[DONE] Fan control daemon stopped cleanly.%s\n", C_GREEN, C_RESET);
}

static void print_usage(const char *prog_name) {
	printf("Usage: %s [OPTIONS]\n\n", prog_name);
	printf("Options:\n");
	printf("  -c, --config <file>    Path to configuration file (default: config.conf in app directory)\n");
	printf("  -t, --test, --dry-run  Simulation mode (read sensors, no writes to sysfs)\n");
	printf("  -v, --verbose          Enable verbose debug output\n");
	printf("  -h, --help             Display this help message and exit\n");
}

int main(int argc, char *argv[]) {
	setlinebuf(stdout);
	setlinebuf(stderr);
	char config_path[MAX_PATH_LEN] = "";

	static struct option long_options[] = {
		{"config",   required_argument, 0, 'c'},
		{"test",     no_argument,       0, 't'},
		{"dry-run",  no_argument,       0, 't'},
		{"verbose",  no_argument,       0, 'v'},
		{"help",     no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "c:tvh", long_options, NULL)) != -1) {
		switch (opt) {
			case 'c':
				safe_copy(config_path, optarg, sizeof(config_path));
				break;
			case 't':
				g_dry_run = true;
				break;
			case 'v':
				g_verbose = true;
				break;
			case 'h':
				print_usage(argv[0]);
				return 0;
			default:
				print_usage(argv[0]);
				return 1;
		}
	}

	set_default_config(&g_config);

	if (config_path[0]) {
		if (!load_config_file(config_path, &g_config)) {
			fprintf(stderr, "Warning: Could not open specified config file: %s (using defaults)\n", config_path);
		}
	} else {
		bool loaded = false;
		char exe_dir[MAX_PATH_LEN];
		char candidate_path[MAX_PATH_LEN];

		// 1. Prioritize config.conf in the same directory as the executable (self-contained mode)
		if (get_executable_dir(exe_dir, sizeof(exe_dir))) {
			path_join(candidate_path, sizeof(candidate_path), exe_dir, "config.conf");
			if (file_exists(candidate_path)) {
				loaded = load_config_file(candidate_path, &g_config);
			}
		}

		// 2. Fallback to current working directory
		if (!loaded && file_exists("config.conf")) {
			loaded = load_config_file("config.conf", &g_config);
		}

		// 3. Fallback to user ~/.config/victus-fan-control/config.conf
		if (!loaded) {
			const char *home = getenv("HOME");
			if (home) {
				path_join(candidate_path, sizeof(candidate_path), home, ".config/victus-fan-control/config.conf");
				if (file_exists(candidate_path)) {
					loaded = load_config_file(candidate_path, &g_config);
				}
			}
		}

		// 4. Fallback to system-wide /etc/victus-fan-control.conf
		if (!loaded && file_exists("/etc/victus-fan-control.conf")) {
			loaded = load_config_file("/etc/victus-fan-control.conf", &g_config);
		}
	}

	init_colors(g_config.enable_colors);

	if (!g_dry_run && geteuid() != 0) {
		fprintf(stderr, "%s%s[ERROR] This daemon requires root (sudo) privileges to control fan speeds.%s\n",
				C_RED, C_BOLD, C_RESET);
		fprintf(stderr, "Run with: sudo %s\n", argv[0]);
		fprintf(stderr, "Or for testing: %s --test\n", argv[0]);
		return 1;
	}

	printf("%s%s======================================================%s\n", C_BOLD, C_CYAN, C_RESET);
	printf("%s%s        HP Dynamic Fan Control Daemon (Native C)      %s\n", C_BOLD, C_CYAN, C_RESET);
	printf("%s%s======================================================%s\n", C_BOLD, C_CYAN, C_RESET);

	if (g_dry_run) {
		printf("%s[WARNING] Running in simulation (DRY-RUN) mode. No sysfs writes will be performed.%s\n", C_YELLOW, C_RESET);
	}

	if (!find_fan_hwmon(g_fan_dir, sizeof(g_fan_dir))) {
		fprintf(stderr, "%s[ERROR] HP Fan control hwmon directory could not be found!%s\n", C_RED, C_RESET);
		return 1;
	}

	if (!find_cpu_temp_sensor(g_cpu_temp_file, sizeof(g_cpu_temp_file))) {
		fprintf(stderr, "%s[ERROR] CPU temperature sensor file could not be found!%s\n", C_RED, C_RESET);
		return 1;
	}

	if (!init_fans(g_fan_dir)) {
		fprintf(stderr, "%s[ERROR] No controllable fans found in %s!%s\n", C_RED, g_fan_dir, C_RESET);
		return 1;
	}

	printf("%s[+] Fan Control Directory :%s %s\n", C_GREEN, C_RESET, g_fan_dir);
	printf("%s[+] CPU Temperature Path  :%s %s\n", C_GREEN, C_RESET, g_cpu_temp_file);
	printf("%s[+] Discovered Fans       :%s %d fan(s)\n", C_GREEN, C_RESET, g_fan_count);
	for (int i = 0; i < g_fan_count; i++) {
		printf("%s[+]   - FAN%d Maximum RPM   :%s %d RPM\n", C_GREEN, g_fans[i].id, C_RESET, g_fans[i].max_rpm);
	}
	printf("%s[+] Polling Interval      :%s %d seconds\n", C_GREEN, C_RESET, g_config.check_interval);
	printf("%s[+] Heartbeat Interval    :%s %d seconds\n", C_GREEN, C_RESET, g_config.heartbeat_interval);
	printf("%s------------------------------------------------------%s\n", C_CYAN, C_RESET);
	print_curve_table();
	printf("%s------------------------------------------------------%s\n\n", C_CYAN, C_RESET);

	setup_signals();
	atexit(cleanup);

	char pwm_path[MAX_PATH_LEN];
	path_join(pwm_path, sizeof(pwm_path), g_fan_dir, "pwm1_enable");

	// Initialize controller in manual mode (1)
	if (!g_dry_run && file_exists(pwm_path)) {
		write_file_int(pwm_path, 1);
	}

	time_t last_write_time = 0;

	while (g_running) {
		int cpu_temp = read_cpu_temp(g_cpu_temp_file);
		if (cpu_temp < 0) {
			find_cpu_temp_sensor(g_cpu_temp_file, sizeof(g_cpu_temp_file));
			cpu_temp = read_cpu_temp(g_cpu_temp_file);
		}

		int speed_percent = calculate_speed_percent(cpu_temp);

		bool any_target_changed = false;
		for (int i = 0; i < g_fan_count; i++) {
			g_fans[i].target_rpm = (g_fans[i].max_rpm * speed_percent) / 100;
			if (g_fans[i].target_rpm != g_fans[i].last_target_rpm) {
				any_target_changed = true;
			}
		}

		time_t now = time(NULL);
		bool heartbeat_due = (now - last_write_time >= g_config.heartbeat_interval);

		if (any_target_changed || heartbeat_due) {
			if (!g_dry_run) {
				for (int i = 0; i < g_fan_count; i++) {
					write_file_int(g_fans[i].target_path, g_fans[i].target_rpm);
				}
			}
			for (int i = 0; i < g_fan_count; i++) {
				g_fans[i].last_target_rpm = g_fans[i].target_rpm;
			}
			last_write_time = now;
		}

		for (int i = 0; i < g_fan_count; i++) {
			g_fans[i].cur_rpm = read_file_int(g_fans[i].input_path);
			if (g_fans[i].cur_rpm < 0) g_fans[i].cur_rpm = 0;
		}

		if (g_error_occurred) {
			print_status_line(stderr, cpu_temp, speed_percent);
			g_error_occurred = false;
		}

		if (g_verbose) {
			print_status_line(stdout, cpu_temp, speed_percent);
		}

		sleep(g_config.check_interval);
	}

	return 0;
}
