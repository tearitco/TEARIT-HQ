/* job.h */
#ifndef DF_JOB_H
#define DF_JOB_H

#include "fort.h"

void job_sync_from_designations(Fort *f);
void job_try_assign(Fort *f);
void job_complete(Fort *f, int jidx);
int  job_add(Fort *f, int kind, int x, int y, int target_item, int work_need);
void job_free(Fort *f, int jidx);
const char *job_kind_name(int k);

#endif
