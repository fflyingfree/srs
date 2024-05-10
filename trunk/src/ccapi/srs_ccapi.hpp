#pragma once

#include "_inner_srs_ccapi_util.hpp"
#include <string>
#include <sstream>
#include <memory>
#include <deque>
#include <thread>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <sys/eventfd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

namespace srs_ccapi
{

//----------------------------------------------------------------------------------
//@共享内存传递的msg (base)
class SrsCcApiMsg
{
public:
    SrsCcApiMsg(long msgId, int msgType, const std::string& streamId) : _msg_id(msgId), _msg_type(msgType), _stream_id(streamId) { }
    virtual ~SrsCcApiMsg() { }

public:
    template<typename T>
    static T* fetchSpecificMsg(SrsCcApiMsg* pMsg) {
        return dynamic_cast<T*>(pMsg);
    }

public:
    long _msg_id;
    int _msg_type; //notify_msg_type media_msg_type
    std::string _stream_id;
};

class SrsCcApiNotifyMsgBase : public SrsCcApiMsg
{
public:
    enum EM_SRS_CCAPI_NOTIFY_MSG_TYPE
    {
        e_srs_ccapi_notifymsg_xxx = 1,
    };

public:
    SrsCcApiNotifyMsgBase(long msgId, int msgType, const std::string& streamId, int toRespondMsgType) : SrsCcApiMsg(msgId, msgType, streamId), _to_respond_msg_type(toRespondMsgType) { }
    virtual ~SrsCcApiNotifyMsgBase() { }

public:
    int _to_respond_msg_type;
};

class SrsCcApiMediaMsgBase : public SrsCcApiMsg
{
public:
    enum EM_SRS_CCAPI_MEDIA_MSG_TYPE
    {
        e_srs_ccapi_mediamsg_frame = 1,
    };

public:
    SrsCcApiMediaMsgBase(long msgId, int msgType, const std::string& streamId) : SrsCcApiMsg(msgId, msgType, streamId) { }
    virtual ~SrsCcApiMediaMsgBase() { }
};

//----------------------------------------------------------------------------------
//@共享内存传递的msg (specific)
class SrsCcApiMediaMsgFrame : public SrsCcApiMediaMsgBase
{
public:
    SrsCcApiMediaMsgFrame(long msgId, const std::string& streamId) : SrsCcApiMediaMsgBase(msgId, e_srs_ccapi_mediamsg_frame, streamId) {
    }
    virtual ~SrsCcApiMediaMsgFrame() { }

public:
    //todo
};

//----------------------------------------------------------------------------------
//@共享内存数据结构
class SrsCcApiSharedMemory
{
public:
    SrsCcApiSharedMemory() {
        _msg_fromsrs_count = 0;
        _msg_tosrs_count = 0;
    }
    ~SrsCcApiSharedMemory() {
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
//@共享内存指针全局声明，需要在外部定义！！
extern std::atomic<SrsCcApiSharedMemory*> g_srs_ccapi_shmptr;

//----------------------------------------------------------------------------------
//@共享内存shmid全局声明，需要在外部定义！！
extern std::atomic<int> g_srs_ccapi_shmid;

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
            g_srs_ccapi_shmptr = shmptr;
            shmptr = g_srs_ccapi_shmptr.load();
        }
    }
    return shmptr;
}

inline void shm_detach() {
    SrsCcApiSharedMemory* shmptr = g_srs_ccapi_shmptr.load();
    if(shmptr != nullptr) {
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
    int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    return fd;
}

inline void close_eventfd(int fd) {
    close(fd);
}

};
