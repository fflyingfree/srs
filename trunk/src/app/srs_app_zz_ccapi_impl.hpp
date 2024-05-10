#pragma once

#include "srs_ccapi.hpp"
#include "srs_protocol_st.hpp"

using namespace srs_ccapi;

class SrsCcApiImplHandler;

class SrsCcApiImplWorker
{
public:
    friend class SrsCcApiImplHandler;

public:
    SrsCcApiImplWorker();
    ~SrsCcApiImplWorker();

public:
    bool dostart(int evfd_srs_read, int evfd_srs_write, int shmid);
    void dostop();
    void notifyev();

private:
    std::string status_info();

private:
    srs_netfd_t m_ev_netfd_srs_read;
    srs_netfd_t m_ev_netfd_srs_write;
    std::shared_ptr<SrsCcApiImplHandler> m_read_handler;
    std::shared_ptr<SrsCcApiImplHandler> m_write_handler;
    srs_cond_t m_write_cond;
};
