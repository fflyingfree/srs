#pragma once

#include "_inner_srs_ccapi_comm.hpp"
#include "_inner_srs_ccapi_util.hpp"
#include "srs_ccapi_msg.hpp"

#include <sys/eventfd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

namespace srs_ccapi
{

//----------------------------------------------------------------------------------
//@共享内存数据结构
class SrsCcApiSharedMemory
{
public:
    SrsCcApiSharedMemory() {
    }
    ~SrsCcApiSharedMemory() {
    }

public:
    void init() {
        _msg_fromsrs_count = 0;
        _msg_tosrs_count = 0;
        _msg_fromsrs_locker.init();
        _msg_tosrs_locker.init();
        _msg_fromsrs_que = std::deque<std::shared_ptr<SrsCcApiMsg>>();
        _msg_tosrs_que = std::deque<std::shared_ptr<SrsCcApiMsg>>();
    }
    void destroy() {
        _msg_fromsrs_que.clear();
        _msg_tosrs_que.clear();
    }

public:
    long msgCount(bool fromSrs) {
        if(fromSrs) {
            return _msg_fromsrs_count.load();
        }else{
            return _msg_tosrs_count.load();
        }
    }
    //putMsg之后，需要写eventfd通知对方！！
    void putMsg(std::shared_ptr<SrsCcApiMsg> pMsg, bool fromSrs) {
        if(!pMsg) {
            return;
        }
        auto doPut = [pMsg](srs_ccapi_SpinLocker& _locker, std::deque<std::shared_ptr<SrsCcApiMsg>>& _que, std::atomic<long>& _count) {
            _locker.lock();
            _que.push_back(pMsg);
            _count = _que.size();
            _locker.unlock();
        };
        if(fromSrs) {
            doPut(_msg_fromsrs_locker, _msg_fromsrs_que, _msg_fromsrs_count);
        }else{
            doPut(_msg_tosrs_locker, _msg_tosrs_que, _msg_tosrs_count);
        }
    }
    //读到eventfd之后，调用getMsg！！
    std::shared_ptr<SrsCcApiMsg> getMsg(bool fromSrs) {
        auto doGet = [](srs_ccapi_SpinLocker& _locker, std::deque<std::shared_ptr<SrsCcApiMsg>>& _que, std::atomic<long>& _count)->std::shared_ptr<SrsCcApiMsg> {
            std::shared_ptr<SrsCcApiMsg> pMsg;
            _locker.lock();
            if(_que.size() > 0) {
                pMsg = _que.front();
                _que.pop_front();
            }
            _count = _que.size();
            _locker.unlock();
            return pMsg;
        };
        if(fromSrs) {
            return doGet(_msg_fromsrs_locker, _msg_fromsrs_que, _msg_fromsrs_count);
        }else{
            return doGet(_msg_tosrs_locker, _msg_tosrs_que, _msg_tosrs_count);
        }
    }

private:
    srs_ccapi_SpinLocker _msg_fromsrs_locker;
    std::deque<std::shared_ptr<SrsCcApiMsg>> _msg_fromsrs_que;
    std::atomic<long> _msg_fromsrs_count;
    srs_ccapi_SpinLocker _msg_tosrs_locker;
    std::deque<std::shared_ptr<SrsCcApiMsg>> _msg_tosrs_que;
    std::atomic<long> _msg_tosrs_count;
};
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//@共享内存指针全局声明，需要在外部定义！！
extern std::atomic<SrsCcApiSharedMemory*> g_srs_ccapi_shmptr;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//@共享内存shmid全局声明，需要在外部定义！！
extern std::atomic<int> g_srs_ccapi_shmid;
//----------------------------------------------------------------------------------
//@共享内存操作函数
inline int shm_create(uint8_t id) {
    key_t key = ftok(".", (int)id);
    int shmid = shmget(key, sizeof(SrsCcApiSharedMemory), IPC_CREAT|0666);
    g_srs_ccapi_shmid = shmid;
    shmid = g_srs_ccapi_shmid.load();
    return shmid;
}

inline SrsCcApiSharedMemory* shm_get() {
    SrsCcApiSharedMemory* shmptr = g_srs_ccapi_shmptr.load();
    if(shmptr != nullptr) {
        return shmptr;
    }
    int shmid = g_srs_ccapi_shmid.load();
    if(shmid >= 0) {
        shmptr = static_cast<SrsCcApiSharedMemory*>(shmat(shmid, nullptr, 0));
        if(shmptr && shmptr != reinterpret_cast<SrsCcApiSharedMemory*>(-1)) {
            if(shmptr != g_srs_ccapi_shmptr.load()) {
                shmptr->init();
                g_srs_ccapi_shmptr = shmptr;
                shmptr = g_srs_ccapi_shmptr.load();
            }
        }
    }
    return shmptr;
}

inline void shm_detach() {
    SrsCcApiSharedMemory* shmptr = g_srs_ccapi_shmptr.load();
    if(shmptr != nullptr) {
        shmptr->destroy();
        shmdt((const void*)shmptr);
    }
    g_srs_ccapi_shmptr = nullptr;
}

inline void shm_remove() {
    int shmid = g_srs_ccapi_shmid.load();
    if(shmid >= 0) {
        shmctl(shmid, IPC_RMID, nullptr);
    }
    g_srs_ccapi_shmptr = nullptr;
    g_srs_ccapi_shmid = 0;
}

//----------------------------------------------------------------------------------
//@打开or关闭eventfd文件描述符
inline int open_eventfd() {
    int fd = eventfd(0, EFD_NONBLOCK /*| EFD_CLOEXEC*/);
    return fd;
}

inline void close_eventfd(int fd) {
    close(fd);
}

};
