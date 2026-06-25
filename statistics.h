#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdio.h>
#include <stdlib.h>
#include "common.h"

#define STAT_PARAM_COUNT 6

typedef struct {
    double mean[STAT_PARAM_COUNT];
    double min[STAT_PARAM_COUNT];
    double max[STAT_PARAM_COUNT];
    double stddev[STAT_PARAM_COUNT];
    long   count; /* 有效样本数 */
} BasicStats;

/* 计算基本统计量（Welford 单次在线算法） */
void compute_basic_stats(WaterQualityDataset *ds, BasicStats *out_stats);

/* 生成并追加预警到 alert_path（"a" 模式）*/
void generate_alerts(WaterQualityDataset *ds, const char *alert_path);

/* 皮尔逊相关系数函数 */
double pearson(const double *x, const double *y, int n);

/* 计算相关矩阵并返回最强正/负相关索引 */
void compute_correlation_matrix(WaterQualityDataset *ds, double corr[STAT_PARAM_COUNT][STAT_PARAM_COUNT],
                                int *idx_out_pos_a, int *idx_out_pos_b,
                                int *idx_out_neg_a, int *idx_out_neg_b);

/* 写统计报告（覆盖模式 "w"），并可引用 alert_path */
void write_stat_report(WaterQualityDataset *ds, const char *report_path, const char *alert_path);

#endif /* STATISTICS_H */
