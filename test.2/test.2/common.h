#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>

/* ── 常量定义 ─────────────────────────────────────────────── */
#define MAX_LINE_LENGTH    2048
#define PAGE_SIZE          15
#define INITIAL_CAPACITY   1000
#define MAX_PARAM_NAME     32
#define MAX_PATH_LEN       512

/* 参数有效范围（海水养殖合理范围） */
#define TEMP_MIN        -5.0
#define TEMP_MAX        40.0
#define SALINITY_MIN     0.0
#define SALINITY_MAX    45.0
#define PH_MIN           6.5
#define PH_MAX           9.0
#define DO_MIN           0.0
#define DO_MAX          15.0
#define PRECIP_MIN       0.0
#define PRECIP_MAX     500.0
#define AIR_TEMP_MIN   -10.0
#define AIR_TEMP_MAX    50.0

/* 缺失/无效数据的标记值 */
#define INVALID_VALUE  -999.0

/* 数据与备份目录 */
#define DATA_DIR        ".\\data"
#define DATA_FILE       ".\\data\\data.csv"
#define BACKUP_DIR      ".\\backups"

/* ── 数据结构 ────────────────────────────────────────────── */
typedef struct {
    int    record_id;        /* 顺序编号（从CSV行号自动生成）        */
    double temp;             /* 水温                  (℃)         */
    double salinity;         /* 盐度                  (PSU)       */
    double ph;               /* pH值                               */
    double do_value;         /* 溶解氧                (mg/l)      */
    double precipitation;    /* 降水量                (mm)        */
    double air_temp;         /* 气温                  (℃)         */
    int    valid;            /* 1 = 有效, 0 = 已删除               */
} WaterQualityRecord;

typedef struct {
    WaterQualityRecord *records;   /* 动态数组                     */
    int    count;                  /* 当前存储的记录数              */
    int    capacity;               /* 已分配的槽位数                */
    int    total_read;             /* 从CSV读取的总行数             */
    int    valid_count;            /* valid == 1 的记录数           */
    char   source_file[MAX_PATH_LEN];
} WaterQualityDataset;

/* ── 工具函数 ────────────────────────────────────────────── */
void     clear_input_buffer(void);
void     pause_and_continue(void);
int      get_int_input(const char *prompt, int min_val, int max_val);
double   get_double_input(const char *prompt, double min_val, double max_val);
int      get_yes_no(const char *prompt);
int      file_exists(const char *path);
int      make_directory(const char *path);
void     get_timestamp_str(char *buf, size_t size);

/* 参数校验 */
int      validate_temp(double v);
int      validate_salinity(double v);
int      validate_ph(double v);
int      validate_do(double v);
int      validate_precipitation(double v);
int      validate_air_temp(double v);
int      validate_record(const WaterQualityRecord *r);

#endif
