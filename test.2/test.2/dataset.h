#ifndef DATASET_H
#define DATASET_H

#include "common.h"

/* ── 参数索引枚举（必须在使用 PARAM_COUNT 的结构体之前定义） ── */
enum {
    PARAM_TEMP = 0,
    PARAM_SALINITY,
    PARAM_PH,
    PARAM_DO,
    PARAM_PRECIPITATION,
    PARAM_AIR_TEMP,
    PARAM_COUNT
};

/* ── 模块 2.1：异常值统计结构体 ─────────────────────────── */
typedef struct {
    int total_records;
    int anomaly_records;
    int valid_records;
    int deleted_count;
    int repaired_count;
    int anomaly_start_id;
    int anomaly_end_id;
    int error_param_counts[PARAM_COUNT];
} OutlierStats;

/* ── 内存管理 ──────────────────────────────────────────── */
WaterQualityDataset *dataset_create(void);
void                 dataset_free(WaterQualityDataset *ds);

/* ── CSV 输入输出 ───────────────────────────────────────── */
WaterQualityDataset *dataset_read_csv(const char *filename);
int                  dataset_write_csv(const WaterQualityDataset *ds,
                                       const char *filename);
int                  dataset_write_overview(const WaterQualityDataset *ds,
                                            const char *filename);
int dataset_write_module2_overview(const WaterQualityDataset *ds,
                                   const char *filename,
                                   const OutlierStats *ostats,
                                   int missing_filled,
                                   const char *stage_desc);

/* ── 二进制输入输出 ─────────────────────────────────────── */
int  dataset_write_binary(const WaterQualityDataset *ds,
                          const char *filename);
WaterQualityDataset *dataset_read_binary(const char *filename);

/* ── 随机访问 ──────────────────────────────────────────── */
WaterQualityRecord *dataset_get_record(WaterQualityDataset *ds, int record_id);
int                 dataset_get_index_by_id(const WaterQualityDataset *ds,
                                            int record_id);

/* ── 预处理（旧版） ─────────────────────────────────────── */
void dataset_mark_outliers_iqr(WaterQualityDataset *ds, double multiplier);
void dataset_fill_missing_mean(WaterQualityDataset *ds);
int  dataset_remove_invalid(WaterQualityDataset *ds);

/* ── 模块 2.1：异常值检测（范围法） ─────────────────────── */
OutlierStats dataset_detect_outliers_range(WaterQualityDataset *ds);
int          dataset_handle_outliers(WaterQualityDataset *ds, OutlierStats *stats);

/* ── 模块 2.2：缺失值填充（均值逼近法） ─────────────────── */
int dataset_fill_missing_approximation(WaterQualityDataset *ds, int n, int m);

/* ── 模块 2.2：移动平均滤波 ─────────────────────────────── */
int  dataset_moving_average(const WaterQualityDataset *ds, int param_idx,
                            int window, WaterQualityDataset **out_ds);
double dataset_param_std(const WaterQualityDataset *ds, int param_idx,
                         int *out_count);

/* ── 统计 ──────────────────────────────────────────────── */
void dataset_print_overview(const WaterQualityDataset *ds);

/* 参数索引辅助函数 */
const char *param_name(int idx);
double      param_value(const WaterQualityRecord *r, int idx);
int         param_validate(double v, int idx);
void        param_range(int idx, double *min_val, double *max_val);

#endif
