#define _CRT_SECURE_NO_WARNINGS
#include "backup.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

/* 确保备份目录存在 */
static int ensure_backup_dir(void) {
    if (!file_exists(BACKUP_DIR)) {
        if (make_directory(BACKUP_DIR) != 0) {
            printf("[错误] 无法创建备份目录: %s\n", BACKUP_DIR);
            return 0;
        }
    }
    return 1;
}

/* ── 自动备份（带时间戳） ───────────────────────────────── */

int backup_create_auto(WaterQualityDataset *ds) {
    if (!ensure_backup_dir()) return 0;

    char ts[32];
    get_timestamp_str(ts, sizeof(ts));

    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s\\backup_%s.csv", BACKUP_DIR, ts);

    if (!dataset_write_csv(ds, path)) return 0;
    printf("备份成功，文件已保存至 %s\n", path);
    return 1;
}

/* ── 手动备份 ───────────────────────────────────────────── */

int backup_create_manual(WaterQualityDataset *ds) {
    if (!ensure_backup_dir()) return 0;

    char ts[32];
    get_timestamp_str(ts, sizeof(ts));

    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s\\backup_%s.csv", BACKUP_DIR, ts);

    if (!dataset_write_csv(ds, path)) return 0;
    printf("备份成功，文件已保存至 %s\n", path);
    return 1;
}

/* ── 列出备份文件 ───────────────────────────────────────── */

/*
 * 返回以 NULL 结尾的文件名数组，存储在备份目录中。
 * 调用者需释放每个字符串和数组本身。
 */
static char **list_backup_files(int *count) {
    *count = 0;
    if (!file_exists(BACKUP_DIR)) return NULL;

    /* 第一遍：统计 CSV 文件数 */
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE hFind;
    char pattern[MAX_PATH_LEN];
    snprintf(pattern, sizeof(pattern), "%s\\backup_*.csv", BACKUP_DIR);
    hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return NULL;

    int n = 0;
    do { n++; } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    char **files = (char **)calloc(n + 1, sizeof(char *));
    if (!files) return NULL;

    hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) { free(files); return NULL; }
    int i = 0;
    do {
        files[i] = _strdup(fd.cFileName);
        i++;
    } while (FindNextFileA(hFind, &fd));
    files[i] = NULL;
    FindClose(hFind);
    *count = n;
    return files;
#else
    DIR *dir = opendir(BACKUP_DIR);
    if (!dir) return NULL;

    struct dirent *entry;
    int n = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "backup_") && strstr(entry->d_name, ".csv"))
            n++;
    }
    rewinddir(dir);

    char **files = (char **)calloc(n + 1, sizeof(char *));
    if (!files) { closedir(dir); return NULL; }

    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "backup_") && strstr(entry->d_name, ".csv")) {
            files[i] = strdup(entry->d_name);
            i++;
        }
    }
    files[i] = NULL;
    closedir(dir);
    *count = n;
    return files;
#endif
}

static void free_backup_list(char **files) {
    if (!files) return;
    for (int i = 0; files[i]; i++) free(files[i]);
    free(files);
}

/* ── 恢复备份 ───────────────────────────────────────────── */

WaterQualityDataset *backup_restore(void) {
    if (!file_exists(BACKUP_DIR)) {
        printf("备份目录不存在，没有可用的备份文件。\n");
        return NULL;
    }

    int count = 0;
    char **files = list_backup_files(&count);
    if (!files || count == 0) {
        printf("没有找到备份文件。\n");
        free_backup_list(files);
        return NULL;
    }

    printf("\n可用的备份文件:\n");
    for (int i = 0; i < count; i++)
        printf("  %d. %s\n", i + 1, files[i]);

    int choice = get_int_input("选择要恢复的备份 (0=取消): ", 0, count);
    if (choice == 0) {
        free_backup_list(files);
        return NULL;
    }

    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s\\%s", BACKUP_DIR, files[choice - 1]);

    printf("正在从 %s 恢复数据...\n", path);

    WaterQualityDataset *ds = dataset_read_csv(path);
    if (!ds) {
        printf("[错误] 恢复失败：无法读取备份文件。\n");
        free_backup_list(files);
        return NULL;
    }

    /* 验证：检查数据是否合理 */
    if (ds->count == 0) {
        printf("[错误] 备份文件格式无效或为空。\n");
        dataset_free(ds);
        free_backup_list(files);
        return NULL;
    }

    printf("恢复成功！已加载 %d 条记录 (有效 %d 条)。\n", ds->count, ds->valid_count);
    free_backup_list(files);
    return ds;
}

/* ── 备份菜单 ───────────────────────────────────────────── */

void backup_menu(WaterQualityDataset *ds) {
    while (1) {
        printf("\n数据备份与恢复\n");
        printf("1. 创建备份\n");
        printf("2. 从备份恢复\n");
        printf("0. 返回主菜单\n");

        int choice = get_int_input("请选择: ", 0, 2);
        switch (choice) {
            case 1:
                backup_create_manual(ds);
                pause_and_continue();
                break;
            case 2:
                {
                    printf("\n恢复将覆盖当前内存中的数据！\n");
                    if (get_yes_no("确认要恢复数据？")) {
                        WaterQualityDataset *restored = backup_restore();
                        if (restored) {
                            free(ds->records);
                            ds->records    = restored->records;
                            ds->count      = restored->count;
                            ds->capacity   = restored->capacity;
                            ds->total_read = restored->total_read;
                            ds->valid_count = restored->valid_count;
                            strncpy(ds->source_file, restored->source_file, MAX_PATH_LEN - 1);
                            restored->records = NULL;
                            dataset_free(restored);

                            /* 恢复后写回 data.csv，确保重启后数据一致 */
                            dataset_write_csv(ds, DATA_FILE);
                            printf("已将恢复数据保存至 data.csv\n");
                        }
                    }
                }
                pause_and_continue();
                break;
            case 0: return;
        }
    }
}
