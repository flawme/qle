#pragma once

#ifdef _WIN32
#include <windows.h>
#include <io.h>

#define PROT_READ     0x1
#define PROT_WRITE    0x2
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FAILED    ((void *)-1)

static inline void *mmap(void *addr, size_t length, int prot, int flags, int fd, long long offset) {
    HANDLE hFile = (HANDLE)_get_osfhandle(fd);
    if (hFile == INVALID_HANDLE_VALUE) {
        return MAP_FAILED;
    }

    DWORD flProtect = PAGE_READONLY;
    if (prot & PROT_WRITE) {
        flProtect = (flags & MAP_PRIVATE) ? PAGE_WRITECOPY : PAGE_READWRITE;
    }

    HANDLE hMapping = CreateFileMappingA(hFile, NULL, flProtect, 0, 0, NULL);
    if (hMapping == NULL) {
        return MAP_FAILED;
    }

    DWORD dwDesiredAccess = FILE_MAP_READ;
    if (prot & PROT_WRITE) {
        dwDesiredAccess = (flags & MAP_PRIVATE) ? FILE_MAP_COPY : FILE_MAP_WRITE;
    }

    void *map = MapViewOfFile(hMapping, dwDesiredAccess, 0, (DWORD)(offset >> 32), (DWORD)(offset & 0xFFFFFFFF), length);
    CloseHandle(hMapping);

    if (map == NULL) {
        return MAP_FAILED;
    }

    return map;
}

static inline int munmap(void *addr, size_t length) {
    (void)length;
    return UnmapViewOfFile(addr) ? 0 : -1;
}

#else
#include <sys/mman.h>
#include <unistd.h>
#endif
