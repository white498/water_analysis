#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "dataset.h"
#include "query.h"
#include "modify.h"
#include "backup.h"

/* 模块二：数据预处理子菜单 */
static void preprocess_menu(WaterQualityDataset *ds) {
    /* 用于概览文件的持久化统计信息 */
    OutlierStats ostats;
    memset(&ostats, 0, sizeof(ostats));
    int ostats_valid = 0;
    int missing_filled = 0;

    while (1) {
        printf("\n===== 模块二: 数据预处理 =====\n");
        printf(" 1. 2.1 异常值检测 (范围法)\n");
        printf(" 2. 2.1 异常值处理 (删除/标记 + 均值逼近填充)\n");
        printf(" 3. 2.2 缺失值填充 (均值逼近法)\n");
        printf(" 4. 2.2 数据滤波 (移动平均法)\n");
        printf(" 0. 返回主菜单\n");

        int choice = get_int_input("请选择: ", 0, 4);
        switch (choice) {
        /* ── 2.1 异常值检测 (范围法) ── */
        case 1: {
            printf("\n===== 2.1 异常值检测 (数值合理范围法) =====\n");
            printf("参数合理范围:\n");
            printf("  水温: %.0f ~ %.0f ℃\n", TEMP_MIN, TEMP_MAX);
            printf("  盐度: %.0f ~ %.0f PSU\n", SALINITY_MIN, SALINITY_MAX);
            printf("  pH:   %.1f ~ %.1f\n", PH_MIN, PH_MAX);
            printf("  DO:   %.0f ~ %.0f mg/l\n", DO_MIN, DO_MAX);
            printf("  降水量: %.0f ~ %.0f mm\n", PRECIP_MIN, PRECIP_MAX);
            printf("  气温: %.0f ~ %.0f ℃\n", AIR_TEMP_MIN, AIR_TEMP_MAX);

            ostats = dataset_detect_outliers_range(ds);
            ostats_valid = 1;

            printf("\n── 数据概览 ──\n");
            printf("  总记录数:         %d\n", ostats.total_records);
            printf("  异常数据记录数:   %d\n", ostats.anomaly_records);
            printf("  有效数据记录数:   %d\n", ostats.valid_records);
            if (ostats.anomaly_start_id > 0)
                printf("  异常数据时间跨度: ID %d ~ ID %d\n",
                       ostats.anomaly_start_id, ostats.anomaly_end_id);
            else
                printf("  异常数据时间跨度: 无\n");
            printf("  异常数据错误参数个数:\n");
            const char *pnames[] = {"水温", "盐度", "pH", "溶解氧", "降水量", "气温"};
            for (int i = 0; i < PARAM_COUNT; i++)
                printf("    %s: %d\n", pnames[i], ostats.error_param_counts[i]);
            printf("────────────────\n");
            pause_and_continue();
            break;
        }

        /* ── 2.1 异常值处理 ── */
        case 2: {
            if (!ostats_valid) {
                printf("\n请先执行\"2.1 异常值检测\"再进行处理。\n");
                pause_and_continue();
                break;
            }
            if (ostats.anomaly_records == 0) {
                printf("\n未检测到异常数据，无需处理。\n");
                pause_and_continue();
                break;
            }
            printf("\n===== 2.1 异常值处理 =====\n");
            printf("处理规则:\n");
            printf("  1) 异常参数 >= 3 个 → 整条记录删除\n");
            printf("  2) 异常参数 <  3 个 → 删除异常值, 后续用均值逼近法填充\n");
            if (!get_yes_no("确认执行异常值处理?")) break;

            int deleted = dataset_handle_outliers(ds, &ostats);
            printf("\n处理完成:\n");
            printf("  删除记录数 (异常>=3): %d\n", ostats.deleted_count);
            printf("  修复记录数 (异常<3):  %d (异常值已标记, 待均值逼近法填充)\n",
                   ostats.repaired_count);

            /* 自动用均值逼近法填充已修复的记录 */
            if (ostats.repaired_count > 0) {
                printf("\n自动对修复记录执行均值逼近法填充...\n");
                int filled = dataset_fill_missing_approximation(ds, 10, 10);
                missing_filled += filled;
                printf("  本次填充缺失值: %d 个\n", filled);
            }

            /* 写入概览文件 */
            dataset_write_module2_overview(ds, "data_overview.txt",
                                           &ostats, missing_filled,
                                           "2.1异常值处理后");
            printf("数据概览已更新至 data_overview.txt\n");

            /* 保存修改到 data.csv */
            if (get_yes_no("是否保存修改到 data.csv？")) {
                if (dataset_write_csv(ds, DATA_FILE))
                    printf("数据已保存到 data.csv\n");
            }

            /* 分析讨论 */
            printf("\n── 2.1 分析讨论 ──\n");
            printf("常见异常值处理方法:\n");
            printf("  1) 直接删除法: 简单但可能损失信息\n");
            printf("  2) 均值/中位数替换: 保留记录但可能引入偏差\n");
            printf("  3) 分位数截断(Winsorize): 截断到边界值\n");
            printf("  4) 模型预测法: 基于回归模型预测替换\n");
            printf("  5) 混合策略: 根据异常程度采用不同策略\n");
            printf("\n本模块采用混合策略的合理性:\n");
            printf("  - 多参数同时异常通常表明传感器系统故障或传输严重错误,\n");
            printf("    整条记录不可靠, 删除是合理的。\n");
            printf("  - 少数参数异常可能是偶发干扰, 其他参数仍有效,\n");
            printf("    删除整条记录会造成信息浪费。\n");
            printf("  - 均值逼近法利用相邻记录而非全局均值填充,\n");
            printf("    能保持时间序列的局部特征和连续性。\n");
            printf("  - 该策略在数据质量和信息保留之间取得良好平衡。\n");
            pause_and_continue();
            break;
        }

        /* ── 2.2 缺失值填充 (均值逼近法) ── */
        case 3: {
            printf("\n===== 2.2 缺失值处理 (均值逼近法) =====\n");
            printf("公式: ai = (a_{i-n} + a_{i+m}) / 2\n");
            printf("默认 n = m = 10\n");
            printf("边界处理: 单方向无有效值时用另一方向; 均无则用全集均值\n");

            int n = get_int_input("请输入 n (默认10): ", 1, 100);
            clear_input_buffer();
            int m = get_int_input("请输入 m (默认10): ", 1, 100);
            clear_input_buffer();

            /* 填充前统计缺失值数量 */
            int missing_before = 0;
            for (int i = 0; i < ds->count; i++) {
                if (!ds->records[i].valid) continue;
                if (ds->records[i].temp          == INVALID_VALUE) missing_before++;
                if (ds->records[i].salinity      == INVALID_VALUE) missing_before++;
                if (ds->records[i].ph            == INVALID_VALUE) missing_before++;
                if (ds->records[i].do_value      == INVALID_VALUE) missing_before++;
                if (ds->records[i].precipitation == INVALID_VALUE) missing_before++;
                if (ds->records[i].air_temp      == INVALID_VALUE) missing_before++;
            }
            printf("处理前缺失值总数: %d\n", missing_before);

            if (missing_before == 0) {
                printf("当前无缺失值, 无需填充。\n");
                pause_and_continue();
                break;
            }

            int filled = dataset_fill_missing_approximation(ds, n, m);
            missing_filled += filled;
            printf("均值逼近法填充完成: 共填充 %d 个缺失值 (n=%d, m=%d)\n", filled, n, m);
            printf("当前有效记录: %d\n", ds->valid_count);
            pause_and_continue();
            break;
        }

        /* ── 2.2 数据滤波 (移动平均法) ── */
        case 4: {
            printf("\n===== 2.2 数据滤波 (移动平均法) =====\n");
            printf("公式: y_i = (1/N) * sum_{j=-k}^{k} x_{i+j},  N = 2k+1\n");
            printf("滤波参数: 水温, 溶解氧, pH, 盐度\n");
            printf("窗口大小: 3, 5, 7, 9, 11\n");
            printf("边界处理: 截断窗口, 仅使用有效数据\n\n");

            int filter_params[] = {PARAM_TEMP, PARAM_DO, PARAM_PH, PARAM_SALINITY};
            const char *fp_names[] = {"水温", "溶解氧", "pH", "盐度"};
            int windows[] = {3, 5, 7, 9, 11};
            int nw = sizeof(windows) / sizeof(windows[0]);

            /* 计算原始标准差 */
            printf("── 原始数据标准差 ──\n");
            for (int pi = 0; pi < 4; pi++) {
                int cnt;
                double s = dataset_param_std(ds, filter_params[pi], &cnt);
                printf("  %s: %.6f (样本数=%d)\n", fp_names[pi], s, cnt);
            }

            printf("\n── 滤波后标准差变化 ──\n");
            printf("%-8s %6s %12s %12s %10s\n", "参数", "窗口", "滤波前std", "滤波后std", "噪声减少");

            for (int wi = 0; wi < nw; wi++) {
                int w = windows[wi];
                /* 为当前窗口创建滤波数据集 */
                WaterQualityDataset *filtered = NULL;

                /* 对各参数应用移动平均并跟踪结果 */
                WaterQualityDataset *work_ds = NULL;
                dataset_moving_average(ds, PARAM_TEMP, w, &work_ds);

                for (int pi = 0; pi < 4; pi++) {
                    WaterQualityDataset *fd = NULL;
                    dataset_moving_average(ds, filter_params[pi], w, &fd);

                    int cnt_before, cnt_after;
                    double std_before = dataset_param_std(ds, filter_params[pi], &cnt_before);
                    double std_after  = dataset_param_std(fd, filter_params[pi], &cnt_after);

                    double reduction = (std_before > 0)
                        ? (1.0 - std_after / std_before) * 100.0 : 0.0;
                    printf("%-8s %6d %12.6f %12.6f %9.2f%%\n",
                           fp_names[pi], w, std_before, std_after, reduction);

                    /* 保存第一个滤波结果用于导出 */
                    if (pi == 0 && fd) {
                        char fname[MAX_PATH_LEN];
                        sprintf(fname, "filtered_w%d.csv", w);

                        /* 应用全部 4 个参数得到完整滤波数据集 */
                        WaterQualityDataset *full = NULL;
                        dataset_moving_average(ds, PARAM_TEMP, w, &full);
                        if (full) {
                            WaterQualityDataset *tmp;
                            /* 将剩余参数的滤波结果应用到完整数据集 */
                            for (int pj = 1; pj < 4; pj++) {
                                dataset_moving_average(ds, filter_params[pj], w, &tmp);
                                if (tmp) {
                                    /* 复制该参数的滤波值 */
                                    for (int i = 0; i < full->count && i < tmp->count; i++) {
                                        if (!full->records[i].valid) continue;
                                        switch (filter_params[pj]) {
                                            case PARAM_DO:   full->records[i].do_value = tmp->records[i].do_value; break;
                                            case PARAM_PH:   full->records[i].ph       = tmp->records[i].ph;       break;
                                            case PARAM_SALINITY: full->records[i].salinity = tmp->records[i].salinity; break;
                                        }
                                    }
                                    dataset_free(tmp);
                                }
                            }
                            dataset_write_csv(full, fname);
                            printf("  → 滤波结果已保存: %s\n", fname);
                            dataset_free(full);
                        }
                        dataset_free(fd);
                    } else if (fd) {
                        dataset_free(fd);
                    }
                }
                if (work_ds) dataset_free(work_ds);
            }

            /* 分析讨论 */
            printf("\n── 2.2 滤波分析讨论 ──\n");
            printf("窗口大小与噪声抑制的关系:\n");
            printf("  - 窗口越大, 平滑效果越强, 噪声标准差降低越多;\n");
            printf("  - 但窗口过大会导致信号失真, 丢失短期变化细节;\n");
            printf("  - 窗口过小则噪声抑制不足;\n");
            printf("  - 通常窗口大小应权衡平滑效果与细节保留。\n");
            printf("\n最佳滤波窗口选择原则:\n");
            printf("  - 观察噪声减少率随窗口增大的变化趋势;\n");
            printf("  - 当增大窗口带来的噪声抑制增益明显减小时,\n");
            printf("    该拐点附近即为较优窗口;\n");
            printf("  - 对于水产养殖水质监测, 通常窗口5~7较为合适,\n");
            printf("    既能有效滤除高频噪声, 又能保留日变化等关键趋势。\n");
            printf("\n建议: 根据实际数据的标准差变化表, 选择噪声减少率\n");
            printf("      增幅收窄的窗口作为最佳滤波窗口。\n");
            pause_and_continue();
            break;
        }

        case 0:
            return;
        }
    }
}

/* 主菜单 */
int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    printf("========================================\n");
    printf(" 海水养殖水质数据分析系统 v1.0\n");
    printf("========================================\n\n");

    /* 自动读取 CSV 数据文件 */
    WaterQualityDataset *ds = NULL;
    if (!file_exists(DATA_DIR)) make_directory(DATA_DIR);
    const char *csv_paths[] = {
        DATA_FILE,
        NULL
    };
    for (int i = 0; csv_paths[i]; i++) {
        ds = dataset_read_csv(csv_paths[i]);
        if (ds) {
            dataset_write_overview(ds, "data_overview.txt");
            printf("数据概览已保存至 data_overview.txt\n");
            break;
        }

    }
    if (!ds) {
        printf("错误: 无法自动读取数据文件，程序退出。\n");
        pause_and_continue();
        return 1;
    }

    int running = 1;
    while (running) {
        printf("\n===== 主 菜 单 =====\n");

        printf(" 1. 数据查询与浏览\n");
        printf(" 2. 数据预处理\n");
        printf(" 3. 数据修改与删除\n");
        printf(" 4. 数据备份与恢复\n");
        printf(" 0. 退出系统\n");
        printf("当前数据: %d 条记录 (有效 %d 条)\n", ds->count, ds->valid_count);

        int choice = get_int_input("请选择: ", 0, 4);

        switch (choice) {
        case 1: query_menu(ds); break;
        case 2: preprocess_menu(ds); break;
        case 3: modify_menu(ds); break;
        case 4: backup_menu(ds); break;
        case 0:
            if (get_yes_no("退出前是否保存到 data.csv？")) {
                if (dataset_write_csv(ds, DATA_FILE))
                    printf("数据已保存到 data.csv\n");
            }
            running = 0;
            printf("\n感谢使用，再见！\n");
            break;
        }
    }

    dataset_free(ds);
    return 0;
}
