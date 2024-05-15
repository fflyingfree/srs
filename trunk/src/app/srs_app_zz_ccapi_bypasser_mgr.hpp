#pragma once

#include "srs_ccapi.hpp"
#include "srs_kernel_flv.hpp"
#include <unordered_map>

using namespace srs_ccapi;

class SrsCcApiInnerStreamSourceInfo;

class SrsCcApiByPasserMgr
{
public:
    SrsCcApiByPasserMgr();
    ~SrsCcApiByPasserMgr();

public:
    void toPassRtmpVideoFrame(const std::string& streamId, SrsSharedPtrMessage* frameMsg);
    void toPassRtmpAudioFrame(const std::string& streamId, SrsSharedPtrMessage* frameMsg);
    
private:
    std::shared_ptr<SrsCcApiInnerStreamSourceInfo> touchStreamSourceInfo(const std::string& streamId, bool toPass);

private:
    std::unordered_map<std::string, std::shared_ptr<SrsCcApiInnerStreamSourceInfo>> mStreamSourceMapToPass;
    std::unordered_map<std::string, std::shared_ptr<SrsCcApiInnerStreamSourceInfo>> mStreamSourceMapOnPass;
};

extern SrsCcApiByPasserMgr gSrsCcApiByPasserMgr;
