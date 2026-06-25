#define _CRT_SECURE_NO_WARNINGS
#include "predict.h"
#include "dataset.h"
#include "common.h"
#include <math.h>

/*
 * 函数功能：根据预测参数枚举，提取水质结构体中对应监测指标的数据
 * 入参：
 *  r：水质监测记录结构体，包含水温、气温、pH、盐度、溶解氧等原始数据
 *  param：参数枚举，用来选择需要读取哪一项水质指标
 * 返回值：
 *  成功：对应指标的double数值
 *  失败：非法枚举值，返回INVALID_VALUE无效标记
 */
static double get_param_value(const WaterQualityRecord *r, PredictParam param) {
    switch (param) {
        case PREDICT_PARAM_TEMP:     return r->temp;        // 水温
        case PREDICT_PARAM_AIR_TEMP: return r->air_temp;    // 气温
        case PREDICT_PARAM_PH:       return r->ph;          // pH值
        case PREDICT_PARAM_SALINITY: return r->salinity;    // 盐度
        case PREDICT_PARAM_DO:       return r->do_value;    // 溶解氧DO
        default: return INVALID_VALUE;
    }
}

/*
 * 函数功能：把枚举类型的参数转换成人可读的中文名称
 * 用途：打印运行日志、输出预测结果时使用
 * 入参：param 水质参数枚举
 * 返回值：对应中文字符串，枚举不存在时返回"未知"
 */
const char *predict_param_name(PredictParam param) {
    switch (param) {
        case PREDICT_PARAM_TEMP:     return "水温";
        case PREDICT_PARAM_AIR_TEMP: return "气温";
        case PREDICT_PARAM_PH:       return "pH";
        case PREDICT_PARAM_SALINITY: return "盐度";
        case PREDICT_PARAM_DO:       return "溶解氧(DO)";
        default: return "未知";
    }
}
//建立线性回归模型（4.1）
LinearRegression linear_regression(const WaterQualityDataset *ds, 
                                   PredictParam x_param, 
                                   PredictParam y_param) {
    LinearRegression result = {0};
    
    double sum_x = 0.0, sum_y = 0.0;
    double sum_xy = 0.0, sum_x2 = 0.0;
    int n = 0;
    
    for (int i = 0; i < ds->count; i++) {
        const WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;
        
        double x = get_param_value(r, x_param);
        double y = get_param_value(r, y_param);
        
        if (x == INVALID_VALUE || y == INVALID_VALUE) continue;
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        n++;
    }
    
    if (n < 2) {
        return result;
    }
    // 计算斜率
    result.sample_count = n;
    result.x_mean = sum_x / n;
    result.y_mean = sum_y / n;
    
    double denominator = sum_x2 - (sum_x * sum_x) / n;
    if (fabs(denominator) < 1e-10) {
        return result;
    }
    // 计算截距
    result.slope = (sum_xy - (sum_x * sum_y) / n) / denominator;
    result.intercept = result.y_mean - result.slope * result.x_mean;
    
    return result;
}

//预测函数（4.1）
double predict(LinearRegression *model, double x) {
    return model->slope * x + model->intercept;
}

// 决定系数R²评估（4.1.1）
double calculate_r_squared(const WaterQualityDataset *ds, 
                            PredictParam x_param, 
                            PredictParam y_param,
                            LinearRegression *model) {
    double ss_tot = 0.0;
    double ss_res = 0.0;
    int n = 0;
    
    for (int i = 0; i < ds->count; i++) {
        const WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;
        
        double x = get_param_value(r, x_param);
        double y = get_param_value(r, y_param);
        
        if (x == INVALID_VALUE || y == INVALID_VALUE) continue;
        
        double y_pred = predict(model, x);
        ss_tot += (y - model->y_mean) * (y - model->y_mean);
        ss_res += (y - y_pred) * (y - y_pred);
        n++;
    }
    
    if (n < 2 || ss_tot < 1e-10) {
        return 0.0;
    }
    
    return 1.0 - (ss_res / ss_tot);
}

//留出法评估RMSE（4.1.2）
//计算RMSE
double calculate_rmse(const WaterQualityDataset *ds, 
                       PredictParam x_param, 
                       PredictParam y_param,
                       LinearRegression *model) {
    double sum_sq_error = 0.0;
    int n = 0;
    
    for (int i = 0; i < ds->count; i++) {
        const WaterQualityRecord *r = &ds->records[i];
        if (!r->valid) continue;
        
        double x = get_param_value(r, x_param);
        double y = get_param_value(r, y_param);
        
        if (x == INVALID_VALUE || y == INVALID_VALUE) continue;
        
        //计算RMSE
        double y_pred = predict(model, x);
        sum_sq_error += (y - y_pred) * (y - y_pred);
        n++;
    }
    
    if (n == 0) {
        return 0.0;
    }
    
    return sqrt(sum_sq_error / n);
}

//留出法评估RMSE（4.1.2）
//划分数据集
void train_test_split(const WaterQualityDataset *ds,
                      WaterQualityDataset **train_set,
                      WaterQualityDataset **test_set,
                      double train_ratio) {
    *train_set = dataset_create();
    *test_set = dataset_create();
    
    if (!*train_set || !*test_set) {
        return;
    }
    
    int train_count = (int)(ds->count * train_ratio);
    
    for (int i = 0; i < ds->count; i++) {
        if (i < train_count) {
            if ((*train_set)->count >= (*train_set)->capacity) {
                int new_cap = (*train_set)->capacity * 2;
                WaterQualityRecord *tmp = (WaterQualityRecord *)
                    realloc((*train_set)->records, new_cap * sizeof(WaterQualityRecord));
                if (!tmp) return;
                (*train_set)->records = tmp;
                (*train_set)->capacity = new_cap;
            }
            (*train_set)->records[(*train_set)->count] = ds->records[i];
            (*train_set)->count++;
            if (ds->records[i].valid) (*train_set)->valid_count++;
        } else {
            if ((*test_set)->count >= (*test_set)->capacity) {
                int new_cap = (*test_set)->capacity * 2;
                WaterQualityRecord *tmp = (WaterQualityRecord *)
                    realloc((*test_set)->records, new_cap * sizeof(WaterQualityRecord));
                if (!tmp) return;
                (*test_set)->records = tmp;
                (*test_set)->capacity = new_cap;
            }
            (*test_set)->records[(*test_set)->count] = ds->records[i];
            (*test_set)->count++;
            if (ds->records[i].valid) (*test_set)->valid_count++;
        }
    }
    
    (*train_set)->total_read = (*train_set)->count;
    (*test_set)->total_read = (*test_set)->count;
}

static void show_regression_results(LinearRegression *model, 
                                    const char *x_name, 
                                    const char *y_name) {
    printf("\n===== 线性回归模型结果 =====\n");
    printf("自变量: %s\n", x_name);
    printf("因变量: %s\n", y_name);
    printf("样本数量: %d\n", model->sample_count);
    printf("\n回归方程: y = %.6f * x + %.6f\n", model->slope, model->intercept);
    printf("R²决定系数: %.6f\n", model->r_squared);
    printf("RMSE均方根误差: %.6f\n", model->rmse);
    printf("\n方程解读:\n");
    printf("  当 %s 每增加1个单位，%s 平均%s %.6f 个单位\n", 
           x_name, y_name, 
           model->slope >= 0 ? "增加" : "减少", 
           fabs(model->slope));
    printf("  当 %s = 0 时，%s 的预测值为 %.6f\n", x_name, y_name, model->intercept);
}

static void evaluate_with_train_test(const WaterQualityDataset *ds,
                                     PredictParam x_param,
                                     PredictParam y_param) {
    const char *x_name = predict_param_name(x_param);
    const char *y_name = predict_param_name(y_param);
    
    WaterQualityDataset *train_set = NULL;
    WaterQualityDataset *test_set = NULL;
    train_test_split(ds, &train_set, &test_set, 0.8);
    
    if (!train_set || !test_set) {
        printf("数据集划分失败\n");
        return;
    }
    
    printf("\n===== 留出法评估 (训练集80%%, 测试集20%%) =====\n");
    printf("训练集样本数: %d\n", train_set->count);
    printf("测试集样本数: %d\n", test_set->count);
    
    LinearRegression model = linear_regression(train_set, x_param, y_param);
    
    model.r_squared = calculate_r_squared(train_set, x_param, y_param, &model);
    
    double train_rmse = calculate_rmse(train_set, x_param, y_param, &model);
    double test_rmse = calculate_rmse(test_set, x_param, y_param, &model);
    
    printf("\n训练集上的评估:\n");
    printf("  R²决定系数: %.6f\n", model.r_squared);
    printf("  RMSE均方根误差: %.6f\n", train_rmse);
    
    printf("\n测试集上的评估:\n");
    printf("  RMSE均方根误差: %.6f\n", test_rmse);
    
    printf("\n分析讨论:\n");
    if (fabs(train_rmse - test_rmse) < 0.1) {
        printf("  ✓ 模型泛化能力较好，训练集和测试集的RMSE接近\n");
    } else if (test_rmse > train_rmse) {
        printf("  ⚠ 模型可能存在过拟合，测试集RMSE明显高于训练集\n");
    } else {
        printf("  ? 测试集RMSE低于训练集，可能是数据分布差异导致\n");
    }
    
    dataset_free(train_set);
    dataset_free(test_set);
}

//多因子探索分析（4.1）
static void multi_factor_analysis(const WaterQualityDataset *ds) {
    printf("\n===== 多因子与溶解氧(DO)的关系分析 =====\n");
    printf("%-10s %-12s %-12s %-12s\n", "自变量", "斜率a", "截距b", "R²值");
    printf("-----------------------------------------------\n");
    
    PredictParam factors[] = {PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_TEMP, 
                              PREDICT_PARAM_PH, PREDICT_PARAM_SALINITY};
    const char *names[] = {"气温", "水温", "pH", "盐度"};
    double r_squared_values[4];
    LinearRegression models[4];
    
    for (int i = 0; i < 4; i++) {
        models[i] = linear_regression(ds, factors[i], PREDICT_PARAM_DO);
        models[i].r_squared = calculate_r_squared(ds, factors[i], PREDICT_PARAM_DO, &models[i]);
        r_squared_values[i] = models[i].r_squared;
        
        printf("%-10s %-12.6f %-12.6f %-12.6f\n", 
               names[i], 
               models[i].slope, 
               models[i].intercept, 
               models[i].r_squared);
    }
    
    int best_idx = 0;
    for (int i = 1; i < 4; i++) {
        if (fabs(r_squared_values[i]) > fabs(r_squared_values[best_idx])) {
            best_idx = i;
        }
    }
    
    printf("\n分析结论:\n");
    printf("与溶解氧(DO)相关性最强的环境因子: %s (R² = %.6f)\n", 
           names[best_idx], r_squared_values[best_idx]);
    
    printf("\n各因子影响分析:\n");
    for (int i = 0; i < 4; i++) {
        printf("  %s: ", names[i]);
        if (models[i].slope > 0) {
            printf("正相关，%s升高时DO倾向于升高\n", names[i]);
        } else if (models[i].slope < 0) {
            printf("负相关，%s升高时DO倾向于降低\n", names[i]);
        } else {
            printf("几乎无线性相关\n");
        }
    }
    
    printf("\n理论分析:\n");
    printf("  - 水温与DO: 理论上应为负相关（温度升高降低氧气溶解度）\n");
    printf("  - 气温与DO: 气温间接影响水温，进而影响DO\n");
    printf("  - pH与DO: 光合作用产氧会影响pH，可能呈正相关\n");
    printf("  - 盐度与DO: 盐度对氧气溶解度有影响，但影响较小\n");
}

void predict_menu(WaterQualityDataset *ds) {
    while (1) {
        printf("\n===== 模块四: 预测模型 =====\n");
        printf(" 1.气温与溶解氧线性回归预测\n");
        printf(" 2.模型评估 - 决定系数R²\n");
        printf(" 3.模型评估 - 留出法(RMSE)\n");
        printf(" 4.多因子探索分析\n");
        printf(" 5.单因素预测功能\n");
        printf(" 6.讨论分析\n");
        printf(" 0. 返回主菜单\n");
        
        int choice = get_int_input("请选择: ", 0, 6);
        
        switch (choice) {
            case 1: {
                LinearRegression model = linear_regression(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO);
                model.r_squared = calculate_r_squared(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO, &model);
                model.rmse = calculate_rmse(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO, &model);
                show_regression_results(&model, "气温", "溶解氧(DO)");
                pause_and_continue();
                break;
            }
            case 2: {
                LinearRegression model = linear_regression(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO);
                model.r_squared = calculate_r_squared(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO, &model);
                
                printf("\n===== 决定系数R²评估 =====\n");
                printf("回归方程: DO = %.6f * 气温 + %.6f\n", model.slope, model.intercept);
                printf("R²值: %.6f\n", model.r_squared);
                printf("\nR²值解读:\n");
                if (model.r_squared >= 0.8) printf("  |R²| >= 0.8: 极强相关\n");
                else if (model.r_squared >= 0.6) printf("  0.6 <= |R²| < 0.8: 强相关\n");
                else if (model.r_squared >= 0.4) printf("  0.4 <= |R²| < 0.6: 中等相关\n");
                else if (model.r_squared >= 0.2) printf("  0.2 <= |R²| < 0.4: 弱相关\n");
                else printf("  |R²| < 0.2: 极弱/无相关\n");
                pause_and_continue();
                break;
            }
            case 3: {
                evaluate_with_train_test(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO);
                pause_and_continue();
                break;
            }
            case 4: {
                multi_factor_analysis(ds);
                pause_and_continue();
                break;
            }
            case 5: {
                LinearRegression model = linear_regression(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO);
                
                printf("\n===== 单因素预测功能 =====\n");
                printf("当前模型: DO = %.6f * 气温 + %.6f\n", model.slope, model.intercept);
                printf("模型R²: %.6f\n", calculate_r_squared(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO, &model));
                
                double air_temp = get_double_input("请输入预测的气温(℃): ", -20, 60);
                double predicted_do = predict(&model, air_temp);
                
                printf("\n预测结果:\n");
                printf("当气温 = %.2f ℃ 时\n", air_temp);
                printf("预测溶解氧(DO) = %.6f mg/l\n", predicted_do);
                
                if (predicted_do < 3.0) {
                    printf("\n⚠ 预警: DO预测值低于3.0 mg/l，建议开启增氧机！\n");
                } else if (predicted_do < 4.0) {
                    printf("\n⚠ 警告: DO预测值处于亚缺氧状态\n");
                } else {
                    printf("\n✓ DO预测值处于正常范围\n");
                }
                pause_and_continue();
                break;
            }
            //讨论分析（4.1）
            case 6: {
                printf("\n===== 单因素线性回归预测模型讨论分析 =====\n");
                printf("\n一、预测准确度分析:\n");
                printf("  1. 单因素线性回归的局限性:\n");
                printf("     - 只考虑单一自变量，忽略了其他影响因素的交互作用\n");
                printf("     - 假设变量之间是线性关系，但实际可能是非线性的\n");
                printf("     - 无法捕捉时间序列的动态变化趋势\n");
                
                LinearRegression model = linear_regression(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO);
                model.r_squared = calculate_r_squared(ds, PREDICT_PARAM_AIR_TEMP, PREDICT_PARAM_DO, &model);
                
                printf("\n  2. 当前模型准确度评估:\n");
                printf("     - R²值: %.6f\n", model.r_squared);
                if (model.r_squared < 0.3) {
                    printf("     - 评估: 预测准确度较低\n");
                    printf("     - 原因: 溶解氧受多种因素影响，单一气温不足以解释其变化\n");
                } else {
                    printf("     - 评估: 预测准确度中等\n");
                }
                
                printf("\n二、误差来源分析:\n");
                printf("  1. 数据质量因素:\n");
                printf("     - 传感器测量误差\n");
                printf("     - 数据中的异常值和噪声\n");
                printf("     - 采样频率可能不足以捕捉快速变化\n");
                
                printf("\n  2. 模型本身的局限性:\n");
                printf("     - 线性假设可能不成立\n");
                printf("     - 忽略了其他重要因素（水温、pH、盐度等）\n");
                printf("     - 无法处理时变关系和滞后效应\n");
                
                printf("\n三、改进建议:\n");
                printf("  1. 多因素回归模型:\n");
                printf("     - 同时考虑水温、气温、pH、盐度等多个因素\n");
                printf("     - 可以提高模型的解释能力\n");
                
                printf("\n  2. 时间序列模型:\n");
                printf("     - ARIMA模型: 适合时间序列预测\n");
                printf("     - LSTM神经网络: 可以捕捉复杂的非线性关系\n");
                printf("     - 移动平均模型: 平滑短期波动\n");
                
                printf("\n  3. 机器学习模型:\n");
                printf("     - 多元线性回归\n");
                printf("     - 随机森林回归\n");
                printf("     - 支持向量机回归\n");
                
                printf("\n四、实际应用建议:\n");
                printf("  1. 当前单因素模型可作为参考，但不应单独依赖\n");
                printf("  2. 建议结合其他监测数据和专业知识综合判断\n");
                printf("  3. 定期更新模型参数，适应环境变化\n");
                pause_and_continue();
                break;
            }
            case 0:
                return;
        }
    }
}
