#ifndef QUERY_H
#define QUERY_H

#include "dataset.h"

/* ── 分页浏览 ────────────────────────────────────────────── */
void query_paginate(const WaterQualityDataset *ds);

/* ── 条件筛选 ────────────────────────────────────────────── */
void query_filter(const WaterQualityDataset *ds);

/* ── 排序 ────────────────────────────────────────────────── */
void query_sort(WaterQualityDataset *ds);

/* ── 查询菜单 ────────────────────────────────────────────── */
void query_menu(WaterQualityDataset *ds);

#endif
