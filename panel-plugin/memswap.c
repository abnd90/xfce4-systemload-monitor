/*
 * Copyright (c) 2003 Riccardo Persichetti <ricpersi@libero.it>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "memswap.h"


#if defined(__linux__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define MEMINFOBUFSIZE (2 * 1024)
static char MemInfoBuf[MEMINFOBUFSIZE];

static unsigned long MTotal = 0;
static unsigned long MFree = 0;
static unsigned long MCached = 0;
static unsigned long MUsed = 0;
static unsigned long STotal = 0;
static unsigned long SFree = 0;
static unsigned long SUsed = 0;

gint read_memswap(gulong *mem, gulong *swap, gulong *MT, gulong *MU, gulong *ST, gulong *SU)
{
    int fd;
    size_t n;
    int o_MTotal, o_MFree, o_MCached, o_STotal, o_SFree;
    char *b_MTotal, *b_MFree, *b_MCached, *b_STotal, *b_SFree;

    if ((fd = open("/proc/meminfo", O_RDONLY)) < 0)
    {
        g_warning("Cannot open \'/proc/meminfo\'");
        return -1;
    }
    if ((n = read(fd, MemInfoBuf, MEMINFOBUFSIZE - 1)) == MEMINFOBUFSIZE - 1)
    {
        g_warning("Internal buffer too small to read \'/proc/mem\'");
        close(fd);
        return -1;
    }
    close(fd);

    MemInfoBuf[n] = '\0';

    b_MTotal = strstr(MemInfoBuf, "MemTotal");
    if (b_MTotal)
        o_MTotal = sscanf(b_MTotal + strlen("MemTotal"), ": %lu", &MTotal);

    b_MFree = strstr(MemInfoBuf, "MemFree");
    if (b_MFree)
        o_MFree = sscanf(b_MFree + strlen("MemFree"), ": %lu", &MFree);

    b_MCached = strstr(MemInfoBuf, "Cached");
    if (b_MCached)
        o_MCached = sscanf(b_MCached + strlen("Cached"), ": %lu", &MCached);

    b_STotal = strstr(MemInfoBuf, "SwapTotal");
    if (b_STotal)
        o_STotal = sscanf(b_STotal + strlen("SwapTotal"), ": %lu", &STotal);

    b_SFree = strstr(MemInfoBuf, "SwapFree");
    if (b_SFree)
        o_SFree = sscanf(b_SFree + strlen("SwapFree"), ": %lu", &SFree);

    MFree += MCached;
    MUsed = MTotal - MFree;
    SUsed = STotal - SFree;
    *mem = MUsed * 100 / MTotal;

    if(STotal)
        *swap = SUsed * 100 / STotal;
    else
        *swap = 0;

    *MT = MTotal;
    *MU = MUsed;
    *ST = STotal;
    *SU = SUsed;

    return 0;
}

#elif defined(__FreeBSD__)
/*
 * FreeBSD defines MAX and MIN in sys/param.h, so undef the glib macros first
 */
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <vm/vm_param.h>
#include <sys/vmmeter.h>
#include <unistd.h>

static size_t MTotal = 0;
static size_t MFree = 0;
static size_t MUsed = 0;
static size_t STotal = 0;
static size_t SFree = 0;
static size_t SUsed = 0;

gint read_memswap(gulong *mem, gulong *swap, gulong *MT, gulong *MU, gulong *ST, gulong *SU)
{
    long pagesize;
    size_t len;

#define ARRLEN(X) (sizeof(X)/sizeof(X[0]))
    {
        static int mib[]={ CTL_HW, HW_PHYSMEM };
        len = sizeof(MTotal);
        sysctl(mib, ARRLEN(mib), &MTotal, &len, NULL, 0);
        MTotal >>= 10;
    }

#if 0 /* NOT YET */
    {
        struct uvmexp x;
        static int mib[] = { CTL_VM, VM_UVMEXP };
        len = sizeof(x);
        STotal = SUsed = SFree = -1;
        pagesize = 1;
        if (-1 < sysctl(mib, ARRLEN(mib), &x, &len, NULL, 0)) {
            pagesize = x.pagesize;
            STotal = (pagesize*x.swpages) >> 10;
            SUsed = (pagesize*x.swpginuse) >> 10;
            SFree = STotal - SUsed;
        }
    }
#else
    STotal = 0;
    SUsed = 0;
    SFree = 0;
#endif

    {
#ifdef VM_TOTAL
        static int mib[]={ CTL_VM, VM_TOTAL };
#else
        static int mib[]={ CTL_VM, VM_METER };
#endif
        struct vmtotal x;

        len = sizeof(x);
        MFree = MUsed = -1;
        if (sysctl(mib, ARRLEN(mib), &x, &len, NULL, 0) > -1) {
            MFree = (x.t_free * pagesize) >> 10;
            MUsed = (x.t_rm * pagesize) >> 10;
        }
    }

    *mem = MUsed * 100 / MTotal;
    if(STotal)
        *swap = SUsed * 100 / STotal;
    else
        *swap = 0;

    *MT = MTotal;
    *MU = MUsed;
    *ST = STotal;
    *SU = SUsed;

    return 0;
}

#elif defined(__NetBSD__)
/*
 * NetBSD defines MAX and MIN in sys/param.h, so undef the glib macros first
 */
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/vmmeter.h>
#include <unistd.h>
/* Everything post 1.5.x uses uvm/uvm_* includes */
#if __NetBSD_Version__ >= 105010000
#include <uvm/uvm_param.h>
#else
#include <vm/vm_param.h>
#endif

static size_t MTotal = 0;
static size_t MFree = 0;
static size_t MUsed = 0;
static size_t STotal = 0;
static size_t SFree = 0;
static size_t SUsed = 0;

gint read_memswap(gulong *mem, gulong *swap, gulong *MT, gulong *MU, gulong *ST, gulong *SU)
{
    long pagesize;
    size_t len;

#define ARRLEN(X) (sizeof(X)/sizeof(X[0]))
    {
        static int mib[]={ CTL_HW, HW_PHYSMEM };
        len = sizeof(MTotal);
        sysctl(mib, ARRLEN(mib), &MTotal, &len, NULL, 0);
        MTotal >>= 10;
    }

    {
        struct uvmexp x;
        static int mib[] = { CTL_VM, VM_UVMEXP };
        len = sizeof(x);
        STotal = SUsed = SFree = -1;
        pagesize = 1;
        if (-1 < sysctl(mib, ARRLEN(mib), &x, &len, NULL, 0)) {
            pagesize = x.pagesize;
            STotal = (pagesize*x.swpages) >> 10;
            SUsed = (pagesize*x.swpginuse) >> 10;
            SFree = STotal - SUsed;
        }
    }

    {
        static int mib[]={ CTL_VM, VM_METER };
        struct vmtotal x;

        len = sizeof(x);
        MFree = MUsed = -1;
        if (sysctl(mib, ARRLEN(mib), &x, &len, NULL, 0) > -1) {
            MFree = (x.t_free * pagesize) >> 10;
            MUsed = (x.t_rm * pagesize) >> 10;
        }
    }

    *mem = MUsed * 100 / MTotal;
    if(STotal)
        *swap = SUsed * 100 / STotal;
    else
        *swap = 0;

    *MT = MTotal;
    *MU = MUsed;
    *ST = STotal;
    *SU = SUsed;

    return 0;
}

#else
#error "Your plattform is not yet support"
#endif
