#pragma once

#include "ccapi/srs_ccapi.hpp"

using namespace srs_ccapi;

class SrsImplCcApiHandler;

class SrsImplCcApiWorker
{
public:
    friend class SrsImplCcApiHandler;

public:
    SrsImplCcApiWorker();
    ~SrsImplCcApiWorker();

public:
    bool dostart(int evfd_srs_read, int evfd_srs_write, int shmid);
    void dostop();
    void notifyev();

private:
    std::string status_info();

private:
    srs_netfd_t m_ev_netfd_srs_read;
    srs_netfd_t m_ev_netfd_srs_write;
    std::shared_ptr<SrsImplCcApiHandler> m_read_handler;
    std::shared_ptr<SrsImplCcApiHandler> m_write_handler;
    srs_cond_t m_write_cond;
};
