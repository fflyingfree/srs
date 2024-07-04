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
    /* /app/stream | vhost/app/stream */
    std::string _stream_id;
};

class SrsCcApiNotifyMsgBase : public SrsCcApiMsg
{
public:
    enum EM_SRS_CCAPI_NOTIFY_MSG_TYPE
    {
        e_srs_ccapi_notifymsg_ping = 1001,
        e_srs_ccapi_notifymsg_signal = 1002,
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
        e_srs_ccapi_mediamsg_videoframe = 2001,
        e_srs_ccapi_mediamsg_audioframe = 2002,
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

class SrsCcApiNotifyMsgSignal : public SrsCcApiNotifyMsgBase
{
public:
    SrsCcApiNotifyMsgSignal(long msgId) : SrsCcApiNotifyMsgBase(msgId, e_srs_ccapi_notifymsg_signal, "", 0) {
        signo = 0;
    }
    virtual ~SrsCcApiNotifyMsgSignal() { }

public:
    int signo;
};

class SrsCcApiMediaMsgVideoFrame : public SrsCcApiMediaMsgBase
{
public:
    enum __em_vframe_type {
        _e_vUnknownFrame = 0,
        _e_vIFrame = 1,
        _e_vPFrame = 2,
        _e_vBFrame = 3,
        _e_vConfigFrame = 4,
        _e_vPBFrame = 5,
    };

public:
    SrsCcApiMediaMsgVideoFrame(long msgId, const std::string& streamId) : SrsCcApiMediaMsgBase(msgId, e_srs_ccapi_mediamsg_videoframe, streamId) {
        vframeType = _e_vUnknownFrame;
        codecId = -1;
        dts = 0;
        cts = 0;
    }
    virtual ~SrsCcApiMediaMsgVideoFrame() { }

public:
    __em_vframe_type vframeType;
    int codecId;
    uint32_t dts;
    uint32_t cts;
    std::list<std::string> naluStrList;
};

class SrsCcApiMediaMsgAudioFrame : public SrsCcApiMediaMsgBase
{
public:
    enum __em_aframe_type {
        _e_aUnknownFrame = 0,
        _e_aIFrame = 1,
        _e_aConfigFrame = 4,
    };

public:
    SrsCcApiMediaMsgAudioFrame(long msgId, const std::string& streamId) : SrsCcApiMediaMsgBase(msgId, e_srs_ccapi_mediamsg_audioframe, streamId) {
        aframeType = _e_aUnknownFrame;
        codecId = -1;
        dts = 0;
    }
    virtual ~SrsCcApiMediaMsgAudioFrame() { }

public:
    __em_aframe_type aframeType;
    int codecId;
    uint32_t dts;
    std::string rawStr;
    std::map<std::string, std::string> extraMap;
};

};
