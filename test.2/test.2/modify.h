#ifndef MODIFY_H
#define MODIFY_H

#include "dataset.h"

/* ── 修改记录 ────────────────────────────────────────────── */
void modify_record(WaterQualityDataset *ds);

/* ── 删除记录 ────────────────────────────────────────────── */
void delete_record_single(WaterQualityDataset *ds);
void delete_record_batch(WaterQualityDataset *ds);

/* ── 修改菜单 ────────────────────────────────────────────── */
void modify_menu(WaterQualityDataset *ds);

#endif
