//
// Created by cain on 2018/12/28.
//

#ifndef CAIN_CainThread_H
#define CAIN_CainThread_H

#include <Mutex.h>
#include <Condition.h>

typedef enum {
    Priority_Default = -1,
    Priority_Low = 0,
    Priority_Normal = 1,
    Priority_High = 2
} CainThreadPriority;

class Runnable {
public:
    virtual ~Runnable(){}

    virtual void run() = 0;
};

/**
 * CainThread can use a custom Runnable, but must delete Runnable constructor yourself
 */
class CainThread : public Runnable {
public:

    CainThread();

    CainThread(CainThreadPriority priority);

    CainThread(Runnable *runnable);

    CainThread(Runnable *runnable, CainThreadPriority priority);

    virtual ~CainThread();

    void start();

    void join();

    void detach();

    pthread_t getId() const;

    bool isActive() const;

protected:
    static void *CainThreadEntry(void *arg);

    int schedPriority(CainThreadPriority priority);

    virtual void run();

protected:
    Mutex mMutex;
    Condition mCondition;
    Runnable *mRunnable;
    CainThreadPriority mPriority; // CainThread priority
    pthread_t mId;  // CainThread id
    bool mRunning;  // CainThread running
    bool mNeedJoin; // if call detach function, then do not call join function
};

inline CainThread::CainThread() {
    mNeedJoin = true;
    mRunning = false;
    mId = -1;
    mRunnable = NULL;
    mPriority = Priority_Default;
}

inline CainThread::CainThread(CainThreadPriority priority) {
    mNeedJoin = true;
    mRunning = false;
    mId = -1;
    mRunnable = NULL;
    mPriority = priority;
}

inline CainThread::CainThread(Runnable *runnable) {
    mNeedJoin = false;
    mRunning = false;
    mId = -1;
    mRunnable = runnable;
    mPriority = Priority_Default;
}

inline CainThread::CainThread(Runnable *runnable, CainThreadPriority priority) {
    mNeedJoin = false;
    mRunning = false;
    mId = -1;
    mRunnable = runnable;
    mPriority = priority;
}

inline CainThread::~CainThread() {
    join();
    mRunnable = NULL;
}

inline void CainThread::start() {
    if (!mRunning) {
        pthread_create(&mId, NULL, CainThreadEntry, this);
        mNeedJoin = true;
    }
    // wait CainThread to run
    mMutex.lock();
    while (!mRunning) {
        mCondition.wait(mMutex);
    }
    mMutex.unlock();
}

inline void CainThread::join() {
    Mutex::Autolock lock(mMutex);
    if (mId > 0 && mNeedJoin) {
        pthread_join(mId, NULL);
        mNeedJoin = false;
        mId = -1;
    }
}

inline  void CainThread::detach() {
    Mutex::Autolock lock(mMutex);
    if (mId >= 0) {
        pthread_detach(mId);
        mNeedJoin = false;
    }
}

inline pthread_t CainThread::getId() const {
    return mId;
}

inline bool CainThread::isActive() const {
    return mRunning;
}

inline void* CainThread::CainThreadEntry(void *arg) {
    CainThread *thread = (CainThread *) arg;

    if (thread != NULL) {
        thread->mRunning = true;
        thread->mCondition.signal();

        thread->schedPriority(thread->mPriority);

        // when runnable is exit，run runnable else run()
        if (thread->mRunnable) {
            thread->mRunnable->run();
        } else {
            thread->run();
        }

        thread->mRunning = false;
        thread->mCondition.signal();
    }

    pthread_exit(NULL);

    return NULL;
}

inline int CainThread::schedPriority(CainThreadPriority priority) {
    if (priority == Priority_Default) {
        return 0;
    }

    struct sched_param sched;
    int policy;
    pthread_t thread = pthread_self();
    if (pthread_getschedparam(thread, &policy, &sched) < 0) {
        return -1;
    }
    if (priority == Priority_Low) {
        sched.sched_priority = sched_get_priority_min(policy);
    } else if (priority == Priority_High) {
        sched.sched_priority = sched_get_priority_max(policy);
    } else {
        int min_priority = sched_get_priority_min(policy);
        int max_priority = sched_get_priority_max(policy);
        sched.sched_priority = (min_priority + (max_priority - min_priority) / 2);
    }

    if (pthread_setschedparam(thread, policy, &sched) < 0) {
        return -1;
    }
    return 0;
}

inline void CainThread::run() {
    // do nothing
}


#endif //CainThread_H
