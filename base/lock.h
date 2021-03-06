//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/lock.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__LOCK_H
#define _ASPIA_BASE__LOCK_H

#include "aspia_config.h"

#include <stdint.h>

#include "base/macros.h"

namespace aspia {

class Lock
{
public:
    Lock();
    ~Lock();

    void lock();
    bool try_lock();
    void unlock();

private:
#if (_WIN32_WINNT >= 0x0600)
    SRWLOCK lock_;
#else
    CRITICAL_SECTION cs_;
#endif

    DISALLOW_COPY_AND_ASSIGN(Lock);
};

template<class T>
class LockGuard
{
public:
    explicit LockGuard(T *locker) : locker_(locker)
    {
        locker_->lock();
    }

    virtual ~LockGuard()
    {
        locker_->unlock();
    }

private:
    T *locker_;

    DISALLOW_COPY_AND_ASSIGN(LockGuard);
};

} // namespace aspia

#endif // _ASPIA_BASE__LOCK_H
