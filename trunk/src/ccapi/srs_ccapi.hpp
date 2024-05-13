#pragma once

#include "_inner_srs_ccapi_comm.hpp"
#include "_inner_srs_ccapi_util.hpp"
#include "srs_ccapi_msg.hpp"
#include <sys/eventfd.h>

namespace srs_ccapi
{

//-------------------------------------------------------------------------------------------------------------------------
//@线程共享内存数据结构
class SrsCcApiSharedMemory
{
public:
    SrsCcApiSharedMemory() {
        _msg_fromsrs_locker.init();
        _msg_tosrs_locker.init();
        _msg_fromsrs_que.clear();
        _msg_tosrs_que.clear();
        _msg_fromsrs_count = 0;
        _msg_tosrs_count = 0;

    }
    ~SrsCcApiSharedMemory() {
    }

public:
    void reset() {
        auto doClear = [](srs_ccapi_SpinLocker& _locker, std::deque<std::shared_ptr<SrsCcApiMsg>>& _que, std::atomic<long>& _count) {
            _locker.lock();
            _que.clear();
            _count = 0;
            _locker.unlock();
        };
        doClear(_msg_fromsrs_locker, _msg_fromsrs_que, _msg_fromsrs_count);
        doClear(_msg_tosrs_locker, _msg_tosrs_que, _msg_tosrs_count);
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
extern SrsCcApiSharedMemory g_srs_ccapi_shm;

//-------------------------------------------------------------------------------------------------------------------------
//@打开or关闭eventfd文件描述符
inline int open_eventfd() {
    int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    return fd;
}

inline void close_eventfd(int fd) {
    close(fd);
}

};

//-------------------------------------------------------------------------------------------------------------------------
//@libsrs入口函数生命
extern int srs_ccapi_main(int argc, char** argv, char** envp, int ccapi_evfd_srs_read, int ccapi_evfd_srs_write);
