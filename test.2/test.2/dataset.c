#define _CRT_SECURE_NO_WARNINGS
#include "dataset.h"

/* ══════════════════════════════════════════════════════════════
   内存管理
   ══════════════════════════════════════════════════════════════ */

WaterQualityDataset *dataset_create(void) {
    WaterQualityDataset *ds = (WaterQualityDataset *)calloc(1, sizeof(*ds));
    if (!ds) return NULL;
    ds->capacity = INITIAL_CAPACITY;
    ds->records = (WaterQualityRecord *)calloc(ds->capacity, sizeof(WaterQualityRecord));
    if (!ds->records) {
        free(ds);
        return NULL;
    }
    ds->count       = 0;
    ds->total_read  = 0;
    ds->valid_count = 0;
    ds->source_file[0] = '\0';
    return ds;
}

void dataset_free(WaterQualityDataset *ds) {
    if (!ds) return;
    free(ds->records);
    free(ds);
}

/* ══════════════════════════════════════════════════════════════
   CSV 读取
   ══════════════════════════════════════════════════════════════ */

static int ensure_capacity(WaterQualityDataset *ds) {
    if (ds->count < ds->capacity) return 1;
    int new_cap = ds->capacity * 2;
    WaterQualityRecord *tmp = (WaterQualityRecord *)
        realloc(ds->records, new_cap * sizeof(WaterQualityRecord));
    if (!tmp) return 0;
    ds->records  = tmp;
    ds->capacity = new_cap;
    return 1;
}

/*
   解析一行 CSV 数据为一条记录。
   正确处理空字段（连续逗号）。
   空字段 / "NA" / "null" / "NAN" / "N/A" → INVALID_VALUE 标记。
   返回 1 成功，列数不足 6 时返回 0。
*/
static int parse_csv_line(const char *line, WaterQualityRecord *r) {
    double vals[7];
    for (int i = 0; i < 7; i++) vals[i] = INVALID_VALUE;

    int field = 0;
    const char *p = line;
    char token[256];
    int  tok_len = 0;

    while (*p && field < 7) {
        if (*p == ',') {
            token[tok_len] = '\0';
            {
                char *s = token;
                while (*s == ' ' || *s == '\t') s++;
                char *e = s + strlen(s) - 1;
                while (e > s && (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n')) {
                    *e = '\0'; e--;
                }
                if (s[0] != '\0' &&
                    strcmp(s, "NA") != 0 &&
                    strcmp(s, "null") != 0 &&
                    strcmp(s, "NULL") != 0 &&
                    strcmp(s, "NAN") != 0 &&
                    strcmp(s, "nan") != 0 &&
                    strcmp(s, "N/A") != 0)
                {
                    char *endptr;
                    vals[field] = strtod(s, &endptr);
                    if (endptr == s) vals[field] = INVALID_VALUE;
                }
            }
            field++;
            tok_len = 0;
            p++;
        } else if (*p == '\r' || *p == '\n') {
            break;
        } else {
            if (tok_len < 255) token[tok_len++] = *p;
            p++;
        }
    }

    /* 处理最后一个字段（无尾部逗号） */
    if (field < 7) {
        token[tok_len] = '\0';
        char *s = token;
        while (*s == ' ' || *s == '\t') s++;
        char *e = s + strlen(s) - 1;
        while (e > s && (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n')) {
            *e = '\0'; e--;
        }
        if (s[0] != '\0' &&
            strcmp(s, "NA") != 0 &&
            strcmp(s, "null") != 0 &&
            strcmp(s, "NULL") != 0 &&
            strcmp(s, "NAN") != 0 &&
            strcmp(s, "nan") != 0 &&
            strcmp(s, "N/A") != 0)
        {
            char *endptr;
            vals[field] = strtod(s, &endptr);
            if (endptr == s) vals[field] = INVALID_VALUE;
        }
        field++;
    }

    /* 兼容两种格式：7 列含 RecordID，6 列不含 */
    if (field >= 7) {
        r->temp          = vals[1];
        r->salinity      = vals[2];
        r->ph            = vals[3];
        r->do_value      = vals[4];
        r->precipitation = vals[5];
        r->air_temp      = vals[6];
    } else if (field >= 6) {
        r->temp          = vals[0];
        r->salinity      = vals[1];
        r->ph            = vals[2];
        r->do_value      = vals[3];
        r->precipitation = vals[4];
        r->air_temp      = vals[5];
    } else {
        return 0;
    }

    r->valid = 1;
    return 1;
}

WaterQualityDataset *dataset_read_csv(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[错误] 无法打开文件: %s\n", filename);
        return NULL;
    }

    WaterQualityDataset *ds = dataset_create();
    if (!ds) {
        fclose(fp);
        printf("[错误] 内存分配失败。\n");
        return NULL;
    }
    strncpy(ds->source_file, filename, MAX_PATH_LEN - 1);

    char line[MAX_LINE_LENGTH];
    int  line_num = 0;

    /* 跳过表头行 */
    if (!fgets(line, sizeof(line), fp)) {
        printf("[错误] 文件为空。\n");
        fclose(fp);
        dataset_free(ds);
        return NULL;
    }
    line_num++;

    /* 读取数据行 */
    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* 跳过空行 */
        if (line[0] == '\n' || line[0] == '\r') continue;

        if (!ensure_capacity(ds)) {
            printf("[错误] 内存不足，在第 %d 行停止读取。\n", line_num);
            break;
        }

        WaterQualityRecord *r = &ds->records[ds->count];
        memset(r, 0, sizeof(*r));

        if (parse_csv_line(line, r)) {
            r->record_id = line_num;  /* 以 CSV 行号作为记录 ID */
            ds->count++;
            ds->total_read++;
            if (r->valid) ds->valid_count++;
        } else {
            printf("[警告] 第 %d 行格式错误，已跳过。\n", line_num);
        }
    }

    fclose(fp);
    printf("\n文件读取完成: 共读取 %d 行，有效记录 %d 条。\n",
           ds->total_read, ds->valid_count);
    return ds;
}

/* ══════════════════════════════════════════════════════════════
   CSV 写入
   ══════════════════════════════════════════════════════════════ */

int dataset_write_csv(const WaterQualityDataset *ds, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("[错误] 无法创建文件: %s\n", filename);
        return 0;
    }

    fprintf(fp, "RecordID,Temp(degC),Salinity(PSU),pH,DO(mg/l),precipitation(mm),Air_temp(degC)\n");

    for (int i = 0; i < ds->count; i++) {
        const WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;
        fprintf(fp, "%d,%.6f,%.6f,%.7f,%.7f,%.8f,%.8f\n",
                r->record_id,
                r->temp, r->salinity, r->ph, r->do_value,
                r->precipitation, r->air_temp);
    }

    fclose(fp);
    return 1;
}

int dataset_write_overview(const WaterQualityDataset *ds, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return 0;

    fprintf(fp, "===== 数据概览 =====\n");
    fprintf(fp, "数据源:   %s\n", ds->source_file);
    fprintf(fp, "总记录数: %d\n", ds->total_read);
    fprintf(fp, "有效记录: %d\n", ds->valid_count);
    fprintf(fp, "无效记录: %d\n", ds->total_read - ds->valid_count);
    fprintf(fp, "记录范围: ID %d ~ %d\n",
            ds->count > 0 ? ds->records[0].record_id : 0,
            ds->count > 0 ? ds->records[ds->count - 1].record_id : 0);
    fprintf(fp, "=====================\n");

    fclose(fp);
    return 1;
}

/*
   写入模块二综合概览文件。
   包含异常值检测、缺失值填充和滤波的所有统计信息。
*/
int dataset_write_module2_overview(const WaterQualityDataset *ds,
                                   const char *filename,
                                   const OutlierStats *ostats,
                                   int missing_filled,
                                   const char *stage_desc) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return 0;

    fprintf(fp, "===== 数据概览 (%s) =====\n", stage_desc ? stage_desc : "模块二");
    fprintf(fp, "数据源:     %s\n", ds->source_file);
    fprintf(fp, "总记录数:   %d\n", ds->total_read);
    fprintf(fp, "有效记录数: %d\n", ds->valid_count);
    fprintf(fp, "无效记录数: %d\n", ds->total_read - ds->valid_count);
    fprintf(fp, "记录范围:   ID %d ~ %d\n",
            ds->count > 0 ? ds->records[0].record_id : 0,
            ds->count > 0 ? ds->records[ds->count - 1].record_id : 0);
    fprintf(fp, "\n");

    if (ostats) {
        fprintf(fp, "异常值检测\n");
        fprintf(fp, "异常数据记录数:   %d\n", ostats->anomaly_records);
        fprintf(fp, "有效数据记录数:   %d\n", ostats->valid_records);
        if (ostats->anomaly_start_id > 0)
            fprintf(fp, "异常数据时间跨度: ID %d ~ ID %d\n",
                    ostats->anomaly_start_id, ostats->anomaly_end_id);
        else
            fprintf(fp, "异常数据时间跨度: 无\n");
        fprintf(fp, "异常数据错误参数个数:\n");
        const char *pnames[] = {"水温", "盐度", "pH", "溶解氧", "降水量", "气温"};
        for (int i = 0; i < PARAM_COUNT; i++)
            fprintf(fp, "  %s: %d\n", pnames[i], ostats->error_param_counts[i]);
        fprintf(fp, "\n异常值处理\n");
        fprintf(fp, "修复异常值记录数: %d  (异常参数<3, 删除异常值后均值逼近填充)\n", ostats->repaired_count);
        fprintf(fp, "删除异常值记录数: %d  (异常参数>=3, 整条记录删除)\n", ostats->deleted_count);
        fprintf(fp, "\n分析讨论\n");
        fprintf(fp, "异常值处理方法分析:\n");
        fprintf(fp, "  常见异常值处理方法包括:\n");
        fprintf(fp, "  1) 直接删除法: 删除包含异常值的记录, 简单但可能损失信息;\n");
        fprintf(fp, "  2) 均值/中位数替换: 用统计量替换异常值, 保留记录但可能引入偏差;\n");
        fprintf(fp, "  3) 分位数截断(Winsorize): 将超过分位数的值截断到边界值;\n");
        fprintf(fp, "  4) 模型预测法: 基于回归模型预测合理值进行替换;\n");
        fprintf(fp, "  5) 混合策略: 根据异常程度采用不同策略。\n");
        fprintf(fp, "\n");
        fprintf(fp, "  本模块采用混合策略: 异常参数>=3时整条删除(避免大范围不可靠数据),\n");
        fprintf(fp, "  异常参数<3时删除异常值后用均值逼近法填充(保留记录主体信息,\n");
        fprintf(fp, "  利用相邻数据的连续性恢复合理值)。\n");
        fprintf(fp, "\n");
        fprintf(fp, "  合理性分析:\n");
        fprintf(fp, "  - 当多个参数同时异常时,通常表明传感器系统性故障或传输严重错误,\n");
        fprintf(fp, "    此时整条记录不可靠,删除是合理的。\n");
        fprintf(fp, "  - 当仅少数参数异常时,可能是偶发干扰或局部传感器问题,\n");
        fprintf(fp, "    其他参数数据仍然有效,删除整条记录会造成信息浪费。\n");
        fprintf(fp, "  - 使用均值逼近法(基于相邻记录)而非全局均值填充,\n");
        fprintf(fp, "    能更好地保持时间序列的局部特征和连续性。\n");
        fprintf(fp, "  - 该策略在数据质量和信息保留之间取得了良好平衡,\n");
        fprintf(fp, "    适用于水产养殖监测数据的实际场景。\n");
        fprintf(fp, "\n");
    }

    if (missing_filled > 0) {
        fprintf(fp, "--- 2.2 缺失值处理 (均值逼近法) ---\n");
        fprintf(fp, "填充缺失值个数: %d\n", missing_filled);
    }

    fprintf(fp, "=========================================\n");
    fclose(fp);
    return 1;
}

/* ══════════════════════════════════════════════════════════════
   二进制输入输出
   ══════════════════════════════════════════════════════════════ */

int dataset_write_binary(const WaterQualityDataset *ds, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("[错误] 无法创建二进制文件: %s\n", filename);
        return 0;
    }

    /* 写入头部：记录数量 */
    int count = ds->count;
    fwrite(&count, sizeof(int), 1, fp);

    /* 写入所有记录 */
    for (int i = 0; i < ds->count; i++) {
        fwrite(&ds->records[i], sizeof(WaterQualityRecord), 1, fp);
    }

    fclose(fp);
    return 1;
}

WaterQualityDataset *dataset_read_binary(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("[错误] 无法打开二进制文件: %s\n", filename);
        return NULL;
    }

    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1) {
        printf("[错误] 文件格式错误。\n");
        fclose(fp);
        return NULL;
    }

    WaterQualityDataset *ds = dataset_create();
    if (!ds) { fclose(fp); return NULL; }

    /* 确保容量足够 */
    while (ds->capacity < count) {
        int new_cap = ds->capacity * 2;
        WaterQualityRecord *tmp = (WaterQualityRecord *)
            realloc(ds->records, new_cap * sizeof(WaterQualityRecord));
        if (!tmp) { dataset_free(ds); fclose(fp); return NULL; }
        ds->records  = tmp;
        ds->capacity = new_cap;
    }

    ds->count = count;
    size_t read_count = fread(ds->records, sizeof(WaterQualityRecord), count, fp);
    fclose(fp);

    if ((int)read_count != count) {
        printf("[警告] 预期 %d 条记录，实际读取 %zu 条。\n", count, read_count);
        ds->count = (int)read_count;
    }

    /* 重新计算统计信息 */
    ds->total_read  = ds->count;
    ds->valid_count = 0;
    for (int i = 0; i < ds->count; i++) {
        if (ds->records[i].valid) ds->valid_count++;
    }

    return ds;
}

/* ══════════════════════════════════════════════════════════════
   随机访问
   ══════════════════════════════════════════════════════════════ */

int dataset_get_index_by_id(const WaterQualityDataset *ds, int record_id) {
    for (int i = 0; i < ds->count; i++) {
        if (ds->records[i].record_id == record_id && ds->records[i].valid)
            return i;
    }
    return -1;
}

WaterQualityRecord *dataset_get_record(WaterQualityDataset *ds, int record_id) {
    int idx = dataset_get_index_by_id(ds, record_id);
    return (idx >= 0) ? &ds->records[idx] : NULL;
}

/* ══════════════════════════════════════════════════════════════
   预处理 — 异常值检测（IQR 方法）
   ══════════════════════════════════════════════════════════════ */

static int cmp_double(const void *a, const void *b) {
    double d = *(const double *)a - *(const double *)b;
    return (d > 0) - (d < 0);
}

void dataset_mark_outliers_iqr(WaterQualityDataset *ds, double multiplier) {
    if (ds->count == 0) return;

    double *values = (double *)malloc(ds->count * sizeof(double));
    if (!values) return;

    for (int p = PARAM_TEMP; p < PARAM_COUNT; p++) {
        int n = 0;
        for (int i = 0; i < ds->count; i++) {
            if (!ds->records[i].valid) continue;
            double v = param_value(&ds->records[i], p);
            if (v != INVALID_VALUE) values[n++] = v;
        }
        if (n < 4) continue;

        qsort(values, n, sizeof(double), cmp_double);

        double q1 = values[n / 4];
        double q3 = values[3 * n / 4];
        double iqr = q3 - q1;
        double lower = q1 - multiplier * iqr;
        double upper = q3 + multiplier * iqr;

        int marked = 0;
        for (int i = 0; i < ds->count; i++) {
            if (!ds->records[i].valid) continue;
            double v = param_value(&ds->records[i], p);
            if (v != INVALID_VALUE && (v < lower || v > upper)) {
                ds->records[i].valid = 0;
                marked++;
            }
        }
        if (marked > 0) {
            printf("  %s: 标记 %d 个异常值 (范围 [%.4f, %.4f])\n",
                   param_name(p), marked, lower, upper);
        }
    }
    free(values);

    /* 重新计算有效记录数 */
    ds->valid_count = 0;
    for (int i = 0; i < ds->count; i++)
        if (ds->records[i].valid) ds->valid_count++;
}

/* ══════════════════════════════════════════════════════════════
   预处理 — 用列均值填充缺失值
   ══════════════════════════════════════════════════════════════ */

void dataset_fill_missing_mean(WaterQualityDataset *ds) {
    for (int p = PARAM_TEMP; p < PARAM_COUNT; p++) {
        double sum = 0.0;
        int    n   = 0;
        for (int i = 0; i < ds->count; i++) {
            if (!ds->records[i].valid) continue;
            double v = param_value(&ds->records[i], p);
            if (v != INVALID_VALUE) { sum += v; n++; }
        }
        if (n == 0) continue;
        double mean = sum / n;

        int filled = 0;
        for (int i = 0; i < ds->count; i++) {
            if (!ds->records[i].valid) continue;
            double v = param_value(&ds->records[i], p);
            if (v == INVALID_VALUE) {
                switch (p) {
                    case PARAM_TEMP:          ds->records[i].temp          = mean; break;
                    case PARAM_SALINITY:      ds->records[i].salinity      = mean; break;
                    case PARAM_PH:            ds->records[i].ph            = mean; break;
                    case PARAM_DO:            ds->records[i].do_value      = mean; break;
                    case PARAM_PRECIPITATION: ds->records[i].precipitation = mean; break;
                    case PARAM_AIR_TEMP:      ds->records[i].air_temp      = mean; break;
                }
                filled++;
            }
        }
        if (filled > 0)
            printf("  %s: 用均值 %.4f 填充 %d 个缺失值\n", param_name(p), mean, filled);
    }
}

int dataset_remove_invalid(WaterQualityDataset *ds) {
    int removed = 0;
    int write_pos = 0;
    for (int i = 0; i < ds->count; i++) {
        if (ds->records[i].valid) {
            if (write_pos != i)
                ds->records[write_pos] = ds->records[i];
            write_pos++;
        } else {
            removed++;
        }
    }
    ds->count       = write_pos;
    ds->valid_count = write_pos;
    return removed;
}

/* ══════════════════════════════════════════════════════════════
   模块 2.1 — 异常值检测（固定范围法）
   ══════════════════════════════════════════════════════════════ */

/*
   使用 common.h 中定义的固定有效范围检测异常值。
   返回统计数据：总数/异常数/有效数、时间跨度、各参数异常数。
   不修改记录 — 仅统计和报告。
*/
OutlierStats dataset_detect_outliers_range(WaterQualityDataset *ds) {
    OutlierStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.total_records = ds->count;
    stats.anomaly_start_id = -1;
    stats.anomaly_end_id   = -1;

    for (int i = 0; i < ds->count; i++) {
        WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;

        int err_count = 0;
        if (r->temp          != INVALID_VALUE && !validate_temp(r->temp))           { err_count++; stats.error_param_counts[PARAM_TEMP]++; }
        if (r->salinity      != INVALID_VALUE && !validate_salinity(r->salinity))   { err_count++; stats.error_param_counts[PARAM_SALINITY]++; }
        if (r->ph            != INVALID_VALUE && !validate_ph(r->ph))               { err_count++; stats.error_param_counts[PARAM_PH]++; }
        if (r->do_value      != INVALID_VALUE && !validate_do(r->do_value))         { err_count++; stats.error_param_counts[PARAM_DO]++; }
        if (r->precipitation != INVALID_VALUE && !validate_precipitation(r->precipitation)) { err_count++; stats.error_param_counts[PARAM_PRECIPITATION]++; }
        if (r->air_temp      != INVALID_VALUE && !validate_air_temp(r->air_temp))   { err_count++; stats.error_param_counts[PARAM_AIR_TEMP]++; }

        if (err_count > 0) {
            stats.anomaly_records++;
            if (stats.anomaly_start_id < 0) stats.anomaly_start_id = r->record_id;
            stats.anomaly_end_id = r->record_id;
        }
    }

    stats.valid_records = ds->valid_count - stats.anomaly_records;
    return stats;
}

/*
   按模块 2.1 规则处理异常值：
   - 记录中异常参数 >= 3 个 → 整条删除 (valid = 0)
   - 记录中异常参数 <  3 个 → 将异常值设为 INVALID_VALUE
     （后续由均值逼近法填充）
   返回删除的记录数。
   更新 stats->deleted_count 和 stats->repaired_count。
*/
int dataset_handle_outliers(WaterQualityDataset *ds, OutlierStats *stats) {
    int deleted = 0;
    int repaired = 0;

    for (int i = 0; i < ds->count; i++) {
        WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;

        /* 统计此条记录的异常参数 */
        int err_flags[PARAM_COUNT] = {0};
        int err_count = 0;
        if (r->temp          != INVALID_VALUE && !validate_temp(r->temp))           { err_flags[PARAM_TEMP] = 1; err_count++; }
        if (r->salinity      != INVALID_VALUE && !validate_salinity(r->salinity))   { err_flags[PARAM_SALINITY] = 1; err_count++; }
        if (r->ph            != INVALID_VALUE && !validate_ph(r->ph))               { err_flags[PARAM_PH] = 1; err_count++; }
        if (r->do_value      != INVALID_VALUE && !validate_do(r->do_value))         { err_flags[PARAM_DO] = 1; err_count++; }
        if (r->precipitation != INVALID_VALUE && !validate_precipitation(r->precipitation)) { err_flags[PARAM_PRECIPITATION] = 1; err_count++; }
        if (r->air_temp      != INVALID_VALUE && !validate_air_temp(r->air_temp))   { err_flags[PARAM_AIR_TEMP] = 1; err_count++; }

        if (err_count >= 3) {
            /* 删除整条记录 */
            r->valid = 0;
            deleted++;
        } else if (err_count > 0) {
            /* 将异常值设为 INVALID_VALUE */
            if (err_flags[PARAM_TEMP])          r->temp          = INVALID_VALUE;
            if (err_flags[PARAM_SALINITY])      r->salinity      = INVALID_VALUE;
            if (err_flags[PARAM_PH])            r->ph            = INVALID_VALUE;
            if (err_flags[PARAM_DO])            r->do_value      = INVALID_VALUE;
            if (err_flags[PARAM_PRECIPITATION]) r->precipitation = INVALID_VALUE;
            if (err_flags[PARAM_AIR_TEMP])      r->air_temp      = INVALID_VALUE;
            repaired++;
        }
    }

    /* 重新计算有效记录数 */
    ds->valid_count = 0;
    for (int i = 0; i < ds->count; i++)
        if (ds->records[i].valid) ds->valid_count++;

    if (stats) {
        stats->deleted_count  = deleted;
        stats->repaired_count = repaired;
    }
    return deleted;
}

/* ══════════════════════════════════════════════════════════════
   模块 2.2 — 缺失值填充（均值逼近法）
   ai = (a_{i-n} + a_{i+m}) / 2
   默认 n = m = 10。
   - 一侧无有效值则用另一侧。
   - 两侧均无则用列全局均值。
   ══════════════════════════════════════════════════════════════ */

int dataset_fill_missing_approximation(WaterQualityDataset *ds, int n, int m) {
    int total_filled = 0;

    for (int p = PARAM_TEMP; p < PARAM_COUNT; p++) {
        /* 计算全局均值作为回退值 */
        double sum = 0.0;
        int    cnt = 0;
        for (int i = 0; i < ds->count; i++) {
            if (!ds->records[i].valid) continue;
            double v = param_value(&ds->records[i], p);
            if (v != INVALID_VALUE) { sum += v; cnt++; }
        }
        double global_mean = (cnt > 0) ? sum / cnt : 0.0;

        int param_filled = 0;
        for (int i = 0; i < ds->count; i++) {
            if (!ds->records[i].valid) continue;
            double v = param_value(&ds->records[i], p);
            if (v != INVALID_VALUE) continue;

            /* 向前搜索 n 步，查找有效值 */
            double back_val = INVALID_VALUE;
            for (int j = 1; j <= n && (i - j) >= 0; j++) {
                if (!ds->records[i - j].valid) continue;
                double bv = param_value(&ds->records[i - j], p);
                if (bv != INVALID_VALUE) { back_val = bv; break; }
            }

            /* 向后搜索 m 步，查找有效值 */
            double fwd_val = INVALID_VALUE;
            for (int j = 1; j <= m && (i + j) < ds->count; j++) {
                if (!ds->records[i + j].valid) continue;
                double fv = param_value(&ds->records[i + j], p);
                if (fv != INVALID_VALUE) { fwd_val = fv; break; }
            }

            double fill_val;
            if (back_val != INVALID_VALUE && fwd_val != INVALID_VALUE)
                fill_val = (back_val + fwd_val) / 2.0;
            else if (back_val != INVALID_VALUE)
                fill_val = back_val;
            else if (fwd_val != INVALID_VALUE)
                fill_val = fwd_val;
            else
                fill_val = global_mean;

            switch (p) {
                case PARAM_TEMP:          ds->records[i].temp          = fill_val; break;
                case PARAM_SALINITY:      ds->records[i].salinity      = fill_val; break;
                case PARAM_PH:            ds->records[i].ph            = fill_val; break;
                case PARAM_DO:            ds->records[i].do_value      = fill_val; break;
                case PARAM_PRECIPITATION: ds->records[i].precipitation = fill_val; break;
                case PARAM_AIR_TEMP:      ds->records[i].air_temp      = fill_val; break;
            }
            param_filled++;
        }
        total_filled += param_filled;
    }
    return total_filled;
}

/* ══════════════════════════════════════════════════════════════
   模块 2.2 — 移动平均滤波
   y_i = (1/N) * sum_{j=-k}^{k} x_{i+j},  N = 2k+1
   边界处理：在边缘处截断窗口。
   返回 1 成功，0 失败。
   ══════════════════════════════════════════════════════════════ */

int dataset_moving_average(const WaterQualityDataset *ds, int param_idx,
                           int window, WaterQualityDataset **out_ds) {
    if (!ds || window < 1 || window % 2 == 0) return 0;

    /* 创建输出数据集（副本） */
    WaterQualityDataset *result = dataset_create();
    if (!result) return 0;
    result->count       = ds->count;
    result->total_read  = ds->total_read;
    result->valid_count = ds->valid_count;
    strncpy(result->source_file, ds->source_file, MAX_PATH_LEN - 1);

    /* 确保容量足够 */
    while (result->capacity < ds->count) {
        int new_cap = result->capacity * 2;
        WaterQualityRecord *tmp = (WaterQualityRecord *)
            realloc(result->records, new_cap * sizeof(WaterQualityRecord));
        if (!tmp) { dataset_free(result); return 0; }
        result->records  = tmp;
        result->capacity = new_cap;
    }

    int k = window / 2;
    for (int i = 0; i < ds->count; i++) {
        result->records[i] = ds->records[i];  /* 复制记录 */
        if (!ds->records[i].valid) continue;

        double sum = 0.0;
        int    cnt = 0;
        int    start = (i - k > 0) ? i - k : 0;
        int    end   = (i + k < ds->count) ? i + k : ds->count - 1;

        for (int j = start; j <= end; j++) {
            if (!ds->records[j].valid) continue;
            double v = param_value(&ds->records[j], param_idx);
            if (v != INVALID_VALUE) { sum += v; cnt++; }
        }

        if (cnt > 0) {
            double avg = sum / cnt;
            switch (param_idx) {
                case PARAM_TEMP:          result->records[i].temp          = avg; break;
                case PARAM_SALINITY:      result->records[i].salinity      = avg; break;
                case PARAM_PH:            result->records[i].ph            = avg; break;
                case PARAM_DO:            result->records[i].do_value      = avg; break;
                case PARAM_PRECIPITATION: result->records[i].precipitation = avg; break;
                case PARAM_AIR_TEMP:      result->records[i].air_temp      = avg; break;
            }
        }
    }

    *out_ds = result;
    return 1;
}

/*
   计算某一参数的标准差（有效、非 INVALID_VALUE 的记录）。
   若 out_count 非空，同时返回样本数量。
*/
double dataset_param_std(const WaterQualityDataset *ds, int param_idx,
                         int *out_count) {
    double sum = 0.0, sum_sq = 0.0;
    int n = 0;
    for (int i = 0; i < ds->count; i++) {
        if (!ds->records[i].valid) continue;
        double v = param_value(&ds->records[i], param_idx);
        if (v != INVALID_VALUE && v == v) { sum += v; sum_sq += v * v; n++; }
    }
    if (out_count) *out_count = n;
    if (n < 2) return 0.0;
    double mean = sum / n;
    double var = sum_sq / n - mean * mean;
    if (var < 0.0 || var != var) var = 0.0;
    return sqrt(var);
}

/* ══════════════════════════════════════════════════════════════
   统计
   ══════════════════════════════════════════════════════════════ */

void dataset_print_overview(const WaterQualityDataset *ds) {
    printf("\n╔══════════════════════ 数据概览 ═══════════════════════╗\n");
    printf("  数据源:     %s\n", ds->source_file[0] ? ds->source_file : "(无)");
    printf("  总记录数:   %d\n", ds->total_read);
    printf("  有效记录:   %d\n", ds->valid_count);
    printf("  当前容量:   %d\n", ds->capacity);
    if (ds->count > 0) {
        printf("  首条 ID:    %d\n", ds->records[0].record_id);
        printf("  末条 ID:    %d\n", ds->records[ds->count - 1].record_id);
    }
    printf("╚════════════════════════════════════════════════════════╝\n");
}

/* ══════════════════════════════════════════════════════════════
   参数辅助函数
   ══════════════════════════════════════════════════════════════ */

const char *param_name(int idx) {
    static const char *names[] = {
        "水温(℃)", "盐度(PSU)", "pH", "溶解氧(mg/l)", "降水量(mm)", "气温(℃)"
    };
    return (idx >= 0 && idx < PARAM_COUNT) ? names[idx] : "未知";
}

double param_value(const WaterQualityRecord *r, int idx) {
    switch (idx) {
        case PARAM_TEMP:          return r->temp;
        case PARAM_SALINITY:      return r->salinity;
        case PARAM_PH:            return r->ph;
        case PARAM_DO:            return r->do_value;
        case PARAM_PRECIPITATION: return r->precipitation;
        case PARAM_AIR_TEMP:      return r->air_temp;
        default: return INVALID_VALUE;
    }
}

int param_validate(double v, int idx) {
    double lo, hi;
    param_range(idx, &lo, &hi);
    return v >= lo && v <= hi;
}

void param_range(int idx, double *min_val, double *max_val) {
    static const double ranges[PARAM_COUNT][2] = {
        {TEMP_MIN,      TEMP_MAX},
        {SALINITY_MIN,  SALINITY_MAX},
        {PH_MIN,        PH_MAX},
        {DO_MIN,        DO_MAX},
        {PRECIP_MIN,    PRECIP_MAX},
        {AIR_TEMP_MIN,  AIR_TEMP_MAX},
    };
    if (idx >= 0 && idx < PARAM_COUNT) {
        *min_val = ranges[idx][0];
        *max_val = ranges[idx][1];
    } else {
        *min_val = *max_val = 0;
    }
}
