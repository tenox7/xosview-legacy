/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#include "memmeter.h"
#include "kstats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void checkres(Meter *m) {
	FieldMeter *fm = (FieldMeter *)m;

	fieldmeter_checkresources(m);

	fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "memKernelColor"));
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "memCacheColor"));
	fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "memUsedColor"));
	fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "memFreeColor"));
	m->priority = atoi(xwin_getresource(m->xw, "memPriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "memDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "memGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "memUsedFormat"));
}

static void getmeminfo(MemMeter *mm) {
	FieldMeter *fm = &mm->f;
	kstat_named_t *k;

	fm->fields[1] = 0;
	if (mm->ksp_zfs) {
		if (kstat_read(mm->kc, mm->ksp_zfs, NULL) == -1) {
			fprintf(stderr, "Can not read vmem::zfs_file_data_buf "
			        "kstat.\n");
			xwin_setdone(fm->m.xw, 1);
			return;
		}
		k = (kstat_named_t *)kstat_data_lookup(mm->ksp_zfs, "mem_inuse");
		if (k == NULL) {
			fprintf(stderr, "Can not read "
			        "vmem::zfs_file_data_buf:mem_inuse kstat.\n");
			xwin_setdone(fm->m.xw, 1);
			return;
		}
		fm->fields[1] = kstat_to_double(k) / mm->pageSize;
	}

	if (kstat_read(mm->kc, mm->ksp_sp, NULL) == -1) {
		fprintf(stderr, "Can not read unix:0:system_pages kstat.\n");
		xwin_setdone(fm->m.xw, 1);
		return;
	}
	k = (kstat_named_t *)kstat_data_lookup(mm->ksp_sp, "pp_kernel");
	if (k == NULL) {
		fprintf(stderr, "Can not read unix:0:system_pages:pp_kernel "
		        "kstat.\n");
		xwin_setdone(fm->m.xw, 1);
		return;
	}
	fm->fields[0] = kstat_to_double(k) - fm->fields[1];
	k = (kstat_named_t *)kstat_data_lookup(mm->ksp_sp, "freemem");
	if (k == NULL) {
		fprintf(stderr, "Can not read unix:0:system_pages:freemem "
		        "kstat.\n");
		xwin_setdone(fm->m.xw, 1);
		return;
	}
	fm->fields[3] = kstat_to_double(k);
	fm->fields[2] = fm->total
	                - (fm->fields[0] + fm->fields[1] + fm->fields[3]);

	XOSDEBUG("kernel: %llu kB zfs: %llu kB other: %llu kB free: %llu kB\n",
	         (unsigned long long)(fm->fields[0] * mm->pageSize / 1024),
	         (unsigned long long)(fm->fields[1] * mm->pageSize / 1024),
	         (unsigned long long)(fm->fields[2] * mm->pageSize / 1024),
	         (unsigned long long)(fm->fields[3] * mm->pageSize / 1024));

	fieldmeter_setused(fm,
	                   (fm->fields[0] + fm->fields[1] + fm->fields[2])
	                     * mm->pageSize,
	                   fm->total * mm->pageSize);
}

static void checkevent(Meter *m) {
	getmeminfo((MemMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *memmeter_new(XOSView *parent, kstat_ctl_t *kc) {
	MemMeter *mm = (MemMeter *)meter_alloc(sizeof *mm);

	fieldmeter_init(&mm->f, parent, 4, "MemMeter", "MEM",
	                "SYS/ZFS/OTHER/FREE", 0, 0, 0);
	mm->f.m.checkres = checkres;
	mm->f.m.checkevent = checkevent;

	mm->kc = kc;
	mm->pageSize = sysconf(_SC_PAGESIZE);
	mm->f.total = sysconf(_SC_PHYS_PAGES);

	mm->ksp_sp = kstat_lookup(kc, "unix", 0, "system_pages");
	mm->ksp_zfs = kstat_lookup(kc, "vmem", -1, "zfs_file_data_buf");
	if (mm->ksp_sp == NULL) {  /*  ZFS cache may be missing  */
		fprintf(stderr, "Can not find unix:0:system_pages kstat.\n");
		xwin_setdone((XWin *)parent, 1);
	}

	return &mm->f.m;
}
