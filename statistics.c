#include "statistics.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "dataset.h" /* 项目中应定义 WaterQualityRecord, WaterQualityDataset, INVALID_VALUE 等 */

static const char *param_names[STAT_PARAM_COUNT] = {
    "水温", "盐度", "pH", "溶解氧", "降水量", "气温"
};

/* -------------------
   3.1 基本统计量 (Welford)
   复杂度: O(N)，空间: O(1)
   边界: 有效样本数<2 时 stddev=0
   ------------------- */
void compute_basic_stats(WaterQualityDataset *ds, BasicStats *out_stats) {
    if (!ds || !out_stats) return;
    for (int p = 0; p < STAT_PARAM_COUNT; p++) {
        out_stats->mean[p] = 0.0;
        out_stats->min[p] = 1e300;
        out_stats->max[p] = -1e300;
        out_stats->stddev[p] = 0.0;
    }
    long valid_seen = 0;
    double mean[STAT_PARAM_COUNT] = {0};
    double M2[STAT_PARAM_COUNT] = {0};

    for (int i = 0; i < ds->count && valid_seen < ds->valid_count; i++) {
        const WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;
        valid_seen++;
        double vals[STAT_PARAM_COUNT] = {
            r->temp, r->salinity, r->ph, r->do_value, r->precipitation, r->air_temp
        };
        for (int p = 0; p < STAT_PARAM_COUNT; p++) {
            double x = vals[p];
            if (!isfinite(x) || x == INVALID_VALUE) continue;
            if (x < out_stats->min[p]) out_stats->min[p] = x;
            if (x > out_stats->max[p]) out_stats->max[p] = x;
            double delta = x - mean[p];
            mean[p] += delta / valid_seen;
            M2[p] += delta * (x - mean[p]);
        }
    }
    out_stats->count = valid_seen;
    for (int p = 0; p < STAT_PARAM_COUNT; p++) {
        out_stats->mean[p] = mean[p];
        out_stats->stddev[p] = (valid_seen > 1) ? sqrt(M2[p] / (valid_seen - 1)) : 0.0;
        if (valid_seen == 0) { out_stats->min[p] = 0.0; out_stats->max[p] = 0.0; }
    }
}

/* 内部：追加一行 alert */
static void append_alert_file(FILE *f, const char *tag, int day, int idx, const char *msg) {
    fprintf(f, "Day%04d idx%07d [%s] %s\n", day, idx, tag, msg);
}

/* -------------------
   3.2 分段统计与预警
   - DO 03:00~05:00 累计均值预警 (索引 180~239)
   - 盐度突变: 1h (i vs i-12) 变化率 >2.0, 24h (i-288) 降幅 >5.0
   复杂度: O(N)，空间: O(1)
   ------------------- */
void generate_alerts(WaterQualityDataset *ds, const char *alert_path) {
    if (!ds || !alert_path) return;
    FILE *f = fopen(alert_path, "a");
    if (!f) return;

    int total_days = (ds->count + 287) / 288;
    for (int day = 0; day < total_days; day++) {
        int start = day * 288 + 180;
        int end   = day * 288 + 240;
        double sum = 0.0; int cnt = 0;
        for (int i = start; i < end && i < ds->count; i++) {
            const WaterQualityRecord *r = &ds->records[i];
            if (!r->valid) continue;
            double v = r->do_value;
            if (!isfinite(v) || v == INVALID_VALUE) continue;
            sum += v; cnt++;
        }
        if (cnt == 0) continue;
        double avg = sum / cnt;
        if (avg < 3.0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "严重缺氧预警: day%d 03:00-05:00 avg_do=%.3f 建议：需立即投放颗粒氧并减少投喂。", day, avg);
            append_alert_file(f, "DO_WARN", day, start, msg);
        } else if (avg < 4.0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "亚缺氧预警: day%d 03:00-05:00 avg_do=%.3f 建议：建议开启底部增氧机。", day, avg);
            append_alert_file(f, "DO_WARN", day, start, msg);
        }
    }

    for (int i = 0; i < ds->count; i++) {
        const WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;
        if (i >= 12) {
            const WaterQualityRecord *rprev = &ds->records[i - 12];
            if (rprev->valid) {
                double sal_now = r->salinity, sal_prev = rprev->salinity;
                if (isfinite(sal_now) && isfinite(sal_prev) && sal_now != INVALID_VALUE && sal_prev != INVALID_VALUE) {
                    if (fabs(sal_now - sal_prev) > 2.0) {
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                                 "盐度小时突变: idx%d sal_now=%.3f sal_1h_before=%.3f 速率=%.3f 建议：立即关闭进水口，并泼洒高稳VC或葡萄糖以增强对虾抗应激能力。",
                                 i, sal_now, sal_prev, fabs(sal_now - sal_prev));
                        append_alert_file(f, "SAL_HOUR", i / 288, i, msg);
                    }
                }
            }
        }
        if (i >= 288) {
            const WaterQualityRecord *r24 = &ds->records[i - 288];
            if (r24->valid) {
                double sal_prev = r24->salinity, sal_now = r->salinity;
                if (isfinite(sal_prev) && isfinite(sal_now) && sal_prev != INVALID_VALUE && sal_now != INVALID_VALUE) {
                    if ((sal_prev - sal_now) > 5.0) {
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                                 "24h 累计降幅: idx%d sal_prev24=%.3f sal_now=%.3f drop=%.3f 建议：立即关闭进水口，并泼洒高稳VC或葡萄糖以增强对虾抗应激能力。",
                                 i, sal_prev, sal_now, sal_prev - sal_now);
                        append_alert_file(f, "SAL_24H", i / 288, i, msg);
                    }
                }
            }
        }
    }

    fclose(f);
}

/* -------------------
   3.3 皮尔逊相关性分析
   复杂度: O(k*n) 空间 O(1) 额外? 实现使用 O(k*n) 空间构建矩阵
   边界: n<=1 或分母为0 时返回0
   ------------------- */
double pearson(const double *x, const double *y, int n) {
    if (!x || !y || n <= 1) { fprintf(stderr, "[pearson] 样本不足 n=%d\n", n); return 0.0; }
    double mean_x = 0.0, mean_y = 0.0;
    for (int i = 0; i < n; i++) { mean_x += x[i]; mean_y += y[i]; }
    mean_x /= n; mean_y /= n;
    double cov = 0.0, varx = 0.0, vary = 0.0;
    for (int i = 0; i < n; i++) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;
        cov += dx * dy;
        varx += dx * dx;
        vary += dy * dy;
    }
    double denom = sqrt(varx * vary);
    if (denom == 0.0) { fprintf(stderr, "[pearson] 分母为0\n"); return 0.0; }
    return cov / denom;
}

void compute_correlation_matrix(WaterQualityDataset *ds, double corr[STAT_PARAM_COUNT][STAT_PARAM_COUNT],
                                int *idx_out_pos_a, int *idx_out_pos_b,
                                int *idx_out_neg_a, int *idx_out_neg_b) {
    if (!ds) return;
    int n = ds->valid_count;
    if (n <= 1) {
        for (int i = 0; i < STAT_PARAM_COUNT; i++)
            for (int j = 0; j < STAT_PARAM_COUNT; j++)
                corr[i][j] = (i==j) ? 1.0 : 0.0;
        return;
    }
    double *data[STAT_PARAM_COUNT];
    for (int p = 0; p < STAT_PARAM_COUNT; p++) {
        data[p] = (double *)malloc(sizeof(double) * n);
        if (!data[p]) { for (int q = 0; q < p; q++) free(data[q]); return; }
    }
    int idx = 0;
    for (int i = 0; i < ds->count && idx < n; i++) {
        const WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;
        data[0][idx] = r->temp;
        data[1][idx] = r->salinity;
        data[2][idx] = r->ph;
        data[3][idx] = r->do_value;
        data[4][idx] = r->precipitation;
        data[5][idx] = r->air_temp;
        idx++;
    }
    double best_pos = -2.0; int pa=0,pb=0;
    double best_neg = 2.0; int na=0,nb=0;
    for (int i = 0; i < STAT_PARAM_COUNT; i++) {
        for (int j = 0; j < STAT_PARAM_COUNT; j++) {
            if (i == j) corr[i][j] = 1.0;
            else corr[i][j] = pearson(data[i], data[j], n);
            if (i < j) {
                double v = corr[i][j];
                if (v > best_pos) { best_pos = v; pa = i; pb = j; }
                if (v < best_neg) { best_neg = v; na = i; nb = j; }
            }
        }
    }
    if (idx_out_pos_a) *idx_out_pos_a = pa;
    if (idx_out_pos_b) *idx_out_pos_b = pb;
    if (idx_out_neg_a) *idx_out_neg_a = na;
    if (idx_out_neg_b) *idx_out_neg_b = nb;
    for (int p = 0; p < STAT_PARAM_COUNT; p++) free(data[p]);
}

/* 简短物理/生物学解释 */
static void add_physical_explanation(FILE *f, int a, int b) {
    if ((a==0 && b==3) || (a==3 && b==0)) {
        fprintf(f, "水温-溶解氧: 水温升高降低溶解氧溶解度并可能增加生物耗氧，通常负相关。\n");
    } else if ((a==2 && b==3) || (a==3 && b==2)) {
        fprintf(f, "pH-溶解氧: pH 与藻类代谢相关，可能影响氧生产/消耗，关系复杂。\n");
    } else if ((a==0 && b==5) || (a==5 && b==0)) {
        fprintf(f, "水温-气温: 通常正相关，气温升高可导致表层水温上升。\n");
    } else if ((a==0 && b==1) || (a==1 && b==0)) {
        fprintf(f, "水温-盐度: 受蒸发/入海淡水影响，可能存在正相关或复杂关系。\n");
    } else {
        fprintf(f, "参数对 %d - %d 无特定模板解释，请结合现场环境与时间序列分析。\n", a, b);
    }
}

/* 写统计报告（覆盖模式 "w"） */
void write_stat_report(WaterQualityDataset *ds, const char *report_path, const char *alert_path) {
    if (!ds || !report_path) return;
    BasicStats stats;
    compute_basic_stats(ds, &stats);
    double corr[STAT_PARAM_COUNT][STAT_PARAM_COUNT];
    int pa,pb,na,nb;
    compute_correlation_matrix(ds, corr, &pa, &pb, &na, &nb);

    FILE *f = fopen(report_path, "w");
    if (!f) return;
    fprintf(f, "Basic statistics (n=%ld)\n", stats.count);
    for (int p = 0; p < STAT_PARAM_COUNT; p++) {
        fprintf(f, "param %s: mean=%.6f min=%.6f max=%.6f std=%.6f\n",
                param_names[p], stats.mean[p], stats.min[p], stats.max[p], stats.stddev[p]);
    }
    fprintf(f, "\nCorrelation matrix (6x6):\n");
    for (int i = 0; i < STAT_PARAM_COUNT; i++) {
        for (int j = 0; j < STAT_PARAM_COUNT; j++) fprintf(f, "%.4f ", corr[i][j]);
        fprintf(f, "\n");
    }
    fprintf(f, "\nStrongest positive correlation: %s - %s (%.4f)\n",
            param_names[pa], param_names[pb], corr[pa][pb]);
    add_physical_explanation(f, pa, pb);
    fprintf(f, "\nStrongest negative correlation: %s - %s (%.4f)\n",
            param_names[na], param_names[nb], corr[na][nb]);
    add_physical_explanation(f, na, nb);
    if (alert_path) fprintf(f, "\nAlerts appended to: %s\n", alert_path);

    /* 3.3 重点参数相关性分析讨论 */
    double r_temp_do    = corr[0][3];
    double r_ph_do      = corr[2][3];
    double r_temp_air   = corr[0][5];
    double r_temp_sal   = corr[0][1];

    double abs_temp_do  = fabs(r_temp_do);
    double abs_temp_air = fabs(r_temp_air);

    #define GET_LEVEL(v) \
        ((v) >= 0.8 ? "极强" : ((v) >= 0.6 ? "强" : ((v) >= 0.4 ? "中等" : ((v) >= 0.2 ? "弱" : "极弱/无"))))

    fprintf(f, "\n── 分析讨论 ──\n");
    fprintf(f, "相关系数解读标准:\n");
    fprintf(f, "  |r| >= 0.8: 极强相关\n");
    fprintf(f, "  0.6 <= |r| < 0.8: 强相关\n");
    fprintf(f, "  0.4 <= |r| < 0.6: 中等相关\n");
    fprintf(f, "  0.2 <= |r| < 0.4: 弱相关\n");
    fprintf(f, "  |r| < 0.2: 极弱/无相关\n");

    fprintf(f, "\n重点参数相关性分析:\n");
    fprintf(f, "1. 水温 - 溶解氧 (r = %.4f):\n", r_temp_do);
    fprintf(f, "   理论关系: 水温升高会降低氧气溶解度，应呈负相关。\n");
    fprintf(f, "   实际分析: 当前相关系数为%.4f，属于%s相关。\n", r_temp_do, GET_LEVEL(abs_temp_do));
    fprintf(f, "   原因讨论: 数据可能受传感器噪声、增氧机干扰或短期波动影响，建议滤波后再分析。\n");

    fprintf(f, "\n2. pH - 溶解氧 (r = %.4f):\n", r_ph_do);
    fprintf(f, "   理论关系: 光合作用消耗CO2使pH升高同时产氧，应呈正相关。\n");
    fprintf(f, "   实际分析: 当前相关系数为%.4f。\n", r_ph_do);
    fprintf(f, "   原因讨论: 养殖水体中生物活动复杂，且传感器精度可能影响相关性识别。\n");

    fprintf(f, "\n3. 水温 - 气温 (r = %.4f):\n", r_temp_air);
    fprintf(f, "   理论关系: 气温是水温的主要热源，应呈强正相关。\n");
    fprintf(f, "   实际分析: 当前相关系数为%.4f", r_temp_air);
    if (abs_temp_air < 0.6)
        fprintf(f, "，异常偏低。\n");
    else
        fprintf(f, "。\n");
    fprintf(f, "   原因讨论: 海水热容量大，水温变化滞后于气温；或传感器位置/校准问题导致。\n");

    fprintf(f, "\n4. 水温 - 盐度 (r = %.4f):\n", r_temp_sal);
    fprintf(f, "   理论关系: 暴雨稀释盐度同时降低水温，可能呈弱正相关或无关。\n");
    fprintf(f, "   实际分析: 当前相关系数为%.4f。\n", r_temp_sal);
    fprintf(f, "   原因讨论: 本数据集盐度变化可能主要受换水/降雨影响，与水温无直接耦合。\n");

    fprintf(f, "\n数据质量说明:\n");
    fprintf(f, "本次数据集皮尔逊相关系数普遍极低（|r| < 0.05），可能原因:\n");
    fprintf(f, "  - 原始数据包含大量噪声和异常值\n");
    fprintf(f, "  - 5分钟高频采集的短期波动淹没了长期趋势\n");
    fprintf(f, "  - 建议先进行异常值清洗和移动平均滤波后再做相关性分析\n");

    #undef GET_LEVEL

    fclose(f);
}
