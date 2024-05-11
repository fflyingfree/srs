#pragma once

#include "_inner_srs_ccapi_comm.hpp"

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
        e_srs_ccapi_notifymsg_ping = 1001,
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
        e_srs_ccapi_mediamsg_frame = 2001,
    };

public:
    SrsCcApiMediaMsgBase(long msgId, int msgType, const std::string& streamId) : SrsCcApiMsg(msgId, msgType, streamId) { }
    virtual ~SrsCcApiMediaMsgBase() { }
};

//----------------------------------------------------------------------------------
//@共享内存传递的msg (specific)
class SrsCcApiNotifyMsgPing : public SrsCcApiNotifyMsgBase
{
public:
    SrsCcApiNotifyMsgPing(long msgId) : SrsCcApiNotifyMsgBase(msgId, e_srs_ccapi_notifymsg_ping, "", 0) {
    }
    virtual ~SrsCcApiNotifyMsgPing() { }
};

class SrsCcApiMediaMsgFrame : public SrsCcApiMediaMsgBase
{
public:
    SrsCcApiMediaMsgFrame(long msgId, const std::string& streamId) : SrsCcApiMediaMsgBase(msgId, e_srs_ccapi_mediamsg_frame, streamId) {
    }
    virtual ~SrsCcApiMediaMsgFrame() { }

public:
    //todo
};

};
