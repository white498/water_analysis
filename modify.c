#define _CRT_SECURE_NO_WARNINGS
#include "modify.h"
#include "backup.h"

/* 打印单条记录 */
static void print_one(const WaterQualityRecord *r) {
    printf("\n记录 ID: %d\n", r->record_id);
    printf("  1. 水温:          %.4f ℃\n", r->temp);
    printf("  2. 盐度:          %.4f PSU\n", r->salinity);
    printf("  3. pH:            %.6f\n", r->ph);
    printf("  4. 溶解氧:        %.6f mg/l\n", r->do_value);
    printf("  5. 降水量:        %.6f mm\n", r->precipitation);
    printf("  6. 气温:          %.4f ℃\n", r->air_temp);
}

/* 修改单个字段 */
static void modify_field(double *field, const char *name, int param_idx) {
    double lo, hi;
    param_range(param_idx, &lo, &hi);
    printf("当前 %s = %.6f (有效范围: %.2f ~ %.2f)\n", name, *field, lo, hi);
    double new_val = get_double_input("新值: ", lo, hi);
    *field = new_val;
    printf("%s 已更新为 %.6f\n", name, new_val);
}

/* 修改单条记录*/

void modify_record(WaterQualityDataset *ds) {
    if (ds->count == 0) {
        printf("\n数据集为空，无数据可修改。\n");
        return;
    }

    int rid = get_int_input("输入要修改的记录 ID: ", 1, ds->count * 2);
    WaterQualityRecord *r = dataset_get_record(ds, rid);
    if (!r) {
        printf("未找到 ID 为 %d 的有效记录。\n", rid);
        return;
    }

    print_one(r);

    if (!get_yes_no("确认修改此记录？")) return;

    /* 修改前自动备份 */
    printf("修改前自动备份...\n");
    backup_create_auto(ds);

    while (1) {
        printf("\n选择要修改的字段 (1-6, 0=完成): ");
        int field;
        if (scanf("%d", &field) != 1) { clear_input_buffer(); continue; }
        clear_input_buffer();

        switch (field) {
            case 1: modify_field(&r->temp,          "水温",    PARAM_TEMP);          break;
            case 2: modify_field(&r->salinity,      "盐度",    PARAM_SALINITY);      break;
            case 3: modify_field(&r->ph,            "pH",      PARAM_PH);            break;
            case 4: modify_field(&r->do_value,      "溶解氧",  PARAM_DO);            break;
            case 5: modify_field(&r->precipitation, "降水量",  PARAM_PRECIPITATION); break;
            case 6: modify_field(&r->air_temp,      "气温",    PARAM_AIR_TEMP);      break;
            case 0: goto done;
            default: printf("无效选择。\n"); break;
        }
    }
done:

    printf("\n修改后的记录:\n");
    print_one(r);

    if (get_yes_no("是否保存到 data.csv？")) {
        if (dataset_write_csv(ds, DATA_FILE))
            printf("数据已保存到 data.csv\n");
    }
}

/* 删除单条记录*/

void delete_record_single(WaterQualityDataset *ds) {
    if (ds->count == 0) {
        printf("\n数据集为空，无数据可删除。\n");
        return;
    }

    int rid = get_int_input("输入要删除的记录 ID: ", 1, ds->count * 2);
    WaterQualityRecord *r = dataset_get_record(ds, rid);
    if (!r) {
        printf("未找到 ID 为 %d 的有效记录。\n", rid);
        return;
    }

    print_one(r);

    if (!get_yes_no("确认删除此记录？")) return;

    /* 删除前自动备份 */
    printf("删除前自动备份...\n");
    backup_create_auto(ds);

    r->valid = 0;
    ds->valid_count--;

    /* 压缩数组：与末位交换后递减计数 */
    int idx = dataset_get_index_by_id(ds, rid);
    if (idx >= 0 && idx < ds->count - 1) {
        ds->records[idx] = ds->records[ds->count - 1];
    }
    ds->count--;

    printf("记录 ID %d 已删除。当前有效记录: %d 条。\n", rid, ds->valid_count);

    if (get_yes_no("是否保存到 data.csv？")) {
        if (dataset_write_csv(ds, DATA_FILE))
            printf("数据已保存到 data.csv\n");
    }
}

/* 按条件批量删除*/

void delete_record_batch(WaterQualityDataset *ds) {
    if (ds->count == 0) {
        printf("\n数据集为空。\n");
        return;
    }

    printf("\n── 批量删除 —— 按条件筛选 ──\n");
    printf("可选参数:\n");
    for (int i = 0; i < PARAM_COUNT; i++) {
        double lo, hi;
        param_range(i, &lo, &hi);
        printf("  %d - %s (范围 %.2f ~ %.2f)\n", i + 1, param_name(i), lo, hi);
    }

    int choice = get_int_input("选择参数 (1-6): ", 1, PARAM_COUNT);
    int param_idx = choice - 1;

    double lo, hi;
    param_range(param_idx, &lo, &hi);
    double min_v = get_double_input("最小值: ", lo, hi);
    double max_v = get_double_input("最大值: ", min_v, hi);

    /* 先统计匹配数 */
    int match_count = 0;
    for (int i = 0; i < ds->count; i++) {
        if (!ds->records[i].valid) continue;
        double v = param_value(&ds->records[i], param_idx);
        if (v != INVALID_VALUE && v >= min_v && v <= max_v) match_count++;
    }

    if (match_count == 0) {
        printf("没有符合条件的记录。\n");
        return;
    }

    printf("\n将删除 %d 条符合条件的记录 (条件: %s 在 %.4f ~ %.4f)。\n",
           match_count, param_name(param_idx), min_v, max_v);

    if (!get_yes_no("确认批量删除？此操作不可恢复！")) return;

    /* 删除前自动备份 */
    printf("删除前自动备份...\n");
    backup_create_auto(ds);

    int deleted = 0;
    int write_pos = 0;
    for (int i = 0; i < ds->count; i++) {
        if (!ds->records[i].valid) continue;
        double v = param_value(&ds->records[i], param_idx);
        if (v != INVALID_VALUE && v >= min_v && v <= max_v) {
            deleted++;
        } else {
            if (write_pos != i)
                ds->records[write_pos] = ds->records[i];
            write_pos++;
        }
    }
    ds->count = write_pos;
    ds->valid_count = write_pos;

    printf("已删除 %d 条记录。当前有效记录: %d 条。\n", deleted, ds->valid_count);

    if (get_yes_no("是否保存到 data.csv？")) {
        if (dataset_write_csv(ds, DATA_FILE))
            printf("数据已保存到 data.csv\n");
    }
}

/* 修改菜单 */

void modify_menu(WaterQualityDataset *ds) {
    while (1) {
        printf("\n数据修改与删除\n");
        printf("1. 修改单条记录\n");
        printf("2. 删除单条记录\n");
        printf("3. 批量删除 (按条件)\n");
        printf("0. 返回主菜单\n");

        int choice = get_int_input("请选择: ", 0, 3);
        switch (choice) {
            case 1: modify_record(ds);        break;
            case 2: delete_record_single(ds); break;
            case 3: delete_record_batch(ds);  break;
            case 0: return;
        }
    }
}
