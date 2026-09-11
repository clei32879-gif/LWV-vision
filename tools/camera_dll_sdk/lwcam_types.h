/** @file lwcam_types.h — 海康 MVS x64 结构复刻 (供外接DLL封装用) */
#pragma once
#pragma pack(push, 8)

// MV_CC_DEVICE_INFO (x64, 复刻关键区; 总大小按 MVS 头 2168B 预留)
struct MvDeviceInfo {
    unsigned short nMajorVer;
    unsigned short nMinorVer;
    unsigned char  spec[1024];          // union SpecialInfo (GigE/USB/CAMLink/CXEP)
    char chUserDefinedName[256];
    char chModelName[64];
    char chSerialNumber[64];
    char _tail[512];
};

struct MvDeviceInfoList {
    unsigned int nDeviceNum;
    MvDeviceInfo* pDeviceInfo[256];
};

// MV_FRAME_OUT 头部 (pBuf/nFrameLen/帧信息)
struct MvFrameOut {
    unsigned char* pBuf;
    unsigned int nFrameLen;
    unsigned int _pad;
    unsigned int nWidth;
    unsigned int nHeight;
    unsigned int enPixelType;
    unsigned int _pad2[4];
};

#pragma pack(pop)
