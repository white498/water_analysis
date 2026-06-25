#define _CRT_SECURE_NO_WARNINGS
#include "common.h"
#ifdef _WIN32
#include <direct.h>
#endif

void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

void pause_and_continue(void) {
    printf("按 Enter 键继续...");
    clear_input_buffer();
}

int get_int_input(const char *prompt, int min_val, int max_val) {
    int val;
    while (1) {
        printf("%s", prompt);
        if (scanf("%d", &val) == 1 && val >= min_val && val <= max_val) {
            clear_input_buffer();
            return val;
        }
        printf("输入无效，请输入 %d-%d 之间的整数。\n", min_val, max_val);
        clear_input_buffer();
    }
}

double get_double_input(const char *prompt, double min_val, double max_val) {
    double val;
    while (1) {
        printf("%s", prompt);
        if (scanf("%lf", &val) == 1 && val >= min_val && val <= max_val) {
            clear_input_buffer();
            return val;
        }
        printf("输入无效，请输入 %.2f-%.2f 之间的数值。\n", min_val, max_val);
        clear_input_buffer();
    }
}

int get_yes_no(const char *prompt) {
    char buf[16];
    while (1) {
        printf("%s (y/n): ", prompt);
        if (scanf("%15s", buf) == 1) {
            clear_input_buffer();
            if (buf[0] == 'y' || buf[0] == 'Y') return 1;
            if (buf[0] == 'n' || buf[0] == 'N') return 0;
        }
        clear_input_buffer();
        printf("请输入 y 或 n。\n");
    }
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int make_directory(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

void get_timestamp_str(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y%m%d_%H%M%S", tm_info);
}

/* ── 参数校验函数 ───────────────────────────────────────── */
int validate_temp(double v)           { return v >= TEMP_MIN      && v <= TEMP_MAX;      }
int validate_salinity(double v)       { return v >= SALINITY_MIN  && v <= SALINITY_MAX;  }
int validate_ph(double v)             { return v >= PH_MIN        && v <= PH_MAX;        }
int validate_do(double v)             { return v >= DO_MIN        && v <= DO_MAX;        }
int validate_precipitation(double v)  { return v >= PRECIP_MIN    && v <= PRECIP_MAX;    }
int validate_air_temp(double v)       { return v >= AIR_TEMP_MIN  && v <= AIR_TEMP_MAX;  }

int validate_record(const WaterQualityRecord *r) {
    if (!r->valid) return 0;
    if (!validate_temp(r->temp))           return 0;
    if (!validate_salinity(r->salinity))   return 0;
    if (!validate_ph(r->ph))               return 0;
    if (!validate_do(r->do_value))         return 0;
    if (!validate_precipitation(r->precipitation)) return 0;
    if (!validate_air_temp(r->air_temp))   return 0;
    return 1;
}
