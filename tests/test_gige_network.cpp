/**
 * @file test_gige_network.cpp
 * @brief GigE 相机网络诊断工具
 * 
 * 用法: test_gige_network.exe [相机IP]
 * 示例: test_gige_network.exe 169.254.1.11
 *       test_gige_network.exe (测试所有 169.254.x.11/x.22/x.33/x.44)
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

static uint16_t read16be(const uint8_t* p) { return ((uint16_t)p[0] << 8) | p[1]; }
static uint32_t read32be(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

struct NicInfo {
    std::string name;
    std::string ip;
    std::string mask;
    uint32_t ipVal;
    uint32_t maskVal;
};

std::vector<NicInfo> getLocalNics() {
    std::vector<NicInfo> result;
#ifdef _WIN32
    ULONG bufLen = 15000;
    std::vector<uint8_t> buf(bufLen);
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    DWORD ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters, &bufLen);
    if (ret != ERROR_SUCCESS) return result;

    for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) continue;
        for (PIP_ADAPTER_UNICAST_ADDRESS ua = adapter->FirstUnicastAddress; ua; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;
            struct sockaddr_in* addr = (struct sockaddr_in*)ua->Address.lpSockaddr;
            NicInfo nic;
            nic.ip = inet_ntoa(addr->sin_addr);
            nic.ipVal = ntohl(addr->sin_addr.s_addr);

            // 获取子网掩码
            ULONG mask = 0;
            ConvertLengthToIpv4Mask(ua->OnLinkPrefixLength, &mask);
            mask = ntohl(mask);
            struct in_addr maskAddr;
            maskAddr.s_addr = htonl(mask);
            nic.mask = inet_ntoa(maskAddr);
            nic.maskVal = mask;

            // 获取网卡名称
            char name[256];
            WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, name, sizeof(name), NULL, NULL);
            nic.name = name;

            result.push_back(nic);
        }
    }
#endif
    return result;
}

bool testGvcpDiscovery(const char* cameraIp, const char* localIp, int timeoutMs) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return false;

    // 绑定到指定本地网卡
    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = inet_addr(localIp);
    localAddr.sin_port = 0;
    if (bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    // 设置超时
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // GVCP DISCOVERY 包
    uint8_t pkt[16] = {
        0x42, 0x01,  // type=cmd, flags=ack_required (单播)
        0x00, 0x02,  // cmd=DISCOVERY
        0x00, 0x00,  // length=0
        0x00, 0x01,  // packet_id=1
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(cameraIp);
    target.sin_port = htons(3956);

    sendto(sock, (const char*)pkt, sizeof(pkt), 0, (struct sockaddr*)&target, sizeof(target));

    // 等待响应
    uint8_t resp[1024];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    int n = recvfrom(sock, (char*)resp, sizeof(resp), 0, (struct sockaddr*)&from, &fromLen);

    closesocket(sock);

    if (n >= 8) {
        uint16_t ackCmd = read16be(resp + 2);
        printf("    收到响应: %d 字节, 命令=0x%04X, 来自 %s:%d\n",
               n, ackCmd, inet_ntoa(from.sin_addr), ntohs(from.sin_port));
        if (ackCmd == 0x0003) {
            printf("    ✓ DISCOVERY_ACK (相机可达!)\n");
            // 解析设备信息
            if (n >= 568) {
                char vendor[33] = {0}, model[33] = {0}, serial[17] = {0};
                memcpy(vendor, resp + 8 + 52, 32);
                memcpy(model, resp + 8 + 84, 32);
                memcpy(serial, resp + 8 + 168, 16);
                printf("    厂商: %s\n", vendor);
                printf("    型号: %s\n", model);
                printf("    序列号: %s\n", serial);
            }
            return true;
        }
    }

    printf("    ✗ 无响应 (超时 %dms)\n", timeoutMs);
    return false;
}

bool testGvcpReadreg(const char* cameraIp, const char* localIp, int timeoutMs) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return false;

    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = inet_addr(localIp);
    localAddr.sin_port = 0;
    if (bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // GVCP READREG 包 (读取 Device Mode 寄存器 0x00000000)
    uint8_t pkt[12] = {
        0x42, 0x01,  // type=cmd, flags=ack_required
        0x00, 0x80,  // cmd=READREG
        0x00, 0x01,  // length=1 (4 bytes)
        0x00, 0x02,  // packet_id=2
        0x00, 0x00, 0x00, 0x00  // register address = 0x00000000
    };

    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(cameraIp);
    target.sin_port = htons(3956);

    sendto(sock, (const char*)pkt, sizeof(pkt), 0, (struct sockaddr*)&target, sizeof(target));

    uint8_t resp[256];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    int n = recvfrom(sock, (char*)resp, sizeof(resp), 0, (struct sockaddr*)&from, &fromLen);

    closesocket(sock);

    if (n >= 16) {
        uint16_t ackCmd = read16be(resp + 2);
        if (ackCmd == 0x0081) {
            uint32_t value = read32be(resp + 12);
            printf("    ✓ READREG_ACK: 0x%08X (寄存器值)\n", value);
            return true;
        }
    }

    printf("    ✗ READREG 无响应\n");
    return false;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    printf("========================================\n");
    printf("  GigE Vision 相机网络诊断工具\n");
    printf("========================================\n\n");

    // 获取本地网卡
    auto nics = getLocalNics();
    printf("本机网卡 (%d 个):\n", (int)nics.size());
    for (const auto& nic : nics) {
        printf("  %s: %s / %s\n", nic.name.c_str(), nic.ip.c_str(), nic.mask.c_str());
    }
    printf("\n");

    // 相机列表
    struct CamTest { const char* name; const char* ip; };
    std::vector<CamTest> cameras;
    if (argc >= 2) {
        cameras.push_back({"相机", argv[1]});
    } else {
        cameras = {{"CCD1", "169.254.1.11"}, {"CCD2", "169.254.2.22"},
                   {"CCD3", "169.254.3.33"}, {"CCD4", "169.254.4.44"}};
    }

    int okCount = 0;
    for (const auto& cam : cameras) {
        printf("--- %s (%s) ---\n", cam.name, cam.ip);

        // 找匹配的本地网卡 (IP第三段相同)
        uint32_t camIp = ntohl(inet_addr(cam.ip));
        int camThird = (camIp >> 8) & 0xFF;

        std::string matchedNic;
        for (const auto& nic : nics) {
            int nicThird = (nic.ipVal >> 8) & 0xFF;
            if (nicThird == camThird) {
                matchedNic = nic.ip;
                break;
            }
        }

        if (matchedNic.empty()) {
            printf("  ✗ 无匹配网卡 (需要 169.254.%d.x 的网卡)\n\n", camThird);
            continue;
        }

        printf("  匹配网卡: %s\n", matchedNic.c_str());

        // 测试 DISCOVERY
        printf("  [1] DISCOVERY 测试:\n");
        bool discOk = testGvcpDiscovery(cam.ip, matchedNic.c_str(), 3000);

        // 测试 READREG (只有 DISCOVERY 成功才测)
        if (discOk) {
            printf("  [2] READREG 测试:\n");
            testGvcpReadreg(cam.ip, matchedNic.c_str(), 3000);
        }

        if (discOk) okCount++;
        printf("\n");
    }

    printf("========================================\n");
    printf("结果: %d/%d 台相机可达\n", okCount, (int)cameras.size());

    if (okCount == 0) {
        printf("\n排查建议:\n");
        printf("  1. 检查相机是否通电 (指示灯亮?)\n");
        printf("  2. 检查网线是否插好\n");
        printf("  3. 关闭 pylon Viewer 等其他相机软件\n");
        printf("  4. 检查 Windows 防火墙是否阻止 UDP 3956\n");
        printf("  5. 用 pylon Viewer 确认相机 IP 是否正确\n");
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return okCount > 0 ? 0 : 1;
}
