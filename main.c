#define _CRT_SECURE_NO_WARNINGS
#include "auth.h"
#include "backup.h"
#include "dataset.h"
#include "modify.h"
#include "predict.h"
#include "query.h"
#include "statistics.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int auth_login_screen(void);
void show_main_menu(WaterQualityDataset *ds);

/* 模块二：数据预处理子菜单 */
static void preprocess_menu(WaterQualityDataset *ds) {
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
                printf("  异常数据时间跨度: ID %d ~ ID %d\n", ostats.anomaly_start_id,
                    ostats.anomaly_end_id);
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
            if (!get_yes_no("确认执行异常值处理?"))
                break;

            int deleted = dataset_handle_outliers(ds, &ostats);
            printf("\n处理完成:\n");
            printf("  删除记录数 (异常>=3): %d\n", ostats.deleted_count);
            printf("  修复记录数 (异常<3):  %d (异常值已标记, 待均值逼近法填充)\n",
                ostats.repaired_count);

            if (ostats.repaired_count > 0) {
                printf("\n自动对修复记录执行均值逼近法填充...\n");
                int filled = dataset_fill_missing_approximation(ds, 10, 10);
                missing_filled += filled;
                printf("  本次填充缺失值: %d 个\n", filled);
            }

            dataset_write_module2_overview(ds, "data_overview.txt", &ostats,
                missing_filled, "2.1异常值处理后");
            printf("数据概览已更新至 data_overview.txt\n");

            if (get_yes_no("是否保存修改到 data.csv？")) {
                if (dataset_write_csv(ds, DATA_FILE))
                    printf("数据已保存到 data.csv\n");
            }

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

        case 3: {
            printf("\n===== 2.2 缺失值处理 (均值逼近法) =====\n");
            printf("公式: ai = (a_{i-n} + a_{i+m}) / 2\n");
            printf("默认 n = m = 10\n");
            printf("边界处理: 单方向无有效值时用另一方向; 均无则用全集均值\n");

            int n = get_int_input("请输入 n (默认10): ", 1, 100);
            clear_input_buffer();
            int m = get_int_input("请输入 m (默认10): ", 1, 100);
            clear_input_buffer();

            int missing_before = 0;
            for (int i = 0; i < ds->count; i++) {
                if (!ds->records[i].valid)
                    continue;
                if (ds->records[i].temp == INVALID_VALUE)
                    missing_before++;
                if (ds->records[i].salinity == INVALID_VALUE)
                    missing_before++;
                if (ds->records[i].ph == INVALID_VALUE)
                    missing_before++;
                if (ds->records[i].do_value == INVALID_VALUE)
                    missing_before++;
                if (ds->records[i].precipitation == INVALID_VALUE)
                    missing_before++;
                if (ds->records[i].air_temp == INVALID_VALUE)
                    missing_before++;
            }
            printf("处理前缺失值总数: %d\n", missing_before);

            if (missing_before == 0) {
                printf("当前无缺失值, 无需填充。\n");
                pause_and_continue();
                break;
            }

            int filled = dataset_fill_missing_approximation(ds, n, m);
            missing_filled += filled;
            printf("均值逼近法填充完成: 共填充 %d 个缺失值 (n=%d, m=%d)\n", filled, n,
                m);
            printf("当前有效记录: %d\n", ds->valid_count);
            pause_and_continue();
            break;
        }

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

            printf("── 原始数据标准差 ──\n");
            for (int pi = 0; pi < 4; pi++) {
                int cnt;
                double s = dataset_param_std(ds, filter_params[pi], &cnt);
                printf("  %s: %.6f (样本数=%d)\n", fp_names[pi], s, cnt);
            }

            printf("\n── 滤波后标准差变化 ──\n");
            printf("%-8s %6s %12s %12s %10s\n", "参数", "窗口", "滤波前std",
                "滤波后std", "噪声减少");

            for (int wi = 0; wi < nw; wi++) {
                int w = windows[wi];

                WaterQualityDataset *work_ds = NULL;
                dataset_moving_average(ds, PARAM_TEMP, w, &work_ds);

                for (int pi = 0; pi < 4; pi++) {
                    WaterQualityDataset *fd = NULL;
                    dataset_moving_average(ds, filter_params[pi], w, &fd);

                    int cnt_before, cnt_after;
                    double std_before =
                        dataset_param_std(ds, filter_params[pi], &cnt_before);
                    double std_after =
                        dataset_param_std(fd, filter_params[pi], &cnt_after);

                    double reduction =
                        (std_before > 0) ? (1.0 - std_after / std_before) * 100.0 : 0.0;
                    printf("%-8s %6d %12.6f %12.6f %9.2f%%\n", fp_names[pi], w,
                        std_before, std_after, reduction);

                    if (pi == 0 && fd) {
                        char fname[MAX_PATH_LEN];
                        sprintf(fname, "filtered_w%d.csv", w);

                        WaterQualityDataset *full = NULL;
                        dataset_moving_average(ds, PARAM_TEMP, w, &full);
                        if (full) {
                            WaterQualityDataset *tmp;
                            for (int pj = 1; pj < 4; pj++) {
                                dataset_moving_average(ds, filter_params[pj], w, &tmp);
                                if (tmp) {
                                    for (int i = 0; i < full->count && i < tmp->count; i++) {
                                        if (!full->records[i].valid)
                                            continue;
                                        switch (filter_params[pj]) {
                                        case PARAM_DO:
                                            full->records[i].do_value = tmp->records[i].do_value;
                                            break;
                                        case PARAM_PH:
                                            full->records[i].ph = tmp->records[i].ph;
                                            break;
                                        case PARAM_SALINITY:
                                            full->records[i].salinity = tmp->records[i].salinity;
                                            break;
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
                if (work_ds)
                    dataset_free(work_ds);
            }

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

/* 模块三：统计分析子菜单 */
static void statistics_menu(WaterQualityDataset *ds) {
    const char *pnames[] = {"水温", "盐度", "pH", "溶解氧", "降水量", "气温"};

    while (1) {
        printf("\n===== 统计分析 =====\n");
        printf(" 1. 基本统计量\n");
        printf(" 2. 分段统计与预警\n");
        printf(" 3. 皮尔逊相关性分析\n");
        printf(" 0. 返回主菜单\n");

        int choice = get_int_input("请选择: ", 0, 3);
        switch (choice) {
        case 1: {
            printf("\n===== 基本统计量 (Welford算法) =====\n");
            BasicStats stats;
            compute_basic_stats(ds, &stats);
            printf("有效样本数: %ld\n", stats.count);
            printf("%-8s %12s %12s %12s %12s\n", "参数", "均值", "最小值", "最大值",
                "标准差");
            for (int p = 0; p < STAT_PARAM_COUNT; p++) {
                printf("%-8s %12.6f %12.6f %12.6f %12.6f\n", pnames[p], stats.mean[p],
                    stats.min[p], stats.max[p], stats.stddev[p]);
            }
            printf("\n── 分析讨论 ──\n");
            printf("Welford算法优势:\n");
            printf("  - 单次遍历 O(N)，无需存储全部数据\n");
            printf("  - 数值稳定性优于传统公式法\n");
            printf("  - 标准差越大说明该参数波动越剧烈\n");
            pause_and_continue();
            break;
        }
        case 2: {
            printf("\n===== 分段统计与预警 =====\n");
            printf("预警规则:\n");
            printf("  - DO缺氧预警: 每日 03:00~05:00 平均DO < 3.0(严重) / < "
                "4.0(亚缺氧)\n");
            printf("  - 盐度突变预警: 1h变化率 > 2.0 / 24h累计降幅 > 5.0\n");

            generate_alerts(ds, "alerts.txt");
            printf("\n预警已写入 alerts.txt\n");

            FILE *check = fopen("alerts.txt", "r");
            if (check) {
                fseek(check, 0, SEEK_END);
                long sz = ftell(check);
                fclose(check);
                if (sz > 0) {
                    printf("发现预警信息, 内容预览:\n");
                    char line[512];
                    FILE *f = fopen("alerts.txt", "r");
                    if (f) {
                        int shown = 0;
                        while (fgets(line, sizeof(line), f) && shown < 20) {
                            printf("  %s", line);
                            shown++;
                        }
                        fclose(f);
                    }
                } else {
                    printf("未触发预警，数据正常。\n");
                }
            }
            pause_and_continue();
            break;
        }
        case 3: {
            printf("\n===== 皮尔逊相关性分析 =====\n");
            double corr[STAT_PARAM_COUNT][STAT_PARAM_COUNT];
            int pa, pb, na, nb;
            compute_correlation_matrix(ds, corr, &pa, &pb, &na, &nb);

            printf("相关矩阵 (6×6):\n");
            printf("%-8s", "");
            for (int p = 0; p < STAT_PARAM_COUNT; p++)
                printf("%-8s", pnames[p]);
            printf("\n");
            for (int i = 0; i < STAT_PARAM_COUNT; i++) {
                printf("%-8s", pnames[i]);
                for (int j = 0; j < STAT_PARAM_COUNT; j++)
                    printf("%-8.4f", corr[i][j]);
                printf("\n");
            }

            printf("\n最强正相关: %s - %s (%.4f)\n", pnames[pa], pnames[pb],
                corr[pa][pb]);
            printf("最强负相关: %s - %s (%.4f)\n", pnames[na], pnames[nb],
                corr[na][nb]);

            double r_temp_do = corr[0][3];
            double r_ph_do = corr[2][3];
            double r_temp_air = corr[0][5];
            double r_temp_sal = corr[0][1];

            double abs_temp_do = fabs(r_temp_do);
            double abs_temp_air = fabs(r_temp_air);

#define GET_LEVEL(v) \
            ((v) >= 0.8 ? "极强" \
            : ((v) >= 0.6 ? "强" \
            : ((v) >= 0.4 ? "中等" \
            : ((v) >= 0.2 ? "弱" : "极弱/无"))))
            const char *level_temp_do = GET_LEVEL(abs_temp_do);
#undef GET_LEVEL

            printf("\n── 分析讨论 ──\n");
            printf("相关系数解读标准:\n");
            printf("  |r| >= 0.8: 极强相关\n");
            printf("  0.6 <= |r| < 0.8: 强相关\n");
            printf("  0.4 <= |r| < 0.6: 中等相关\n");
            printf("  0.2 <= |r| < 0.4: 弱相关\n");
            printf("  |r| < 0.2: 极弱/无相关\n");

            printf("\n重点参数相关性分析:\n");
            printf("1. 水温 - 溶解氧 (r = %.4f):\n", r_temp_do);
            printf("   理论关系: 水温升高会降低氧气溶解度，应呈负相关。\n");
            printf("   实际分析: 当前相关系数为%.4f，属于%s相关。\n", r_temp_do,
                level_temp_do);
            printf("   原因讨论: "
                "数据可能受传感器噪声、增氧机干扰或短期波动影响，建议滤波后再分析"
                "。\n");

            printf("\n2. pH - 溶解氧 (r = %.4f):\n", r_ph_do);
            printf("   理论关系: 光合作用消耗CO2使pH升高同时产氧，应呈正相关。\n");
            printf("   实际分析: 当前相关系数为%.4f。\n", r_ph_do);
            printf("   原因讨论: "
                "养殖水体中生物活动复杂，且传感器精度可能影响相关性识别。\n");

            printf("\n3. 水温 - 气温 (r = %.4f):\n", r_temp_air);
            printf("   理论关系: 气温是水温的主要热源，应呈强正相关。\n");
            printf("   实际分析: 当前相关系数为%.4f", r_temp_air);
            if (abs_temp_air < 0.6)
                printf("，异常偏低。\n");
            else
                printf("。\n");
            printf("   原因讨论: "
                "海水热容量大，水温变化滞后于气温；或传感器位置/校准问题导致。\n");

            printf("\n4. 水温 - 盐度 (r = %.4f):\n", r_temp_sal);
            printf("   理论关系: 暴雨稀释盐度同时降低水温，可能呈弱正相关或无关。\n");
            printf("   实际分析: 当前相关系数为%.4f。\n", r_temp_sal);
            printf("   原因讨论: "
                "本数据集盐度变化可能主要受换水/降雨影响，与水温无直接耦合。\n");

            printf("\n数据质量说明:\n");
            printf("本次数据集皮尔逊相关系数普遍极低（|r| < 0.05），可能原因:\n");
            printf("  - 原始数据包含大量噪声和异常值\n");
            printf("  - 5分钟高频采集的短期波动淹没了长期趋势\n");
            printf("  - 建议先进行异常值清洗和移动平均滤波后再做相关性分析\n");

            write_stat_report(ds, "stat_report.txt", "alerts.txt");
            printf("\n完整统计报告已保存至 stat_report.txt\n");
            pause_and_continue();
            break;
        }
        case 0:
            return;
        }
    }
}

int auth_login_screen(void) {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int attempts = 0;
    const int max_attempts = 3;

    auth_init_users();

    while (attempts < max_attempts) {
        system("cls");
        printf("========================================\n");
        printf("    海水养殖水质数据分析系统 v1.0\n");
        printf("========================================\n");
        printf("            用户登录\n");
        printf("========================================\n");
        printf("剩余尝试次数: %d\n", max_attempts - attempts);
        printf("\n用户名: ");
        scanf("%s", username);
        printf("密码: ");
        scanf("%s", password);

        User *user = auth_login(username, password);
        if (user) {
            system("cls");
            printf("========================================\n");
            printf("    海水养殖水质数据分析系统 v1.0\n");
            printf("========================================\n");
            printf("登录成功！\n");
            printf("用户: %s\n", user->username);
            printf("角色: %s\n", role_to_string(user->role));
            printf("========================================\n");
            pause_and_continue();
            return 1;
        }

        attempts++;
        printf("\n用户名或密码错误！\n");
        if (attempts < max_attempts) {
            pause_and_continue();
        }
    }

    printf("\n登录失败次数过多，系统将退出。\n");
    pause_and_continue();
    return 0;
}

// 菜单展示
void show_main_menu(WaterQualityDataset *ds) {
    int running = 1;
    while (running) {
        system("cls");
        printf("========================================\n");
        printf("    海水养殖水质数据分析系统 v1.0\n");
        printf("========================================\n");
        printf("当前用户: %s (%s)\n", g_current_user->username,
            role_to_string(g_current_user->role));
        printf("当前数据: %d 条记录 (有效 %d 条)\n", ds->count, ds->valid_count);
        printf("========================================\n\n");

        int menu_items[10] = {0};
        int item_count = 0;

        if (auth_has_permission(g_current_user->role, "view_query") || 
            g_current_user->role == ROLE_ADMIN) {
            printf("[1] 数据基础操作\n");
            menu_items[item_count++] = 1;
        }
        //管理员显示预处理
        if (g_current_user->role == ROLE_ADMIN) {
            printf("[2] 数据预处理\n");
            menu_items[item_count++] = 2;
        }
        if (g_current_user->role == ROLE_ADMIN) {
            printf("[3] 统计分析\n");
            menu_items[item_count++] = 3;
        }
        if (auth_has_permission(g_current_user->role, "view_predict") || 
            g_current_user->role == ROLE_ADMIN) {
            printf("[4] 预测分析\n");
            menu_items[item_count++] = 4;
        }
        if (auth_has_permission(g_current_user->role, "view_overview") || 
            g_current_user->role == ROLE_ADMIN) {
            printf("[5] 查看数据概览\n");
            menu_items[item_count++] = 5;
        }
        if (g_current_user->role == ROLE_ADMIN) {
            printf("[6] 查看预警报告\n");
            menu_items[item_count++] = 6;
        }
        if (auth_has_permission(g_current_user->role, "view_statistics") || 
            g_current_user->role == ROLE_ADMIN) {
            printf("[7] 查看分析报告\n");
            menu_items[item_count++] = 7;
        }
        if (g_current_user->role == ROLE_ADMIN) {
            printf("[8] 数据备份与恢复\n");
            menu_items[item_count++] = 8;
        }
        printf("[9] 清屏\n");
        menu_items[item_count++] = 9;
        printf("[0] 退出系统\n");
        menu_items[item_count++] = 0;

        printf("========================================\n");
        
        int choice = get_int_input("请选择操作 (0-9): ", 0, 9);

        //错误提示
        switch (choice) {
        case 1:
            if (auth_has_permission(g_current_user->role, "view_query") || 
                g_current_user->role == ROLE_ADMIN) {
                query_menu(ds);
            } else {
                printf("您没有此操作权限！\n");
                pause_and_continue();
            }
            break;
        case 2:
            if (g_current_user->role == ROLE_ADMIN) {
                preprocess_menu(ds);
            } else {
                printf("您没有此操作权限！\n");
                pause_and_continue();
            }
            break;
        case 3:
            if (g_current_user->role == ROLE_ADMIN) {
                statistics_menu(ds);
            } else {
                printf("您没有此操作权限！\n");
                pause_and_continue();
            }
            break;
        case 4:
            if (auth_has_permission(g_current_user->role, "view_predict") || 
                g_current_user->role == ROLE_ADMIN) {
                predict_menu(ds);
            } else {
                printf("您没有此操作权限！\n");
                pause_and_continue();
            }
            break;
        case 5:
            if (auth_has_permission(g_current_user->role, "view_overview") || 
                g_current_user->role == ROLE_ADMIN) {
                FILE *f = fopen("data_overview.txt", "r");
                if (f) {
                    char line[2048];
                    printf("\n===== 数据概览 =====\n");
                    while (fgets(line, sizeof(line), f)) {
                        printf("%s", line);
                    }
                    fclose(f);
                } else {
                    printf("数据概览文件不存在！\n");
                }
                pause_and_continue();
            } else {
                printf("您没有此操作权限！\n");
                pause_and_continue();
            }
            break;
        case 6:
            if (g_current_user->role == ROLE_ADMIN) {
                FILE *f = fopen("alerts.txt", "r");
                if (f) {
                    char line[2048];
                    printf("\n===== 预警报告 =====\n");
                    while (fgets(line, sizeof(line), f)) {
                        printf("%s", line);
                    }
                    fclose(f);
                } else {
                    printf("预警报告文件不存在！\n");
                }
                pause_and_continue();
            } else {
                printf("您没有此操作权限！\n");
                pause_and_continue();
            }
            break;
        case 7:
            if (auth_has_permission(g_current_user->role, "view_statistics") || 
                g_current_user->role == ROLE_ADMIN) {
                FILE *f = fopen("stat_report.txt", "r");
                if (f) {
                    char line[2048];
                    printf("\n===== 分析报告 =====\n");
                    while (fgets(line, sizeof(line), f)) {
                        printf("%s", line);
                    }
                    fclose(f);
                } else {
                    printf("分析报告文件不存在！\n");
                }
                pause_and_continue();
            } else {
                printf("您没有此操作权限！\n");
                pause_and_continue();
            }
            break;
        case 8:
            if (g_current_user->role == ROLE_ADMIN) {
                backup_menu(ds);
            } else {
                printf("您没有此操作权限！\n");
                pause_and_continue();
            }
            break;
        case 9:
            system("cls");
            break;
        case 0:
            if (get_yes_no("退出前是否保存到 data.csv？")) {
                if (dataset_write_csv(ds, DATA_FILE))
                    printf("数据已保存到 data.csv\n");
            }
            running = 0;
            printf("\n感谢使用，再见！\n");
            break;
        default:
            printf("输入无效，请重新选择！\n");
            pause_and_continue();
        }
    }
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (!auth_login_screen()) {
        return 1;
    }

    WaterQualityDataset *ds = NULL;
    if (!file_exists(DATA_DIR))
        make_directory(DATA_DIR);
    const char *csv_paths[] = {DATA_FILE, NULL};
    for (int i = 0; csv_paths[i]; i++) {
        ds = dataset_read_csv(csv_paths[i]);
        if (ds) {
            dataset_write_overview(ds, "data_overview.txt");
            break;
        }
    }
    if (!ds) {
        printf("错误: 无法自动读取数据文件，程序退出。\n");
        pause_and_continue();
        return 1;
    }

    show_main_menu(ds);
    dataset_free(ds);
    return 0;
}