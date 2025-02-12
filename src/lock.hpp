#pragma once

#include <shared_mutex>

enum class LockState
{
    Unlocked,
    Shared,
    Unique,
};

struct SharedLockGuard
{
    std::shared_mutex* mutex;
    LockState prior_state;
    LockState state;

    SharedLockGuard(std::shared_mutex& _mutex, LockState initial_state, LockState _prior_state = LockState::Unlocked)
        : mutex(&_mutex)
        , prior_state(_prior_state)
        , state(_prior_state)
    {
        SetState(initial_state);
    }

    SharedLockGuard(SharedLockGuard& _lock, LockState initial_state)
        : mutex(_lock.mutex)
        , prior_state(_lock.state)
        , state(_lock.state)
    {
        SetState(initial_state);
    }

    ~SharedLockGuard()
    {
        SetState(prior_state);
    }

    LockState GetState()
    {
        return state;
    }

    void SetState(LockState new_state)
    {
        if (new_state == state) return;

        if      (state == LockState::Shared) mutex->unlock_shared();
        else if (state == LockState::Unique) mutex->unlock();

        if      (new_state == LockState::Shared) mutex->lock_shared();
        else if (new_state == LockState::Unique) mutex->lock();

        state = new_state;
    }

    void LockUnique()
    {
        if (state == LockState::Unique) return;
        if (state == LockState::Shared) mutex->unlock_shared();
        mutex->lock();
        state = LockState::Unique;
    }

    void LockShared()
    {
        if (state == LockState::Shared) return;
        if (state == LockState::Unique) mutex->unlock();
        mutex->lock_shared();
        state = LockState::Shared;
    }

    void Unlock()
    {
        if      (state == LockState::Shared) mutex->unlock_shared();
        else if (state == LockState::Unique) mutex->unlock();
    }
};
