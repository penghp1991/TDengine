/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE
#include "os.h"

#if defined(_TD_WINDOWS_64) || defined(_TD_WINDOWS_32)

/*
 * windows implementation
 */

#if (_WIN64)
#include <iphlpapi.h>
#include <mswsock.h>
#include <psapi.h>
#include <stdio.h>
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Mswsock.lib ")
#endif

#include <objbase.h>

#pragma warning(push)
#pragma warning(disable : 4091)
#include <DbgHelp.h>
#pragma warning(pop)

int32_t taosGetTotalMemory(int64_t *totalKB) {
  MEMORYSTATUSEX memsStat;
  memsStat.dwLength = sizeof(memsStat);
  if (!GlobalMemoryStatusEx(&memsStat)) {
    return -1;
  }

  *totalKB = memsStat.ullTotalPhys / 1024;
  return 0;
}

int32_t taosGetSysMemory(int64_t *usedKB) {
  MEMORYSTATUSEX memsStat;
  memsStat.dwLength = sizeof(memsStat);
  if (!GlobalMemoryStatusEx(&memsStat)) {
    return -1;
  }

  int64_t nMemFree = memsStat.ullAvailPhys / 1024;
  int64_t nMemTotal = memsStat.ullTotalPhys / 1024.0;

  *usedKB = nMemTotal - nMemFree;
  return 0;
}

int32_t taosGetProcMemory(int64_t *usedKB) {
  unsigned bytes_used = 0;

#if defined(_WIN64) && defined(_MSC_VER)
  PROCESS_MEMORY_COUNTERS pmc;
  HANDLE                  cur_proc = GetCurrentProcess();

  if (GetProcessMemoryInfo(cur_proc, &pmc, sizeof(pmc))) {
    bytes_used = (unsigned)(pmc.WorkingSetSize + pmc.PagefileUsage);
  }
#endif

  *usedKB = bytes_used / 1024;
  return 0;
}

int32_t taosGetCpuCores(float *numOfCores) {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  *numOfCores = info.dwNumberOfProcessors;
  return 0;
}

int32_t taosGetCpuUsage(float *sysCpuUsage, float *procCpuUsage) {
  *sysCpuUsage = 0;
  *procCpuUsage = 0;
  return 0;
}

int32_t taosGetDiskSize(char *dataDir, SDiskSize *diskSize) {
  unsigned _int64 i64FreeBytesToCaller;
  unsigned _int64 i64TotalBytes;
  unsigned _int64 i64FreeBytes;

  BOOL fResult = GetDiskFreeSpaceExA(dataDir, (PULARGE_INTEGER)&i64FreeBytesToCaller, (PULARGE_INTEGER)&i64TotalBytes,
                                     (PULARGE_INTEGER)&i64FreeBytes);
  if (fResult) {
    diskSize->tsize = (int64_t)(i64TotalBytes);
    diskSize->avail = (int64_t)(i64FreeBytesToCaller);
    diskSize->used = (int64_t)(i64TotalBytes - i64FreeBytes);
    return 0;
  } else {
    // printf("failed to get disk size, dataDir:%s errno:%s", tsDataDir, strerror(errno));
    terrno = TAOS_SYSTEM_ERROR(errno);
    return -1;
  }
}

int32_t taosGetCardInfo(int64_t *bytes, int64_t *rbytes, int64_t *tbytes) {
  if (bytes) *bytes = 0;
  if (rbytes) *rbytes = 0;
  if (tbytes) *tbytes = 0;
  return 0;
}

int32_t taosGetBandSpeed(float *bandSpeedKb) {
  *bandSpeedKb = 0;
  return 0;
}

int32_t taosReadProcIO(int64_t *readbyte, int64_t *writebyte) {
  IO_COUNTERS io_counter;
  if (GetProcessIoCounters(GetCurrentProcess(), &io_counter)) {
    if (readbyte) *readbyte = io_counter.ReadTransferCount;
    if (writebyte) *writebyte = io_counter.WriteTransferCount;
    return 0;
  }
  return -1;
}

int32_t taosGetProcIO(float *readKB, float *writeKB) {
  static int64_t lastReadbyte = -1;
  static int64_t lastWritebyte = -1;

  int64_t curReadbyte = 0;
  int64_t curWritebyte = 0;

  if (taosReadProcIO(&curReadbyte, &curWritebyte) != 0) {
    return -1;
  }

  if (lastReadbyte == -1 || lastWritebyte == -1) {
    lastReadbyte = curReadbyte;
    lastWritebyte = curWritebyte;
    return -1;
  }

  *readKB = (float)((double)(curReadbyte - lastReadbyte) / 1024);
  *writeKB = (float)((double)(curWritebyte - lastWritebyte) / 1024);
  if (*readKB < 0) *readKB = 0;
  if (*writeKB < 0) *writeKB = 0;

  lastReadbyte = curReadbyte;
  lastWritebyte = curWritebyte;

  return 0;
}

void taosGetSystemInfo() {
  taosGetCpuCores(&tsNumOfCores);
  taosGetTotalMemory(&tsTotalMemoryKB);

  float tmp1, tmp2;
  taosGetBandSpeed(&tmp1);
  taosGetCpuUsage(&tmp1, &tmp2);
  taosGetProcIO(&tmp1, &tmp2);
}

void taosKillSystem() {
  // printf("function taosKillSystem, exit!");
  exit(0);
}

int taosSystem(const char *cmd) {
  // printf("taosSystem not support");
  return -1;
}

LONG WINAPI FlCrashDump(PEXCEPTION_POINTERS ep) {
  typedef BOOL(WINAPI * FxMiniDumpWriteDump)(IN HANDLE hProcess, IN DWORD ProcessId, IN HANDLE hFile,
                                             IN MINIDUMP_TYPE                           DumpType,
                                             IN CONST PMINIDUMP_EXCEPTION_INFORMATION   ExceptionParam,
                                             IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                                             IN CONST PMINIDUMP_CALLBACK_INFORMATION    CallbackParam);

  HMODULE dll = LoadLibrary("dbghelp.dll");
  if (dll == NULL) return EXCEPTION_CONTINUE_SEARCH;
  FxMiniDumpWriteDump mdwd = (FxMiniDumpWriteDump)(GetProcAddress(dll, "MiniDumpWriteDump"));
  if (mdwd == NULL) {
    FreeLibrary(dll);
    return EXCEPTION_CONTINUE_SEARCH;
  }

  TCHAR path[MAX_PATH];
  DWORD len = GetModuleFileName(NULL, path, _countof(path));
  path[len - 3] = 'd';
  path[len - 2] = 'm';
  path[len - 1] = 'p';

  HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  MINIDUMP_EXCEPTION_INFORMATION mei;
  mei.ThreadId = GetCurrentThreadId();
  mei.ExceptionPointers = ep;
  mei.ClientPointers = FALSE;

  (*mdwd)(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithHandleData, &mei, NULL, NULL);

  CloseHandle(file);
  FreeLibrary(dll);

  return EXCEPTION_CONTINUE_SEARCH;
}

void taosSetCoreDump() { SetUnhandledExceptionFilter(&FlCrashDump); }

int32_t taosGetSystemUUID(char *uid, int32_t uidlen) {
  GUID guid;
  CoCreateGuid(&guid);

  sprintf(uid, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
          guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

  return 0;
}

char *taosGetCmdlineByPID(int pid) { return ""; }

#elif defined(_TD_DARWIN_64)

/*
 * darwin implementation
 */

#include <errno.h>
#include <libproc.h>

void taosKillSystem() {
  // printf("function taosKillSystem, exit!");
  exit(0);
}

int32_t taosGetCpuCores(float *numOfCores) {
  *numOfCores = sysconf(_SC_NPROCESSORS_ONLN);
  return 0;
}

void taosGetSystemInfo() {
  long physical_pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  tsTotalMemoryKB = physical_pages * page_size / 1024;
  tsPageSizeKB = page_size / 1024;
  tsNumOfCores = sysconf(_SC_NPROCESSORS_ONLN);
}

int32_t taosReadProcIO(int64_t *rchars, int64_t *wchars) {
  if (rchars) *rchars = 0;
  if (wchars) *wchars = 0;
  return 0;
}

int32_t taosGetProcIO(float *readKB, float *writeKB) {
  *readKB = 0;
  *writeKB = 0;
  return 0;
}

int32_t taosGetCardInfo(int64_t *bytes, int64_t *rbytes, int64_t *tbytes) {
  if (bytes) *bytes = 0;
  if (rbytes) *rbytes = 0;
  if (tbytes) *tbytes = 0;
  return 0;
}

int32_t taosGetBandSpeed(float *bandSpeedKb) {
  *bandSpeedKb = 0;
  return 0;
}

int32_t taosGetCpuUsage(float *sysCpuUsage, float *procCpuUsage) {
  *sysCpuUsage = 0;
  *procCpuUsage = 0;
  return 0;
}

int32_t taosGetProcMemory(int64_t *usedKB) {
  *usedKB = 0;
  return 0;
}

int32_t taosGetSysMemory(int64_t *usedKB) {
  *usedKB = 0;
  return 0;
}

int taosSystem(const char *cmd) {
  // printf("un support funtion");
  return -1;
}

void taosSetCoreDump() {}

int32_t taosGetDiskSize(char *dataDir, SDiskSize *diskSize) {
  struct statvfs info;
  if (statvfs(dataDir, &info)) {
    // printf("failed to get disk size, dataDir:%s errno:%s", tsDataDir, strerror(errno));
    terrno = TAOS_SYSTEM_ERROR(errno);
    return -1;
  } else {
    diskSize->tsize = info.f_blocks * info.f_frsize;
    diskSize->avail = info.f_bavail * info.f_frsize;
    diskSize->used = (info.f_blocks - info.f_bfree) * info.f_frsize;
    return 0;
  }
}

int32_t taosGetSystemUUID(char *uid, int32_t uidlen) {
  uuid_t uuid = {0};
  uuid_generate(uuid);
  // it's caller's responsibility to make enough space for `uid`, that's 36-char + 1-null
  uuid_unparse_lower(uuid, uid);
  return 0;
}

char *taosGetCmdlineByPID(int pid) {
  static char cmdline[1024];
  errno = 0;

  if (proc_pidpath(pid, cmdline, sizeof(cmdline)) <= 0) {
    fprintf(stderr, "PID is %d, %s", pid, strerror(errno));
    return strerror(errno);
  }

  return cmdline;
}

#else

/*
 * linux implementation
 */

#include <argp.h>
#include <linux/sysctl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

#define PROCESS_ITEM 12

typedef struct {
  uint64_t user;
  uint64_t nice;
  uint64_t system;
  uint64_t idle;
} SysCpuInfo;

typedef struct {
  uint64_t utime;   // user time
  uint64_t stime;   // kernel time
  uint64_t cutime;  // all user time
  uint64_t cstime;  // all dead time
} ProcCpuInfo;

static pid_t tsProcId;
static char  tsSysNetFile[] = "/proc/net/dev";
static char  tsSysCpuFile[] = "/proc/stat";
static char  tsProcCpuFile[25] = {0};
static char  tsProcMemFile[25] = {0};
static char  tsProcIOFile[25] = {0};

static void taosGetProcInfos() {
  tsPageSizeKB = sysconf(_SC_PAGESIZE) / 1024;
  tsOpenMax = sysconf(_SC_OPEN_MAX);
  tsStreamMax = sysconf(_SC_STREAM_MAX);
  tsProcId = (pid_t)syscall(SYS_gettid);

  snprintf(tsProcMemFile, sizeof(tsProcMemFile), "/proc/%d/status", tsProcId);
  snprintf(tsProcCpuFile, sizeof(tsProcCpuFile), "/proc/%d/stat", tsProcId);
  snprintf(tsProcIOFile, sizeof(tsProcIOFile), "/proc/%d/io", tsProcId);
}

int32_t taosGetTotalMemory(int64_t *totalKB) {
  *totalKB = (int64_t)(sysconf(_SC_PHYS_PAGES) * tsPageSizeKB);
  return 0;
}

int32_t taosGetSysMemory(int64_t *usedKB) {
  *usedKB = sysconf(_SC_AVPHYS_PAGES) * tsPageSizeKB;
  return 0;
}

int32_t taosGetProcMemory(int64_t *usedKB) {
  // FILE *fp = fopen(tsProcMemFile, "r");
  TdFilePtr pFile = taosOpenFile(tsProcMemFile, TD_FILE_READ | TD_FILE_STREAM);
  if (pFile == NULL) {
    // printf("open file:%s failed", tsProcMemFile);
    return -1;
  }

  ssize_t _bytes = 0;
  char   *line = NULL;
  while (!taosEOFFile(pFile)) {
    _bytes = taosGetLineFile(pFile, &line);
    if ((_bytes < 0) || (line == NULL)) {
      break;
    }
    if (strstr(line, "VmRSS:") != NULL) {
      break;
    }
  }

  if (line == NULL) {
    // printf("read file:%s failed", tsProcMemFile);
    taosCloseFile(&pFile);
    return -1;
  }

  char tmp[10];
  sscanf(line, "%s %" PRId64, tmp, usedKB);

  if (line != NULL) tfree(line);
  taosCloseFile(&pFile);
  return 0;
}

static int32_t taosGetSysCpuInfo(SysCpuInfo *cpuInfo) {
  // FILE *fp = fopen(tsSysCpuFile, "r");
  TdFilePtr pFile = taosOpenFile(tsSysCpuFile, TD_FILE_READ | TD_FILE_STREAM);
  if (pFile == NULL) {
    // printf("open file:%s failed", tsSysCpuFile);
    return -1;
  }

  char   *line = NULL;
  ssize_t _bytes = taosGetLineFile(pFile, &line);
  if ((_bytes < 0) || (line == NULL)) {
    // printf("read file:%s failed", tsSysCpuFile);
    taosCloseFile(&pFile);
    return -1;
  }

  char cpu[10] = {0};
  sscanf(line, "%s %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, cpu, &cpuInfo->user, &cpuInfo->nice, &cpuInfo->system,
         &cpuInfo->idle);

  if (line != NULL) tfree(line);
  taosCloseFile(&pFile);
  return 0;
}

static int32_t taosGetProcCpuInfo(ProcCpuInfo *cpuInfo) {
  // FILE *fp = fopen(tsProcCpuFile, "r");
  TdFilePtr pFile = taosOpenFile(tsProcCpuFile, TD_FILE_READ | TD_FILE_STREAM);
  if (pFile == NULL) {
    // printf("open file:%s failed", tsProcCpuFile);
    return -1;
  }

  char   *line = NULL;
  ssize_t _bytes = taosGetLineFile(pFile, &line);
  if ((_bytes < 0) || (line == NULL)) {
    // printf("read file:%s failed", tsProcCpuFile);
    taosCloseFile(&pFile);
    return -1;
  }

  for (int i = 0, blank = 0; line[i] != 0; ++i) {
    if (line[i] == ' ') blank++;
    if (blank == PROCESS_ITEM) {
      sscanf(line + i + 1, "%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, &cpuInfo->utime, &cpuInfo->stime,
             &cpuInfo->cutime, &cpuInfo->cstime);
      break;
    }
  }

  if (line != NULL) tfree(line);
  taosCloseFile(&pFile);
  return 0;
}

int32_t taosGetCpuCores(float *numOfCores) {
  *numOfCores = sysconf(_SC_NPROCESSORS_ONLN);
  return 0;
}

int32_t taosGetCpuUsage(float *cpu_system, float *cpu_engine) {
  static uint64_t lastSysUsed = 0;
  static uint64_t lastSysTotal = 0;
  static uint64_t lastProcTotal = 0;

  SysCpuInfo  sysCpu;
  ProcCpuInfo procCpu;
  if (taosGetSysCpuInfo(&sysCpu) != 0) {
    return -1;
  }
  if (taosGetProcCpuInfo(&procCpu) != 0) {
    return -1;
  }

  uint64_t curSysUsed = sysCpu.user + sysCpu.nice + sysCpu.system;
  uint64_t curSysTotal = curSysUsed + sysCpu.idle;
  uint64_t curProcTotal = procCpu.utime + procCpu.stime + procCpu.cutime + procCpu.cstime;

  if (lastSysUsed == 0 || lastSysTotal == 0 || lastProcTotal == 0) {
    lastSysUsed = curSysUsed > 1 ? curSysUsed : 1;
    lastSysTotal = curSysTotal > 1 ? curSysTotal : 1;
    lastProcTotal = curProcTotal > 1 ? curProcTotal : 1;
    return -1;
  }

  if (curSysTotal == lastSysTotal) {
    return -1;
  }

  *cpu_engine = (float)((double)(curSysUsed - lastSysUsed) / (double)(curSysTotal - lastSysTotal) * 100);
  *cpu_system = (float)((double)(curProcTotal - lastProcTotal) / (double)(curSysTotal - lastSysTotal) * 100);

  lastSysUsed = curSysUsed;
  lastSysTotal = curSysTotal;
  lastProcTotal = curProcTotal;

  return 0;
}

int32_t taosGetDiskSize(char *dataDir, SDiskSize *diskSize) {
  struct statvfs info;
  if (statvfs(dataDir, &info)) {
    return -1;
  } else {
    diskSize->total = info.f_blocks * info.f_frsize;
    diskSize->avail = info.f_bavail * info.f_frsize;
    diskSize->used = diskSize->total - diskSize->avail;
    return 0;
  }
}

int32_t taosGetCardInfo(int64_t *bytes, int64_t *rbytes, int64_t *tbytes) {
  if (bytes) *bytes = 0;
  // FILE *fp = fopen(tsSysNetFile, "r");
  TdFilePtr pFile = taosOpenFile(tsSysNetFile, TD_FILE_READ | TD_FILE_STREAM);
  if (pFile == NULL) {
    // printf("open file:%s failed", tsSysNetFile);
    return -1;
  }

  ssize_t _bytes = 0;
  char   *line = NULL;

  while (!taosEOFFile(pFile)) {
    int64_t o_rbytes = 0;
    int64_t rpackts = 0;
    int64_t o_tbytes = 0;
    int64_t tpackets = 0;
    int64_t nouse1 = 0;
    int64_t nouse2 = 0;
    int64_t nouse3 = 0;
    int64_t nouse4 = 0;
    int64_t nouse5 = 0;
    int64_t nouse6 = 0;
    char    nouse0[200] = {0};

    _bytes = taosGetLineFile(pFile, &line);
    if (_bytes < 0) {
      break;
    }

    line[_bytes - 1] = 0;

    if (strstr(line, "lo:") != NULL) {
      continue;
    }

    sscanf(line,
           "%s %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64
           " %" PRId64,
           nouse0, &o_rbytes, &rpackts, &nouse1, &nouse2, &nouse3, &nouse4, &nouse5, &nouse6, &o_tbytes, &tpackets);
    if (rbytes) *rbytes = o_rbytes;
    if (tbytes) *tbytes = o_tbytes;
    if (bytes)  *bytes += (o_rbytes + o_tbytes);
  }

  if (line != NULL) tfree(line);
  taosCloseFile(&pFile);

  return 0;
}

int32_t taosGetBandSpeed(float *bandSpeedKb) {
  static int64_t lastBytes = 0;
  static time_t  lastTime = 0;
  int64_t        curBytes = 0;
  time_t         curTime = time(NULL);

  if (taosGetCardInfo(&curBytes, NULL, NULL) != 0) {
    return -1;
  }

  if (lastTime == 0 || lastBytes == 0) {
    lastTime = curTime;
    lastBytes = curBytes;
    *bandSpeedKb = 0;
    return 0;
  }

  if (lastTime >= curTime || lastBytes > curBytes) {
    lastTime = curTime;
    lastBytes = curBytes;
    *bandSpeedKb = 0;
    return 0;
  }

  double totalBytes = (double)(curBytes - lastBytes) / 1024 * 8;  // Kb
  *bandSpeedKb = (float)(totalBytes / (double)(curTime - lastTime));

  // //printf("bandwidth lastBytes:%ld, lastTime:%ld, curBytes:%ld, curTime:%ld,
  // speed:%f", lastBytes, lastTime, curBytes, curTime, *bandSpeed);

  lastTime = curTime;
  lastBytes = curBytes;

  return 0;
}

int32_t taosReadProcIO(int64_t *rchars, int64_t *wchars) {
  // FILE *fp = fopen(tsProcIOFile, "r");
  TdFilePtr pFile = taosOpenFile(tsProcIOFile, TD_FILE_READ | TD_FILE_STREAM);
  if (pFile == NULL) {
    // printf("open file:%s failed", tsProcIOFile);
    return -1;
  }

  ssize_t _bytes = 0;
  char   *line = NULL;
  char    tmp[10];
  int     readIndex = 0;

  while (!taosEOFFile(pFile)) {
    _bytes = taosGetLineFile(pFile, &line);
    if ((_bytes < 0) || (line == NULL)) {
      break;
    }
    if (strstr(line, "rchar:") != NULL) {
      sscanf(line, "%s %" PRId64, tmp, rchars);
      readIndex++;
    } else if (strstr(line, "wchar:") != NULL) {
      sscanf(line, "%s %" PRId64, tmp, wchars);
      readIndex++;
    } else {
    }

    if (readIndex >= 2) break;
  }

  if (line != NULL) tfree(line);
  taosCloseFile(&pFile);

  if (readIndex < 2) {
    // printf("read file:%s failed", tsProcIOFile);
    return -1;
  }

  return 0;
}

int32_t taosGetProcIO(float *readKB, float *writeKB) {
  static int64_t lastReadbyte = -1;
  static int64_t lastWritebyte = -1;

  int64_t curReadbyte = 0;
  int64_t curWritebyte = 0;

  if (taosReadProcIO(&curReadbyte, &curWritebyte) != 0) {
    return -1;
  }

  if (lastReadbyte == -1 || lastWritebyte == -1) {
    lastReadbyte = curReadbyte;
    lastWritebyte = curWritebyte;
    return -1;
  }

  *readKB = (float)((double)(curReadbyte - lastReadbyte) / 1024);
  *writeKB = (float)((double)(curWritebyte - lastWritebyte) / 1024);
  if (*readKB < 0) *readKB = 0;
  if (*writeKB < 0) *writeKB = 0;

  lastReadbyte = curReadbyte;
  lastWritebyte = curWritebyte;

  return 0;
}

void taosGetSystemInfo() {
  taosGetProcInfos();
  taosGetCpuCores(&tsNumOfCores);
  taosGetTotalMemory(&tsTotalMemoryKB);

  float tmp1, tmp2;
  taosGetBandSpeed(&tmp1);
  taosGetCpuUsage(&tmp1, &tmp2);
  taosGetProcIO(&tmp1, &tmp2);
}

void taosKillSystem() {
  // SIGINT
  // printf("taosd will shut down soon");
  kill(tsProcId, 2);
}

int taosSystem(const char *cmd) {
  FILE *fp;
  int   res;
  char  buf[1024];
  if (cmd == NULL) {
    // printf("taosSystem cmd is NULL!");
    return -1;
  }

  if ((fp = popen(cmd, "r")) == NULL) {
    // printf("popen cmd:%s error: %s", cmd, strerror(errno));
    return -1;
  } else {
    while (fgets(buf, sizeof(buf), fp)) {
      // printf("popen result:%s", buf);
    }

    if ((res = pclose(fp)) == -1) {
      // printf("close popen file pointer fp error!");
    } else {
      // printf("popen res is :%d", res);
    }

    return res;
  }
}

void taosSetCoreDump(bool enable) {
  if (!enable) return;

  // 1. set ulimit -c unlimited
  struct rlimit rlim;
  struct rlimit rlim_new;
  if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
#ifndef _ALPINE
    // printf("the old unlimited para: rlim_cur=%" PRIu64 ", rlim_max=%" PRIu64, rlim.rlim_cur, rlim.rlim_max);
#else
    // printf("the old unlimited para: rlim_cur=%llu, rlim_max=%llu", rlim.rlim_cur, rlim.rlim_max);
#endif
    rlim_new.rlim_cur = RLIM_INFINITY;
    rlim_new.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rlim_new) != 0) {
      // printf("set unlimited fail, error: %s", strerror(errno));
      rlim_new.rlim_cur = rlim.rlim_max;
      rlim_new.rlim_max = rlim.rlim_max;
      (void)setrlimit(RLIMIT_CORE, &rlim_new);
    }
  }

  if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
#ifndef _ALPINE
    // printf("the new unlimited para: rlim_cur=%" PRIu64 ", rlim_max=%" PRIu64, rlim.rlim_cur, rlim.rlim_max);
#else
    // printf("the new unlimited para: rlim_cur=%llu, rlim_max=%llu", rlim.rlim_cur, rlim.rlim_max);
#endif
  }

#ifndef _TD_ARM_
  // 2. set the path for saving core file
  struct __sysctl_args args;

  int    old_usespid = 0;
  size_t old_len = 0;
  int    new_usespid = 1;
  size_t new_len = sizeof(new_usespid);

  int name[] = {CTL_KERN, KERN_CORE_USES_PID};

  memset(&args, 0, sizeof(struct __sysctl_args));
  args.name = name;
  args.nlen = sizeof(name) / sizeof(name[0]);
  args.oldval = &old_usespid;
  args.oldlenp = &old_len;
  args.newval = &new_usespid;
  args.newlen = new_len;

  old_len = sizeof(old_usespid);

  if (syscall(SYS__sysctl, &args) == -1) {
    // printf("_sysctl(kern_core_uses_pid) set fail: %s", strerror(errno));
  }

  // printf("The old core_uses_pid[%" PRIu64 "]: %d", old_len, old_usespid);

  old_usespid = 0;
  old_len = 0;
  memset(&args, 0, sizeof(struct __sysctl_args));
  args.name = name;
  args.nlen = sizeof(name) / sizeof(name[0]);
  args.oldval = &old_usespid;
  args.oldlenp = &old_len;

  old_len = sizeof(old_usespid);

  if (syscall(SYS__sysctl, &args) == -1) {
    // printf("_sysctl(kern_core_uses_pid) get fail: %s", strerror(errno));
  }

  // printf("The new core_uses_pid[%" PRIu64 "]: %d", old_len, old_usespid);
#endif
}

int32_t taosGetSystemUUID(char *uid, int32_t uidlen) {
  int len = 0;

  // fd = open("/proc/sys/kernel/random/uuid", 0);
  TdFilePtr pFile = taosOpenFile("/proc/sys/kernel/random/uuid", TD_FILE_READ);
  if (pFile == NULL) {
    return -1;
  } else {
    len = taosReadFile(pFile, uid, uidlen);
    taosCloseFile(&pFile);
  }

  if (len >= 36) {
    uid[36] = 0;
    return 0;
  }

  return 0;
}

char *taosGetCmdlineByPID(int pid) {
  static char cmdline[1024];
  sprintf(cmdline, "/proc/%d/cmdline", pid);

  // int fd = open(cmdline, O_RDONLY);
  TdFilePtr pFile = taosOpenFile(cmdline, TD_FILE_READ);
  if (pFile != NULL) {
    int n = taosReadFile(pFile, cmdline, sizeof(cmdline) - 1);
    if (n < 0) n = 0;

    if (n > 0 && cmdline[n - 1] == '\n') --n;

    cmdline[n] = 0;

    taosCloseFile(&pFile);
  } else {
    cmdline[0] = 0;
  }

  return cmdline;
}

#endif

#if !(defined(_TD_WINDOWS_64) || defined(_TD_WINDOWS_32))

SysNameInfo taosGetSysNameInfo() {
  SysNameInfo info = {0};

  struct utsname uts;
  if (!uname(&uts)) {
    tstrncpy(info.sysname, uts.sysname, sizeof(info.sysname));
    tstrncpy(info.nodename, uts.nodename, sizeof(info.nodename));
    tstrncpy(info.release, uts.release, sizeof(info.release));
    tstrncpy(info.version, uts.version, sizeof(info.version));
    tstrncpy(info.machine, uts.machine, sizeof(info.machine));
  }

  return info;
}

int32_t taosGetEmail(char *email, int32_t maxLen) {
  const char *filepath = "/usr/local/taos/email";

  TdFilePtr pFile = taosOpenFile(filepath, TD_FILE_READ);
  if (pFile == NULL) return false;

  if (taosReadFile(pFile, (void *)email, maxLen) < 0) {
    taosCloseFile(&pFile);
    return -1;
  }

  taosCloseFile(&pFile);
  return 0;
}

int32_t taosGetOsReleaseName(char *releaseName, int32_t maxLen) {
  char   *line = NULL;
  size_t  size = 0;
  int32_t code = -1;

  TdFilePtr pFile = taosOpenFile("/etc/os-release", TD_FILE_READ | TD_FILE_STREAM);
  if (pFile == NULL) return false;

  while ((size = taosGetLineFile(pFile, &line)) != -1) {
    line[size - 1] = '\0';
    if (strncmp(line, "PRETTY_NAME", 11) == 0) {
      const char *p = strchr(line, '=') + 1;
      if (*p == '"') {
        p++;
        line[size - 2] = 0;
      }
      tstrncpy(releaseName, p, maxLen);
      code = 0;
      break;
    }
  }

  if (line != NULL) free(line);
  taosCloseFile(&pFile);
  return code;
}

int32_t taosGetCpuInfo(char *cpuModel, int32_t maxLen, float *numOfCores) {
  char   *line = NULL;
  size_t  size = 0;
  int32_t done = 0;
  int32_t code = -1;

  TdFilePtr pFile = taosOpenFile("/proc/cpuinfo", TD_FILE_READ | TD_FILE_STREAM);
  if (pFile == NULL) return false;

  while (done != 3 && (size = taosGetLineFile(pFile, &line)) != -1) {
    line[size - 1] = '\0';
    if (((done & 1) == 0) && strncmp(line, "model name", 10) == 0) {
      const char *v = strchr(line, ':') + 2;
      tstrncpy(cpuModel, v, maxLen);
      code = 0;
      done |= 1;
    } else if (((done & 2) == 0) && strncmp(line, "cpu cores", 9) == 0) {
      const char *v = strchr(line, ':') + 2;
      *numOfCores = atof(v);
      done |= 2;
    }
  }

  if (line != NULL) free(line);
  taosCloseFile(&pFile);

  return code;
}

#endif
