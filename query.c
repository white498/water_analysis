#define _CRT_SECURE_NO_WARNINGS
#include "query.h"

/* ── 记录显示辅助函数 ──────────────────────────────────── */
static void print_header(void) {
    printf("\n%-8s %-10s %-10s %-12s %-12s %-12s %-10s\n",
           "ID", "水温(℃)", "盐度(PSU)", "pH", "溶解氧(mg/l)", "降水量(mm)", "气温(℃)");
    printf("──────────────────────────────────────────────────────────────────────────\n");
}

static void print_record(const WaterQualityRecord *r) {
    printf("%-8d %-10.4f %-10.4f %-12.6f %-12.6f %-12.6f %-10.4f\n",
           r->record_id, r->temp, r->salinity, r->ph, r->do_value,
           r->precipitation, r->air_temp);
}

/* ══════════════════════════════════════════════════════════════
   分页浏览
   ══════════════════════════════════════════════════════════════ */

void query_paginate(const WaterQualityDataset *ds) {
    if (ds->count == 0) {
        printf("\n数据集为空。\n");
        return;
    }

    int total_valid = ds->valid_count;
    int total_pages = (total_valid + PAGE_SIZE - 1) / PAGE_SIZE;
    int current_page = 1;

    while (1) {
        printf("\n分页浏览 (第 %d/%d 页, 共 %d 条) \n",
               current_page, total_pages, total_valid);

        if (total_pages > 1) {
            printf("P：上一页  N：下一页  J：跳转  Q：返回\n");
        }
        printf("\n");

        print_header();

        int start = (current_page - 1) * PAGE_SIZE;
        int shown = 0;
        int skipped = 0;
        for (int i = 0; i < ds->count && shown < PAGE_SIZE; i++) {
            if (!ds->records[i].valid) continue;
            if (skipped < start) { skipped++; continue; }
            print_record(&ds->records[i]);
            shown++;
        }

        if (total_pages <= 1) {
            printf("\n按Enter返回主菜单");
            clear_input_buffer();
            return;
        }

        printf("\n操作: ");
        char cmd[16];
        if (scanf("%15s", cmd) != 1) { clear_input_buffer(); continue; }
        clear_input_buffer();

        switch (cmd[0]) {
            case 'p': case 'P':
                if (current_page > 1) current_page--;
                else printf("已经是第一页。\n");
                break;
            case 'n': case 'N':
                if (current_page < total_pages) current_page++;
                else printf("已经是最后一页。\n");
                break;
            case 'j': case 'J':
                {
                    int page = get_int_input("跳转到页码: ", 1, total_pages);
                    current_page = page;
                }
                break;
            case 'q': case 'Q': return;
            default: printf("无效操作。\n"); break;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   条件筛选
   ══════════════════════════════════════════════════════════════ */

void query_filter(const WaterQualityDataset *ds) {
    if (ds->count == 0) {
        printf("\n数据集为空。\n");
        return;
    }

    printf("\n按参数范围筛选\n");
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

    printf("\n筛选条件: %s 在 %.4f ~ %.4f 之间\n\n", param_name(param_idx), min_v, max_v);
    print_header();

    int count = 0;
    for (int i = 0; i < ds->count; i++) {
        if (!ds->records[i].valid) continue;
        double v = param_value(&ds->records[i], param_idx);
        if (v == INVALID_VALUE) continue;
        if (v >= min_v && v <= max_v) {
            print_record(&ds->records[i]);
            count++;
            if (count % PAGE_SIZE == 0 && count > 0) {
                printf("\n── 已显示 %d 条，按 Enter 继续，输入 q 停止 ──\n", count);
                char buf[16];
                if (fgets(buf, sizeof(buf), stdin) && (buf[0] == 'q' || buf[0] == 'Q'))
                    break;
            }
        }
    }
    printf("\n共找到 %d 条符合条件的记录。\n", count);
    pause_and_continue();
}

/* ══════════════════════════════════════════════════════════════
   排序
   ══════════════════════════════════════════════════════════════ */

typedef struct {
    const WaterQualityRecord *rec;
    double                    sort_key;
} SortEntry;

static int cmp_asc(const void *a, const void *b) {
    double d = ((const SortEntry *)a)->sort_key - ((const SortEntry *)b)->sort_key;
    return (d > 0) - (d < 0);
}

static int cmp_desc(const void *a, const void *b) {
    double d = ((const SortEntry *)b)->sort_key - ((const SortEntry *)a)->sort_key;
    return (d > 0) - (d < 0);
}

void query_sort(WaterQualityDataset *ds) {
    if (ds->count == 0) {
        printf("\n数据集为空。\n");
        return;
    }

    printf("\n按参数排序\n");
    printf("可选参数:\n");
    for (int i = 0; i < PARAM_COUNT; i++)
        printf("  %d - %s\n", i + 1, param_name(i));

    int choice = get_int_input("选择参数 (1-6): ", 1, PARAM_COUNT);
    int param_idx = choice - 1;

    printf("排序方向: 1 - 升序, 2 - 降序\n");
    int dir = get_int_input("选择 (1/2): ", 1, 2);
    int ascending = (dir == 1);

    /* 统计有效记录数 */
    int n = 0;
    for (int i = 0; i < ds->count; i++)
        if (ds->records[i].valid) n++;

    SortEntry *entries = (SortEntry *)malloc(n * sizeof(SortEntry));
    if (!entries) {
        printf("[错误] 内存不足。\n");
        return;
    }

    int idx = 0;
    for (int i = 0; i < ds->count; i++) {
        if (!ds->records[i].valid) continue;
        entries[idx].rec      = &ds->records[i];
        entries[idx].sort_key = param_value(&ds->records[i], param_idx);
        idx++;
    }

    qsort(entries, n, sizeof(SortEntry), ascending ? cmp_asc : cmp_desc);

    printf("\n排序结果 (前100条):\n");
    print_header();
    int limit = n < 100 ? n : 100;
    for (int i = 0; i < limit; i++)
        print_record(entries[i].rec);

    printf("\n共 %d 条记录 (显示前 %d 条)。\n", n, limit);
    free(entries);
    pause_and_continue();
}

/* ══════════════════════════════════════════════════════════════
   查询菜单
   ══════════════════════════════════════════════════════════════ */

void query_menu(WaterQualityDataset *ds) {
    while (1) {
        printf("\n数据查询\n");
        printf("1. 分页浏览\n");
        printf("2. 按条件筛选\n");
        printf("3. 按参数排序\n");
        printf("4. 数据概览\n");
        printf("0. 返回主菜单\n");

        int choice = get_int_input("请选择: ", 0, 4);
        switch (choice) {
            case 1: query_paginate(ds);       break;
            case 2: query_filter(ds);         break;
            case 3: query_sort(ds);           break;
            case 4: dataset_print_overview(ds); pause_and_continue(); break;
            case 0: return;
        }
    }
}
