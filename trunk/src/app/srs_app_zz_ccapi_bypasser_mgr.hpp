#pragma once

#include "srs_ccapi.hpp"
#include "srs_kernel_flv.hpp"
#include "srs_app_source.hpp"
#include <unordered_map>

using namespace srs_ccapi;

class SrsCcApiInnerStreamSourceInfo;

class SrsCcApiByPasserMgr : public ISrsLiveSourceHandler
{
public:
    SrsCcApiByPasserMgr();
    ~SrsCcApiByPasserMgr();

public:
    void toPassRtmpVideoFrame(const std::string& streamId, SrsSharedPtrMessage* frameMsg);
    void toPassRtmpAudioFrame(const std::string& streamId, SrsSharedPtrMessage* frameMsg);
    
public:
    void onPassVideoFrame(SrsCcApiMediaMsgVideoFrame* videoFrame);
    void onPassAudioFrame(SrsCcApiMediaMsgAudioFrame* audioFrame);

private:
    //callback for onPass mode
    virtual srs_error_t on_publish(SrsLiveSource* s, SrsRequest* r);
    virtual void on_unpublish(SrsLiveSource* s, SrsRequest* r);

private:
    std::shared_ptr<SrsCcApiInnerStreamSourceInfo> touchStreamSourceInfo(const std::string& streamId, bool toPass);

private:
    std::unordered_map<std::string, std::shared_ptr<SrsCcApiInnerStreamSourceInfo>> mStreamSourceMapToPass;
    std::unordered_map<std::string, std::shared_ptr<SrsCcApiInnerStreamSourceInfo>> mStreamSourceMapOnPass;
};

extern SrsCcApiByPasserMgr gSrsCcApiByPasserMgr;
