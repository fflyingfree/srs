#include "srs_impl_ccapi_worker.hpp"
#include "app/srs_app_st.hpp"
#include "kernel/srs_kernel_log.hpp"
#include <sys/syscall.h>

std::atomic<SrsCcApiSharedMemory*> g_srs_ccapi_shmptr(nullptr);
std::atomic<int> g_srs_ccapi_shmid(-1);

class SrsImplCcApiHandler : public ISrsCoroutineHandler
{
public:
    SrsImplCcApiHandler(SrsImplCcApiWorker* pworker, bool isread) {
        m_worker = pworker;
        m_is_read = isread;
        std::ostringstream ssCid;
        if(m_is_read) {
            ssCid << "SrsImplCcApiWorkerReadHandler#t";
        }else{
            ssCid << "SrsImplCcApiWorkerWriteHandler#t";
        }
        ssCid << syscall(__NR_gettid);
        m_cid.set_value(ssCid.str());
        m_trd = nullptr;
        srs_info("Notice srsimplccapi, handler cid(%s) created", m_cid.c_str());
    }
    virtual ~SrsImplCcApiHandler() {
        srs_info("Notice srsimplccapi, handler cid(%s) m_trd(%p) delete", m_cid.c_str(), m_trd);
        if(m_trd) {
            m_trd->stop();
            srs_freep(m_trd);
            m_trd = nullptr;
        }
    }

public:
    bool start_handler() {
        std::string tname = std::string(m_cid.c_str());
        m_trd = new SrsFastCoroutine(tname, this, m_cid);
        srs_error_t err = srs_success;
        if((err = m_trd->start()) != srs_success) {
            srs_error("Error srsimplccapi, handler cid(%s) start failed err(%d)", tname.c_str(), err);
            return false;
        }
        srs_info("Notice srsimplccapi, handler cid(%s) start finished", tname.c_str());
        return true;
    }

public:
    virtual srs_error_t cycle() {
        if(m_is_read) {
            return do_cycle_on_read_trd();
        }else{
            return do_cycle_on_write_trd();
        }
    }

private:
    srs_error_t do_cycle_on_read_trd() {
        srs_info("Notice srsimplccapi, handler cid(%s) trd start cycle_on_read", m_cid.c_str());
        srs_error_t err = srs_success;
        for(;;) {
            long one = 0;
            int nret = st_read((st_netfd_t)m_worker->m_ev_netfd_srs_read, &one, sizeof(one), 100*1000);
            if(nret == 0) {
                srs_error("Error srsimplccapi, handler cid(%s) st_read close with nret 0, exit", m_cid.c_str());
                exit(1);
            }else if(nret < 0) {
                if(!(errno == EAGAIN || errno == EINTR)) {
                    srs_error("Error srsimplccapi, handler cid(%s) st_read error(nret:%d err:%d)", m_cid.c_str(), nret, errno);
                    exit(1);
                }
            }
            SrsCcApiSharedMemory* shmptr = shm_get();
            if(!shmptr) {
                srs_error("Error srsimplccapi, handler cid(%s) shmptr nullptr", m_cid.c_str());
            }
            if(shmptr && shmptr->msgCount(false) > 0) {
                for(int loop = 0; loop < 1000; ++loop) {
                    if(shmptr->msgCount(false) > 0) {
                        std::shared_ptr<SrsCcApiMsg> pmsg = shmptr->getMsg(false);
                        if(pmsg) {
                            handler_msg_on_read(pmsg);
                        }
                    }else{
                        break;
                    }
                }
            }
        }
        return err;
    }

    srs_error_t do_cycle_on_write_trd() {
        srs_info("Notice srsimplccapi, handler cid(%s) trd start cycle_on_write", m_cid.c_str());
        srs_error_t err = srs_success;
        for(;;) {
            srs_cond_timedwait(100*1000);
            SrsCcApiSharedMemory* shmptr = shm_get();
            if(!shmptr) {
                srs_error("Error srsimplccapi, handler cid(%s) shmptr nullptr", m_cid.c_str());
            }
            if(shmptr && shmptr->msgCount(true) > 0) {
                long one = 1;
                int wret = st_write((st_netfd_t)m_worker->m_ev_netfd_srs_write, &one, sizeof(one), 100*1000);
                if(wret == 0) {
                    srs_error("Error srsimplccapi, handler cid(%s) st_read close with wret 0, exit", m_cid.c_str());
                    exit(1);
                }else if(wret < 0) {
                    if(!(errno == EAGAIN || errno == EINTR)) {
                        srs_error("Error srsimplccapi, handler cid(%s) st_read error(wret:%d err:%d)", m_cid.c_str(), wret, errno);
                        exit(1);
                    }
                }
            }
        }
        return err;
    }

private:
    void handler_msg_on_read(std::shared_ptr<SrsCcApiMsg> pmsg) {
        srs_info("Debug srsimplccapi, handler cid(%s), pmsg:%p", m_cid.c_str(), pmsg.get());
    }


private:
    SrsImplCcApiWorker* m_worker;
    bool m_is_read;
    SrsContextId m_cid;
    SrsFastCoroutine* m_trd;
    
};

SrsImplCcApiWorker::SrsImplCcApiWorker() {
    m_ev_netfd_srs_read = nullptr;
    m_ev_netfd_srs_write = nullptr;
    m_write_cond = nullptr;
}

SrsImplCcApiWorker::~SrsImplCcApiWorker() {
    dostop();
}

bool SrsImplCcApiWorker::dostart(int evfd_srs_read, int evfd_srs_write, int shmid) {
    if(evfd_srs_read <= 0 || evfd_srs_write <= 0 || shmid <= 0) {
        dostop();
        return false;
    }
    int _evfd_srs_read = evfd_srs_read;
    int _evfd_srs_write = evfd_srs_write;
    m_ev_netfd_srs_read = srs_netfd_open(_evfd_srs_read);
    m_ev_netfd_srs_write = srs_netfd_open(_evfd_srs_write);
    if(m_ev_netfd_srs_read == nullptr || m_ev_netfd_srs_write == nullptr) {
        dostop();
        return false;
    }
    g_srs_ccapi_shmid = shmid;
    g_srs_ccapi_shmptr = shm_get();
    if(g_srs_ccapi_shmptr.load() == nullptr || shm_get() == nullptr) {
        dostop();
        return false;
    }
    m_read_handler = std::make_shared<SrsImplCcApiHandler>(this, true);
    m_write_cond = srs_cond_new();
    m_write_handler = std::make_shared<SrsImplCcApiHandler>(this, false);
    if(!m_read_handler->start_handler() || !m_write_handler->start_handler()) {
        dostop();
        return false;
    }
    srs_info("Notice srsimplccapi, worker started with params(ev_fd:(r:%d w:%d) shmid:%d) on status(%s)..",
        evfd_srs_read, evfd_srs_write, shmid, status_info().c_str());
    return true;
}

void SrsImplCcApiWorker::dostop() {
    srs_info("Notice srsimplccapi, worker stopping on status(%s)..", status_info().c_str());
    shm_detach();
    g_srs_ccapi_shmptr = nullptr;
    g_srs_ccapi_shmid = -1;
    if(m_ev_netfd_srs_read) {
        st_netfd_free((st_netfd_t)m_ev_netfd_srs_read);
        m_ev_netfd_srs_read = nullptr;
    }
    if(m_ev_netfd_srs_write) {
        st_netfd_free((st_netfd_t)m_ev_netfd_srs_write);
        m_ev_netfd_srs_write = nullptr;
    }
    m_read_handler.reset();
    m_write_handler.reset();   
    if(m_write_cond) {
        srs_cond_destroy(m_write_cond);
        m_write_cond = nullptr;
    }
}

void SrsImplCcApiWorker::notifyev() {
    srs_cond_signal(m_write_cond);
}

std::string SrsImplCcApiWorker::status_info() {
    std::ostringstream ssinfo;
    ssinfo << "g_srs_ccapi_shmptr:" << g_srs_ccapi_shmptr.load();
    ssinfo << " g_srs_ccapi_shmid:" << g_srs_ccapi_shmid.load();
    ssinfo << " ev_netfd(r:" << (void*)m_ev_netfd_srs_read << "," << (m_ev_netfd_srs_read ? ((st_netfd_t)m_ev_netfd_srs_read)->osfd : 0);
    ssinfo << " w:" << (void*)m_ev_netfd_srs_write << "," << (m_ev_netfd_srs_write ? ((st_netfd_t)m_ev_netfd_srs_write)->osfd : 0) << ")";
    ssinfo << " handler(r:" << m_read_handler.get() << " w:" << m_write_handler.get() << ")";
    ssinfo << " write_cond(" << (void*)m_write_cond << ")";
    return ssinfo.str();  
}
