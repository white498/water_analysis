#ifndef PREDICT_H
#define PREDICT_H

#include "common.h"

typedef struct {
    double slope;       /* 斜率 a */
    double intercept;   /* 截距 b */
    double r_squared;   /* 决定系数 R² */
    double rmse;        /* 均方根误差 */
    int sample_count;   /* 样本数量 */
    double x_mean;      /* 自变量均值 */
    double y_mean;      /* 因变量均值 */
} LinearRegression;

// 参数枚举
typedef enum {
    PREDICT_PARAM_TEMP, //水温
    PREDICT_PARAM_AIR_TEMP,  //气温
    PREDICT_PARAM_PH,  //pH
    PREDICT_PARAM_SALINITY, //盐度
    PREDICT_PARAM_DO  //溶解氧
} PredictParam;

// 训练线性回归模型，传进数据集和两个参数，算出斜率和截距
LinearRegression linear_regression(const WaterQualityDataset *ds, 
                                   PredictParam x_param, 
                                   PredictParam y_param);

// 用训练好的模型，输入x值预测y值
double predict(LinearRegression *model, double x);

// 计算决定系数R²
double calculate_r_squared(const WaterQualityDataset *ds, 
                            PredictParam x_param, 
                            PredictParam y_param,
                            LinearRegression *model);

// 计算均方根误差
double calculate_rmse(const WaterQualityDataset *ds, 
                       PredictParam x_param, 
                       PredictParam y_param,
                       LinearRegression *model);

//	将数据集按比例拆分成训练集和测试集
void train_test_split(const WaterQualityDataset *ds,
                      WaterQualityDataset **train_set,
                      WaterQualityDataset **test_set,
                      double train_ratio);

// 预测模块的子菜单界面
void predict_menu(WaterQualityDataset *ds);

//	根据枚举值返回参数名称字符串
const char *predict_param_name(PredictParam param);

#endif
