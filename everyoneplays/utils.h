#pragma once
#include <stddef.h>
#include <windows.h>

// for mutex and shared mem
#define APP_KEY_MUTEX "EveryonePlaysGames_Mutex"
#define APP_KEY_MEM "EveryonePlaysGames_Mem"

enum CommandToken
{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    L,
    R,
    A,
    B,
    X,
    Y,
    START,
    SELECT,
    XNYN,
    ANARCHY,
    DEMOCRACY,
    RED,
    BLUE,
    NUMBER_COMMAND_TOKEN
};

template<typename T, unsigned int NUM_ELEM>
struct CircularQueue
{
    char mem[sizeof(T)*NUM_ELEM];

    size_t pos;
    size_t usedElems;
    size_t getStart;

    enum { TOTAL_SIZE = sizeof(T)*NUM_ELEM };

    CircularQueue()
        : pos(0), usedElems(0), getStart(0)
    {
        static_assert(NUM_ELEM > 0, "number of elements must be greater than 0");
    }

    void push(const T &item)
    {
        T *dst = (T*)(mem + pos);
        *dst = item;
        pos = (pos + sizeof(T)) % TOTAL_SIZE;
        if (usedElems < NUM_ELEM)
        {
            usedElems++;
        }
        else
        {
            getStart = pos;
        }
    }

    size_t size() const
    {
        return usedElems;
    }

    const T *get(size_t i) const
    {
        i = (i*sizeof(T)+getStart) % TOTAL_SIZE;
        return (T*)(mem + i);
    }

    void clear()
    {
        pos = 0;
        usedElems = 0;
    }
};

struct KeyPress
{
    // for key presses
    CommandToken cmd;
    int presses;
    int x;
    int y;
    int is_active;

    //signal the emulator to reload mutex+memory
    int require_reload;
    // tell the score its safe to quit
    int ready_for_quit;
};


HANDLE shared_mutex_create(const char *name)
{
    return CreateMutexA(NULL,
                        FALSE,
                        name);
}

HANDLE shared_mutex_open(const char *name)
{
    return OpenMutexA(SYNCHRONIZE,
                        FALSE,
                        name);
}

/*
usage:
if(shared_mutex_enter(mutex))
{
    // code
    shared_mutex_exit(mutex);
}
*/
bool shared_mutex_enter(HANDLE mutex)
{
    for (int i = 0; i < 5; i++)
    {
        switch (WaitForSingleObject(mutex, INFINITE))
        {
            // The thread got ownership of the mutex
        case WAIT_OBJECT_0:
            return true;
        }
    }
    return false;
}

// this function must be called if shared_mutex_wait() returns true
void shared_mutex_exit(HANDLE mutex)
{
    ReleaseMutex(mutex);
}

void shared_mutex_close(HANDLE mutex)
{
    CloseHandle(mutex);
}

void *create_shared_mem(const char *name, unsigned int size, HANDLE *outhandle)
{
    HANDLE hMapFile;
    void *pBuf;

    hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security
        PAGE_READWRITE,          // read/write access
        0,                       // maximum object size (high-order DWORD)
        size,                // maximum object size (low-order DWORD)
        name);                 // name of mapping object

    if (hMapFile == NULL)
    {
        return 0;
    }
    pBuf = MapViewOfFile(hMapFile,   // handle to map object
        FILE_MAP_ALL_ACCESS, // read/write permission
        0,
        0,
        size);

    if (pBuf == NULL)
    {
        CloseHandle(hMapFile);

        return 0;
    }

    *outhandle = hMapFile;
    return pBuf;
}

void *open_shared_mem(const char *name, unsigned int size, HANDLE *outhandle)
{
    HANDLE hMapFile;
    void *pBuf;

    hMapFile = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,   // read/write access
        FALSE,                 // do not inherit the name
        name);               // name of mapping object

    if (hMapFile == NULL)
    {
        return 0;
    }

    pBuf = MapViewOfFile(hMapFile, // handle to map object
        FILE_MAP_ALL_ACCESS,  // read/write permission
        0,
        0,
        size);

    if (pBuf == NULL)
    {
        CloseHandle(hMapFile);
        return 0;
    }

    *outhandle = hMapFile;
    return pBuf;
}

void close_shared_mem(HANDLE handle, void *buf)
{
    UnmapViewOfFile(buf);
    CloseHandle(handle);
}
