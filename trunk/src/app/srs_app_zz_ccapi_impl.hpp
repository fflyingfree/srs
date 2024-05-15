#pragma once

#include "srs_ccapi.hpp"
#include "srs_protocol_st.hpp"

using namespace srs_ccapi;

class SrsCcApiImplTimer;
class SrsCcApiImplHandler;

class SrsCcApiImplWorker
{
public:
    friend class SrsCcApiImplTimer;
    friend class SrsCcApiImplHandler;

public:
    SrsCcApiImplWorker();
    ~SrsCcApiImplWorker();

public:
    bool ison();
    bool dostart(int evfd_srs_read, int evfd_srs_write);
    void dostop();
    void notifyev();

public:
    long allocMsgId();
    void heatPing();
    void postMsg(std::shared_ptr<SrsCcApiMsg> pMsg);

private:
    std::string status_info();

private:
    bool m_switch_on;
    srs_netfd_t m_ev_netfd_srs_read;
    srs_netfd_t m_ev_netfd_srs_write;
    std::shared_ptr<SrsCcApiImplTimer> m_timer;
    std::shared_ptr<SrsCcApiImplHandler> m_read_handler;
    std::shared_ptr<SrsCcApiImplHandler> m_write_handler;
    srs_cond_t m_write_cond;

private:
    long m_msg_gen_id;
};

extern SrsCcApiImplWorker gSrsCcApiImplWorker;
