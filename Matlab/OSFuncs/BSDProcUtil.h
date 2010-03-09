/*
 *  BSDProcUtil.h
 *  StimulateOpenGL_II
 *
 *  Created by calin on 3/8/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef BSDProcUtil_H
#define BSDProcUtil_H

#include <sys/sysctl.h>

#ifdef __cplusplus
extern "C" {
#endif

	
	int GetBSDProcessList(struct kinfo_proc **procList, size_t *procCount);
	int IsBSDProcessRunning(const char *exe_name);
	
#ifdef __cplusplus
}
#endif


#endif
