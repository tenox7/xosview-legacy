#ifndef _KStatList_H_
#define _KStatList_H_

#include <kstat.h>

/*  Helper to keep track of kstats.  */

typedef enum {          /*  module:instance:name (class)  */
  KSL_CPU_STAT,         /*  *:*:cpu_stat*  */
  KSL_CPU_INFO,         /*  *:*:cpu_info*  */
  KSL_CPU_SYS,          /*  cpu:*:sys  */
  KSL_DISKS,            /*  *:*:*         (disk)  */
  KSL_NETS,             /*  {link,lo}:*:* (net)  */
  KSL_COUNT
} KStatModule;

typedef struct {
  kid_t chain;
  KStatModule m;
  kstat_t **stats;
  unsigned int count, cap;
} KStatList;

/*  One list per module, created on first use.  */
KStatList *kstatlist_get(kstat_ctl_t *kcp, KStatModule m);
void kstatlist_update(KStatList *l, kstat_ctl_t *kcp);
kstat_t *kstatlist_at(KStatList *l, unsigned int i);
unsigned int kstatlist_count(KStatList *l);

/*  Read the correct value from a "named" type kstat.  */
double kstat_to_double(kstat_named_t *k);
unsigned long long kstat_to_ui64(kstat_named_t *k);

#endif
