#ifndef BACKUP_H
#define BACKUP_H

#include "dataset.h"

/* ── 备份 ────────────────────────────────────────────────── */
int  backup_create_auto(WaterQualityDataset *ds);
int  backup_create_manual(WaterQualityDataset *ds);

/* ── 恢复 ────────────────────────────────────────────────── */
WaterQualityDataset *backup_restore(void);

/* ── 菜单 ────────────────────────────────────────────────── */
void backup_menu(WaterQualityDataset *ds);

#endif
