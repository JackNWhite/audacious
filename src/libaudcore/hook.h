/*
 * hook.h
 * Copyright 2011-2015 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#ifndef LIBAUDCORE_HOOK_H
#define LIBAUDCORE_HOOK_H

#include <libaudcore/templates.h>

// Timer API.  This API allows functions to be registered to run at a given
// periodic rate.  The advantage of this API rather than QueuedFunc (see
// mainloop.h) is that multiple functions can run on the same timer tick,
// reducing CPU wakeups.
// ========================================================================

enum class TimerRate
{
    Hz1,
    Hz4,
    Hz10,
    Hz30,
    count
};

typedef void (*TimerFunc)(void * data);

/* Adds <func> to the list of functions to be called at the given <rate>,
 * unless it has already been added with the same <data>. */
void timer_add(TimerRate rate, TimerFunc func, void * data = nullptr);

/* Removes all instances matching <func> and <data> from the list of functions
 * to be called at the given <rate>.  If <data> is nullptr, all instances
 * matching <func> are removed. */
void timer_remove(TimerRate rate, TimerFunc func, void * data = nullptr);

/* Convenience wrapper for C++ classes.  Allows non-static member functions to
 * be used as timer callbacks.  The Timer should be made a member of the class
 * in question so that timer_remove() is called automatically from the
 * destructor.  Note that the timer is not started automatically. */
template<class T>
class Timer
{
public:
    Timer(TimerRate rate, T * target, void (T::*func)())
        : rate(rate), target(target), func(func)
    {
    }

    void start() const { timer_add(rate, run, (void *)this); }
    void stop() const { timer_remove(rate, run, (void *)this); }

    ~Timer() { stop(); }

    Timer(const Timer &) = delete;
    void operator=(const Timer &) = delete;

private:
    const TimerRate rate;
    T * const target;
    void (T::*const func)();

    static void run(void * timer_)
    {
        auto timer = (const Timer *)timer_;
        (timer->target->*timer->func)();
    }
};

// Hook API.  This API allows functions to be registered to run when a given
// named event, or "hook", is called.
// =========================================================================

typedef void (*HookFunction)(void * data, void * user);

/* Adds <func> to the list of functions to be called when the hook <name> is
 * triggered. */
void hook_associate(const char * name, HookFunction func, void * user);

/* Removes all instances matching <func> and <user> from the list of functions
 * to be called when the hook <name> is triggered.  If <user> is nullptr, all
 * instances matching <func> are removed. */
void hook_dissociate(const char * name, HookFunction func,
                     void * user = nullptr);

/* Triggers the hook <name>. */
void hook_call(const char * name, void * data);

typedef void (*EventDestroyFunc)(void * data);

/* Schedules a call of the hook <name> from the program's main loop.
 * If <destroy> is not nullptr, it will be called on <data> after the
 * hook is called. */
void event_queue(const char * name, void * data,
                 EventDestroyFunc destroy = nullptr);

/* Cancels pending hook calls matching <name> and <data>.  If <data> is nullptr,
 * all hook calls matching <name> are canceled. */
void event_queue_cancel(const char * name, void * data = nullptr);

template<class T, class D>
struct HookTarget
{
    using Func = void (T::*)(D);
    static void run(T * target, Func func, void * d)
    {
        (target->*func)(aud::from_ptr<D>(d));
    }
};

template<class T>
struct HookTarget<T, void>
{
    using Func = void (T::*)();
    static void run(T * target, Func func, void *) { (target->*func)(); }
};

/* Convenience wrapper for C++ classes.  Allows non-static member functions to
 * be used as hook callbacks.  The HookReceiver should be made a member of the
 * class in question so that hook_dissociate() is called automatically from the
 * destructor. */
template<class T, class D = void>
class HookReceiver
{
public:
    using Target = HookTarget<T, D>;
    using Func = typename Target::Func;

    constexpr HookReceiver(T * target, Func func)
        : m_hook(nullptr), m_target(target), m_func(func)
    {
    }

    HookReceiver(const char * hook, T * target, Func func)
        : HookReceiver(target, func)
    {
        connect(hook);
    }

    ~HookReceiver() { disconnect(); }

    HookReceiver(const HookReceiver &) = delete;
    void operator=(const HookReceiver &) = delete;

    void connect(const char * hook)
    {
        disconnect();
        hook_associate(hook, run, this);
        m_hook = hook;
    }

    void disconnect()
    {
        if (!m_hook)
            return;
        hook_dissociate(m_hook, run, this);
        m_hook = nullptr;
    }

private:
    const char * m_hook;
    T * const m_target;
    const Func m_func;

    static void run(void * d, void * recv_)
    {
        auto recv = (const HookReceiver *)recv_;
        Target::run(recv->m_target, recv->m_func, d);
    }
};

#endif /* LIBAUDCORE_HOOK_H */
