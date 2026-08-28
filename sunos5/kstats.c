#include "kstats.h"
#include "xosview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static KStatList lists[KSL_COUNT];
static int created[KSL_COUNT];

static void push(KStatList *l, kstat_t *ksp) {
  if (l->count == l->cap) {
    unsigned int cap = l->cap ? l->cap * 2 : 16;
    kstat_t **grown = (kstat_t **)realloc(l->stats, cap * sizeof(kstat_t *));

    if (grown == NULL) {
      fprintf(stderr, "Out of memory.\n");
      exit(1);
    }
    l->stats = grown;
    l->cap = cap;
  }
  l->stats[l->count++] = ksp;
}

static void getstats(KStatList *l, kstat_ctl_t *kcp) {
  kstat_t *ksp;

  for (ksp = kcp->kc_chain; ksp != NULL; ksp = ksp->ks_next) {
    if (l->m == KSL_CPU_STAT && strncmp(ksp->ks_name, "cpu_stat", 8) == 0)
      push(l, ksp);
    if (l->m == KSL_CPU_INFO && strncmp(ksp->ks_name, "cpu_info", 8) == 0)
      push(l, ksp);
    if (l->m == KSL_CPU_SYS && ksp->ks_type == KSTAT_TYPE_NAMED &&
        strncmp(ksp->ks_module, "cpu", 3) == 0 &&
        strncmp(ksp->ks_name, "sys", 3) == 0)
      push(l, ksp);
    if (l->m == KSL_DISKS && ksp->ks_type == KSTAT_TYPE_IO &&
        strncmp(ksp->ks_class, "disk", 4) == 0)
      push(l, ksp);
    if (l->m == KSL_NETS && ksp->ks_type == KSTAT_TYPE_NAMED &&
        strncmp(ksp->ks_class, "net", 3) == 0 &&
        (strncmp(ksp->ks_module, "link", 4) == 0 ||
         strncmp(ksp->ks_module, "lo", 2) == 0))
      push(l, ksp);
  }
}

KStatList *kstatlist_get(kstat_ctl_t *kcp, KStatModule m) {
  KStatList *l;

  if (m < 0 || m >= KSL_COUNT)
    return NULL;

  l = &lists[m];
  if (!created[m]) {
    created[m] = 1;
    l->chain = kcp->kc_chain_id;
    l->m = m;
    l->stats = NULL;
    l->count = l->cap = 0;
    getstats(l, kcp);
  }

  return l;
}

void kstatlist_update(KStatList *l, kstat_ctl_t *kcp) {
  if (kstat_chain_update(kcp) > 0 || l->chain != kcp->kc_chain_id) {
    XOSDEBUG("kstat chain id changed to %d\n", kcp->kc_chain_id);
    l->chain = kcp->kc_chain_id;
    l->count = 0;
    getstats(l, kcp);
  }
}

kstat_t *kstatlist_at(KStatList *l, unsigned int i) {
  return i < l->count ? l->stats[i] : NULL;
}

unsigned int kstatlist_count(KStatList *l) {
  return l->count;
}

static void unconvertible(kstat_named_t *k) {
  fprintf(stderr, "kstat data type %d can not be converted to number.\n",
          k->data_type);
  exit(1);
}

double kstat_to_double(kstat_named_t *k) {
  switch (k->data_type) {
  case KSTAT_DATA_INT32:
    return k->value.i32;
  case KSTAT_DATA_UINT32:
    return k->value.ui32;
#if defined(_INT64_TYPE)
  case KSTAT_DATA_INT64:
    return k->value.i64;
  case KSTAT_DATA_UINT64:
    return k->value.ui64;
#endif
  case KSTAT_DATA_FLOAT:
    return k->value.f;
  case KSTAT_DATA_DOUBLE:
    return k->value.d;
  default:
    unconvertible(k);
    return 0;
  }
}

unsigned long long kstat_to_ui64(kstat_named_t *k) {
  switch (k->data_type) {
  case KSTAT_DATA_INT32:
    return k->value.i32;
  case KSTAT_DATA_UINT32:
    return k->value.ui32;
#if defined(_INT64_TYPE)
  case KSTAT_DATA_INT64:
    return k->value.i64;
  case KSTAT_DATA_UINT64:
    return k->value.ui64;
#endif
  case KSTAT_DATA_FLOAT:
    return k->value.f;
  case KSTAT_DATA_DOUBLE:
    return k->value.d;
  default:
    unconvertible(k);
    return 0;
  }
}
