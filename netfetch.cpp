#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wininet.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fstream>
#include <netdb.h>
#include <net/if.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>

typedef unsigned long long ULONG64;
typedef unsigned long ULONG;
#endif

struct PingResult {
    int latency;
    int lossPercent;
};

#ifndef _WIN32
unsigned short icmp_checksum(unsigned short *ptr, int nbytes) {
    long sum;
    unsigned short oddbyte;
    short answer;
    sum = 0;
    while(nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if(nbytes == 1) {
        oddbyte = 0;
        *((unsigned char*)&oddbyte) = *(unsigned char*)ptr;
        sum += oddbyte;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;
    return answer;
}
#endif

void enableANSI() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

std::string getOSVersion() {
    std::string osName = "Unknown";
#ifdef _WIN32
    osName = "Windows";
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != nullptr) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (fxPtr(&rovi) == 0) {
                if (rovi.dwMajorVersion == 10) {
                    if (rovi.dwBuildNumber >= 22000) {
                        osName = "Windows 11";
                    } else {
                        osName = "Windows 10";
                    }
                } else if (rovi.dwMajorVersion == 6 && rovi.dwMinorVersion == 3) {
                    osName = "Windows 8.1";
                } else if (rovi.dwMajorVersion == 6 && rovi.dwMinorVersion == 2) {
                    osName = "Windows 8";
                } else if (rovi.dwMajorVersion == 6 && rovi.dwMinorVersion == 1) {
                    osName = "Windows 7";
                }
                osName += " (Build " + std::to_string(rovi.dwBuildNumber) + ")";
            }
        }
    }
#else
    if (access("/system/bin/getprop", F_OK) == 0) {
        osName = "Android";
        FILE* p = popen("getprop ro.build.version.release", "r");
        if (p) {
            char buf[32];
            if (fgets(buf, sizeof(buf), p)) {
                std::string ver(buf);
                ver.erase(std::remove(ver.begin(), ver.end(), '\n'), ver.end());
                if (!ver.empty()) osName += " " + ver;
            }
            pclose(p);
        }
    } else {
        std::ifstream f("/etc/os-release");
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                if (line.find("PRETTY_NAME=") == 0) {
                    osName = line.substr(12);
                    if (osName.front() == '"' && osName.back() == '"') {
                        osName = osName.substr(1, osName.length() - 2);
                    }
                    break;
                }
            }
        } else {
            osName = "Linux";
        }
    }
#endif
    return osName;
}

const std::string RESET  = "\033[0m";
const std::string CYAN   = "\033[36m";
const std::string WHITE  = "\033[97m";
const std::string GREEN  = "\033[32m";
const std::string RED    = "\033[31m";
const std::string YELLOW = "\033[33m";
const std::string GRAY   = "\033[90m";
const std::string BOLD   = "\033[1m";

std::string colorizeStatus(const std::string& status) {
    if (status == "UP" || status == "OK" || status == "OPEN" || status == "ENABLED" || status == "Enabled" || status == "Yes") {
        return GREEN + status + RESET;
    } else if (status == "DOWN" || status == "ERROR" || status == "CLOSED" || status == "No") {
        return RED + status + RESET;
    } else if (status == "N/A" || status == "Unknown") {
        return YELLOW + status + " " + RED + "(!)" + RESET;
    } else {
        return YELLOW + status + RESET;
    }
}

std::string bar(int percent, bool invertColor = false) {
    int total = 18;
    percent = std::max(0, std::min(100, percent));
    int filled = (percent * total) / 100;

    std::string color = GREEN;
    if (percent > 50) color = YELLOW;
    if (percent > 80) color = RED;

    std::string result = RESET + "[ " + color;
    for (int i = 0; i < filled; i++) result += "/";
    result += GRAY;
    for (int i = filled; i < total; i++) result += "-";
    result += RESET + " ]";

    return result;
}

std::string lossBar(int percent) {
    int total = 18;
    percent = std::max(0, std::min(100, percent));
    int filled = (percent * total) / 100;

    std::string color = GREEN;
    if (percent > 0) color = YELLOW;
    if (percent > 5) color = RED;

    std::string result = RESET + "[ " + color;
    for (int i = 0; i < filled; i++) result += "x";
    result += GRAY;
    for (int i = filled; i < total; i++) result += "-";
    result += RESET + " ]";

    return result;
}

std::string signalBar(int dbm) {
    int total = 18;
    // Map -90dBm to 0% and -30dBm to 100%
    int percent = (dbm + 90) * 100 / 60;
    percent = std::max(0, std::min(100, percent));
    int filled = (percent * total) / 100;

    std::string color = RED;
    if (percent > 40) color = YELLOW;
    if (percent > 70) color = GREEN;

    std::string result = RESET + "[ " + color;
    for (int i = 0; i < filled; i++) result += "!";
    result += GRAY;
    for (int i = filled; i < total; i++) result += "-";
    result += RESET + " ]";

    return result;
}

std::string fetchUrl(const char* url) {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("Netfetch/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "N/A";

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return "N/A";
    }

    std::string result = "";
    char buffer[1024];
    DWORD bytesRead;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());

    return result.empty() ? "N/A" : result;
#else
    std::string urlStr = url;
    std::string host, path = "/";
    if (urlStr.find("http://") == 0) urlStr = urlStr.substr(7);
    size_t slashPos = urlStr.find("/");
    if (slashPos != std::string::npos) {
        host = urlStr.substr(0, slashPos);
        path = urlStr.substr(slashPos);
    } else {
        host = urlStr;
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), "80", &hints, &res) != 0) return "N/A";

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        return "N/A";
    }

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        close(sockfd);
        freeaddrinfo(res);
        return "N/A";
    }
    freeaddrinfo(res);

    std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
        close(sockfd);
        return "N/A";
    }

    std::string response;
    char buffer[1024];
    int bytesRead;
    while ((bytesRead = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }
    close(sockfd);

    size_t bodyPos = response.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        std::string body = response.substr(bodyPos + 4);
        body.erase(std::remove(body.begin(), body.end(), '\n'), body.end());
        body.erase(std::remove(body.begin(), body.end(), '\r'), body.end());
        return body.empty() ? "N/A" : body;
    }
    return "N/A";
#endif
}

struct NetworkStats {
    ULONG64 rxBytes;
    ULONG64 txBytes;
};

NetworkStats getTotalNetworkUsage() {
    NetworkStats stats = {0, 0};
#ifdef _WIN32
    PMIB_IF_TABLE2 pIfTable;
    if (GetIfTable2(&pIfTable) == NO_ERROR) {
        for (ULONG i = 0; i < pIfTable->NumEntries; i++) {
            MIB_IF_ROW2* row = &pIfTable->Table[i];
            if (row->Type != IF_TYPE_SOFTWARE_LOOPBACK && row->OperStatus == IfOperStatusUp) {
                stats.rxBytes += row->InOctets;
                stats.txBytes += row->OutOctets;
            }
        }
        FreeMibTable(pIfTable);
    }
#else
    std::ifstream f("/proc/net/dev");
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("lo:") == std::string::npos && line.find(":") != std::string::npos) {
                size_t colonPos = line.find(":");
                std::string data = line.substr(colonPos + 1);
                std::istringstream iss(data);
                ULONG64 rxBytes, rxPackets, rxErrs, rxDrop, rxFifo, rxFrame, rxComp, rxMulticast;
                ULONG64 txBytes;
                if (iss >> rxBytes >> rxPackets >> rxErrs >> rxDrop >> rxFifo >> rxFrame >> rxComp >> rxMulticast >> txBytes) {
                    stats.rxBytes += rxBytes;
                    stats.txBytes += txBytes;
                }
            }
        }
    }
    if (stats.rxBytes == 0 && stats.txBytes == 0) {
        // Fallback for Android/Termux where /proc/net/dev is restricted
        // Use ifconfig to get stats
        FILE* pipe = popen("ifconfig 2>/dev/null", "r");
        if (pipe) {
            char buffer[512];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                std::string line(buffer);
                if (line.find("bytes") != std::string::npos) {
                    size_t pos = line.find("RX bytes");
                    if (pos == std::string::npos) pos = line.find("RX packets");
                    if (pos != std::string::npos) {
                        // Look for the number after "bytes" or "bytes:"
                        size_t bpos = line.find("bytes", pos);
                        if (bpos != std::string::npos) {
                            std::string s = line.substr(bpos + 5);
                            if (!s.empty() && s[0] == ':') s = s.substr(1);
                            std::istringstream iss(s);
                            ULONG64 val;
                            if (iss >> val) stats.rxBytes += val;
                        }
                    }
                    pos = line.find("TX bytes");
                    if (pos == std::string::npos) pos = line.find("TX packets");
                    if (pos != std::string::npos) {
                        size_t bpos = line.find("bytes", pos);
                        if (bpos != std::string::npos) {
                            std::string s = line.substr(bpos + 5);
                            if (!s.empty() && s[0] == ':') s = s.substr(1);
                            std::istringstream iss(s);
                            ULONG64 val;
                            if (iss >> val) stats.txBytes += val;
                        }
                    }
                }
            }
            pclose(pipe);
        }
    }
#endif
    return stats;
}

std::string formatBytes(ULONG64 bytes) {
    double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    double mb = bytes / (1024.0 * 1024.0);
    double kb = bytes / 1024.0;
    std::ostringstream out;
    out << std::fixed << std::setprecision(2);
    if (gb >= 1.0) { out << gb << " GB"; return out.str(); }
    if (mb >= 1.0) { out << mb << " MB"; return out.str(); }
    if (kb >= 1.0) { out << kb << " KB"; return out.str(); }
    out << bytes << " B";
    return out.str();
}

PingResult getPing(const char* ipString, int count = 4) {
    PingResult pr = {-1, 100};
#ifdef _WIN32
    HANDLE hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) return pr;

    unsigned long ipaddr = INADDR_NONE;
    inet_pton(AF_INET, ipString, &ipaddr);
    if (ipaddr == INADDR_NONE || ipaddr == 0) {
        IcmpCloseHandle(hIcmpFile);
        return pr;
    }

    char SendData[32] = "NetfetchPingData";
    DWORD ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData) + 8;
    LPVOID ReplyBuffer = malloc(ReplySize);

    int successCount = 0;
    int totalLatency = 0;

    for (int i = 0; i < count; i++) {
        DWORD dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof(SendData), NULL, ReplyBuffer, ReplySize, 1000);
        if (dwRetVal != 0) {
            PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
            if (pEchoReply->Status == IP_SUCCESS) {
                int lat = pEchoReply->RoundTripTime;
                if (lat == 0) lat = 1;
                totalLatency += lat;
                successCount++;
            }
        }
    }

    free(ReplyBuffer);
    IcmpCloseHandle(hIcmpFile);

    if (successCount > 0) {
        pr.latency = totalLatency / successCount;
        pr.lossPercent = ((count - successCount) * 100) / count;
    }
    return pr;
#else
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ipString, &addr.sin_addr) <= 0) return pr;

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    bool is_raw = false;
    if (sockfd < 0) {
        sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        is_raw = true;
    }

    if (sockfd >= 0) {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        int successCount = 0;
        int totalLatency = 0;
        int id = getpid() & 0xFFFF;

        for (int i = 0; i < count; i++) {
            struct icmphdr icmp_hdr;
            memset(&icmp_hdr, 0, sizeof(icmp_hdr));
            icmp_hdr.type = ICMP_ECHO;
            icmp_hdr.code = 0;
            icmp_hdr.un.echo.id = htons(id);
            icmp_hdr.un.echo.sequence = htons(i + 1);
            icmp_hdr.checksum = icmp_checksum((unsigned short*)&icmp_hdr, sizeof(icmp_hdr));

            struct timeval start, end;
            gettimeofday(&start, NULL);

            if (sendto(sockfd, &icmp_hdr, sizeof(icmp_hdr), 0, (struct sockaddr*)&addr, sizeof(addr)) <= 0) continue;

            char buffer[1024];
            struct sockaddr_in recv_addr;
            socklen_t addr_len = sizeof(recv_addr);
            
            while (true) {
                int bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&recv_addr, &addr_len);
                if (bytes <= 0) break;
                
                gettimeofday(&end, NULL);
                struct icmphdr *recv_hdr = NULL;
                if (is_raw) {
                    int ip_hdr_len = (buffer[0] & 0x0F) * 4;
                    if (bytes >= ip_hdr_len + (int)sizeof(struct icmphdr)) {
                        recv_hdr = (struct icmphdr *)(buffer + ip_hdr_len);
                    }
                } else {
                    if (bytes >= (int)sizeof(struct icmphdr)) {
                        recv_hdr = (struct icmphdr *)buffer;
                    }
                }
                
                if (recv_hdr && (recv_hdr->type == ICMP_ECHOREPLY || recv_hdr->type == 0) && recv_hdr->un.echo.id == htons(id) && recv_hdr->un.echo.sequence == htons(i + 1)) {
                    int lat = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
                    if (lat == 0) lat = 1;
                    totalLatency += lat;
                    successCount++;
                    break;
                }
            }
        }
        close(sockfd);

        if (successCount > 0) {
            pr.latency = totalLatency / successCount;
            pr.lossPercent = ((count - successCount) * 100) / count;
            return pr;
        }
    }

    // Fallback to ping command if sockets fail
    std::string cmd = "ping -c " + std::to_string(count) + " -W 1 " + std::string(ipString) + " 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (p) {
        char buf[256];
        while (fgets(buf, sizeof(buf), p)) {
            std::string line(buf);
            if (line.find("rtt min/avg/max/mdev") != std::string::npos) {
                size_t eq = line.find("=");
                if (eq != std::string::npos) {
                    size_t s1 = line.find("/", eq);
                    size_t s2 = line.find("/", s1 + 1);
                    if (s1 != std::string::npos && s2 != std::string::npos) {
                        std::string avg = line.substr(s1 + 1, s2 - s1 - 1);
                        pr.latency = (int)atof(avg.c_str());
                    }
                }
            }
            if (line.find("packet loss") != std::string::npos) {
                size_t percent = line.find("%");
                if (percent != std::string::npos) {
                    size_t space = line.find_last_of(" ", percent);
                    if (space != std::string::npos) {
                        std::string loss = line.substr(space + 1, percent - space - 1);
                        pr.lossPercent = atoi(loss.c_str());
                    }
                }
            }
        }
        pclose(p);
    }
    return pr;
#endif
}

struct AdapterInfo {
    std::string name;
    std::string description;
    std::string ipv4;
    std::string ipv6;
    std::string gateway;
    std::string dns;
    std::string mac;
    std::string status;
    std::string mtu;
    std::string speed;
    std::string signal;
    bool dhcpEnabled;
};

AdapterInfo getRealAdapterInfo() {
    AdapterInfo info;
    info.status = "DOWN";
    info.ipv4 = "N/A";
    info.ipv6 = "N/A";
    info.gateway = "N/A";
    info.dns = "N/A";
    info.mac = "N/A";
    info.name = "Unknown";
    info.description = "Unknown";
    info.mtu = "N/A";
    info.speed = "Unknown";
    info.signal = "N/A";
    info.dhcpEnabled = false;

#ifdef _WIN32
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
    if (!pAddresses) return info;

    DWORD dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        dwRetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen);
    }

    if (dwRetVal == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurr = pAddresses;
        while (pCurr) {
            if (pCurr->OperStatus == IfOperStatusUp && pCurr->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                std::wstring wName(pCurr->FriendlyName);
                info.name = std::string(wName.begin(), wName.end());
                
                std::wstring wDesc(pCurr->Description);
                info.description = std::string(wDesc.begin(), wDesc.end());
                
                info.status = "UP";
                info.mtu = std::to_string(pCurr->Mtu);
                info.dhcpEnabled = (pCurr->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;

                if (pCurr->TransmitLinkSpeed > 0) {
                    double mbps = pCurr->TransmitLinkSpeed / 1000000.0;
                    if (mbps >= 1000.0) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%.1f Gbps", mbps / 1000.0);
                        info.speed = buf;
                    } else {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%.0f Mbps", mbps);
                        info.speed = buf;
                    }
                }

                if (pCurr->PhysicalAddressLength != 0) {
                    char macStr[32];
                    snprintf(macStr, sizeof(macStr), "%02X-%02X-%02X-%02X-%02X-%02X",
                        pCurr->PhysicalAddress[0], pCurr->PhysicalAddress[1], pCurr->PhysicalAddress[2],
                        pCurr->PhysicalAddress[3], pCurr->PhysicalAddress[4], pCurr->PhysicalAddress[5]);
                    info.mac = macStr;
                }

                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress;
                while (pUnicast) {
                    sockaddr* sa = pUnicast->Address.lpSockaddr;
                    char buf[INET6_ADDRSTRLEN];
                    if (sa->sa_family == AF_INET) {
                        inet_ntop(AF_INET, &(((sockaddr_in*)sa)->sin_addr), buf, sizeof(buf));
                        if (info.ipv4 == "N/A") info.ipv4 = buf;
                    } else if (sa->sa_family == AF_INET6) {
                        inet_ntop(AF_INET6, &(((sockaddr_in6*)sa)->sin6_addr), buf, sizeof(buf));
                        if (info.ipv6 == "N/A") info.ipv6 = "Enabled"; 
                    }
                    pUnicast = pUnicast->Next;
                }

                PIP_ADAPTER_GATEWAY_ADDRESS_LH pGateway = pCurr->FirstGatewayAddress;
                if (pGateway) {
                    sockaddr* sa = pGateway->Address.lpSockaddr;
                    char buf[INET6_ADDRSTRLEN];
                    if (sa->sa_family == AF_INET) {
                        inet_ntop(AF_INET, &(((sockaddr_in*)sa)->sin_addr), buf, sizeof(buf));
                        info.gateway = buf;
                    }
                }

                PIP_ADAPTER_DNS_SERVER_ADDRESS pDns = pCurr->FirstDnsServerAddress;
                if (pDns) {
                    sockaddr* sa = pDns->Address.lpSockaddr;
                    char buf[INET6_ADDRSTRLEN];
                    if (sa->sa_family == AF_INET) {
                        inet_ntop(AF_INET, &(((sockaddr_in*)sa)->sin_addr), buf, sizeof(buf));
                        info.dns = buf;
                    }
                }

                break; 
            }
            pCurr = pCurr->Next;
        }
    }
    free(pAddresses);
#else
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            if (!(ifa->ifa_flags & IFF_UP)) continue;

            info.name = ifa->ifa_name;
            info.description = ifa->ifa_name;
            info.status = "UP";

            int family = ifa->ifa_addr->sa_family;
            char host[NI_MAXHOST];
            if (family == AF_INET || family == AF_INET6) {
                if (getnameinfo(ifa->ifa_addr,
                        (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                        host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
                    if (family == AF_INET && info.ipv4 == "N/A") info.ipv4 = host;
                    else if (family == AF_INET6 && info.ipv6 == "N/A") info.ipv6 = "Enabled";
                }
            }
        }
        freeifaddrs(ifaddr);
    }

#ifndef _WIN32
    // On Android, if we couldn't find an interface with an IP, try to find ANY UP interface
    if (info.name == "Unknown") {
        FILE* pipe = popen("ifconfig 2>/dev/null | grep 'Link' | awk '{print $1}'", "r");
        if (!pipe) pipe = popen("ifconfig 2>/dev/null | grep 'flags' | awk -F':' '{print $1}'", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                std::string name(buffer);
                name.erase(std::remove(name.begin(), name.end(), '\n'), name.end());
                if (!name.empty()) {
                    info.name = name;
                    info.status = "UP";
                }
            }
            pclose(pipe);
        }
    }
#endif

    if (info.name != "Unknown") {
        std::string macPath = "/sys/class/net/" + info.name + "/address";
        std::ifstream f(macPath);
        if (f.is_open()) {
            std::string mac;
            f >> mac;
            if (!mac.empty()) info.mac = mac;
        }

        std::string speedPath = "/sys/class/net/" + info.name + "/speed";
        std::ifstream sf(speedPath);
        if (sf.is_open()) {
            int speed;
            if (sf >> speed && speed > 0) {
                info.speed = std::to_string(speed) + " Mbps";
            }
        }
        
        if (info.speed == "Unknown") {
            std::string usbSpeedPath = "/sys/class/net/" + info.name + "/device/../speed";
            std::ifstream usf(usbSpeedPath);
            if (usf.is_open()) {
                int uspeed;
                if (usf >> uspeed && uspeed > 0) {
                    info.speed = std::to_string(uspeed) + " Mbps (USB)";
                }
            }
        }

        if (info.speed == "Unknown") {
            std::string scmd = "ethtool " + info.name + " 2>/dev/null | grep Speed | awk '{print $2}'";
            FILE* spipe = popen(scmd.c_str(), "r");
            if (spipe) {
                char sbuf[64];
                if (fgets(sbuf, sizeof(sbuf), spipe)) {
                    std::string s(sbuf);
                    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
                    if (!s.empty() && s.find("Unknown") == std::string::npos) info.speed = s;
                }
                pclose(spipe);
            }
        }
        
        if (info.mtu == "N/A") {
            std::string mcmd = "ifconfig " + info.name + " 2>/dev/null";
            FILE* mpipe = popen(mcmd.c_str(), "r");
            if (mpipe) {
                char mbuf[512];
                while (fgets(mbuf, sizeof(mbuf), mpipe)) {
                    std::string s(mbuf);
                    size_t pos = s.find("mtu ");
                    if (pos != std::string::npos) {
                        std::string m = s.substr(pos + 4);
                        std::istringstream iss(m);
                        std::string val;
                        if (iss >> val) info.mtu = val;
                        break;
                    }
                }
                pclose(mpipe);
            }
        }

        if (info.mac == "N/A") {
            std::string maccmd = "ifconfig " + info.name + " 2>/dev/null";
            FILE* macpipe = popen(maccmd.c_str(), "r");
            if (macpipe) {
                char macbuf[512];
                while (fgets(macbuf, sizeof(macbuf), macpipe)) {
                    std::string s(macbuf);
                    size_t pos = s.find("ether ");
                    if (pos == std::string::npos) pos = s.find("HWaddr ");
                    if (pos != std::string::npos) {
                        size_t offset = (s.find("ether ") != std::string::npos) ? 6 : 7;
                        std::string m = s.substr(pos + offset);
                        std::istringstream iss(m);
                        std::string val;
                        if (iss >> val) info.mac = val;
                        break;
                    }
                }
                pclose(macpipe);
            }
        }
    }

#ifndef _WIN32
    // Try to get gateway and MTU from /proc/net/route (more reliable on Android)
    std::ifstream routeFile("/proc/net/route");
    if (routeFile.is_open()) {
        std::string line;
        std::getline(routeFile, line); // Skip header
        while (std::getline(routeFile, line)) {
            std::istringstream iss(line);
            std::string iface, dest, gatewayStr, flags, refcnt, use, metric, mask, mtu;
            if (iss >> iface >> dest >> gatewayStr >> flags >> refcnt >> use >> metric >> mask >> mtu) {
                if (dest == "00000000") { // Default route
                    if (info.gateway == "N/A") {
                        unsigned int addr;
                        std::stringstream ss;
                        ss << std::hex << gatewayStr;
                        ss >> addr;
                        struct in_addr gaddr;
                        gaddr.s_addr = addr;
                        char* ip = inet_ntoa(gaddr);
                        if (ip && strcmp(ip, "0.0.0.0") != 0) info.gateway = ip;
                    }
                    if (info.mtu == "N/A") info.mtu = mtu;
                }
            }
        }
    }
#endif

    FILE* pipe = popen("ip route 2>/dev/null | grep default | awk '{print $3}'", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            std::string gw(buffer);
            gw.erase(std::remove(gw.begin(), gw.end(), '\n'), gw.end());
            if (!gw.empty()) info.gateway = gw;
        }
        pclose(pipe);
    }

                    if (info.gateway == "N/A") {
                        FILE* gp = popen("getprop | grep -E 'gateway|default_gw' | head -n 1 | awk -F': ' '{print $2}'", "r");
                        if (gp) {
                            char gbuf[128];
                            if (fgets(gbuf, sizeof(gbuf), gp)) {
                                std::string g(gbuf);
                                g.erase(std::remove(g.begin(), g.end(), '['), g.end());
                                g.erase(std::remove(g.begin(), g.end(), ']'), g.end());
                                g.erase(std::remove(g.begin(), g.end(), '\n'), g.end());
                                if (!g.empty()) info.gateway = g;
                            }
                            pclose(gp);
                        }
                    }

    std::ifstream rf("/etc/resolv.conf");
    if (rf.is_open()) {
        std::string line;
        while (std::getline(rf, line)) {
            if (line.find("nameserver ") == 0) {
                std::string dns = line.substr(11);
                dns.erase(std::remove(dns.begin(), dns.end(), ' '), dns.end());
                info.dns = dns;
                break;
            }
        }
    }

    if (info.dns == "N/A") {
        FILE* dp = popen("getprop | grep -E '\\.dns[0-9]*\\]' | head -n 1 | awk -F': ' '{print $2}'", "r");
        if (dp) {
            char dbuf[128];
            if (fgets(dbuf, sizeof(dbuf), dp)) {
                std::string d(dbuf);
                d.erase(std::remove(d.begin(), d.end(), '\n'), d.end());
                d.erase(std::remove(d.begin(), d.end(), '['), d.end());
                d.erase(std::remove(d.begin(), d.end(), ']'), d.end());
                if (!d.empty()) info.dns = d;
            }
            pclose(dp);
        }
    }

#ifndef _WIN32
    // Get WiFi signal strength on Android
    std::ifstream wifiFile("/proc/net/wireless");
    if (wifiFile.is_open()) {
        std::string line;
        std::getline(wifiFile, line); // Skip header
        std::getline(wifiFile, line); // Skip header
        while (std::getline(wifiFile, line)) {
            if (line.find(":") != std::string::npos) {
                size_t colon = line.find(":");
                std::string data = line.substr(colon + 1);
                std::istringstream iss(data);
                std::string status;
                double link, level, noise;
                if (iss >> status >> link >> level >> noise) {
                    // level is in dBm
                    info.signal = std::to_string((int)level) + " dBm";
                    break;
                }
            }
        }
    }

    if (info.signal == "N/A") {
        FILE* sp = popen("cmd wifi status 2>/dev/null | grep RSSI | awk '{print $3}'", "r");
        if (sp) {
            char sbuf[64];
            if (fgets(sbuf, sizeof(sbuf), sp)) {
                std::string s(sbuf);
                s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
                if (!s.empty()) {
                    if (s.find("-") == std::string::npos) s = "-" + s;
                    info.signal = s + " dBm";
                }
            }
            pclose(sp);
        }
    }
#endif

    FILE* dpipe = popen("ip addr show 2>/dev/null | grep -q \"dynamic\" && echo yes || echo no", "r");
    if (dpipe) {
        char dbuf[16];
        if (fgets(dbuf, sizeof(dbuf), dpipe)) {
            std::string d(dbuf);
            if (d.find("yes") != std::string::npos) info.dhcpEnabled = true;
        }
        pclose(dpipe);
    }
#endif
    return info;
}

int getTerminalWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80;
#endif
}

struct InfoLine {
    std::string label;
    std::string value;
    bool isHeader = false;
    bool isIndented = false;
    bool isSeparator = false;
    bool isColorBlock = false;
};

void printNeofetch(const std::vector<std::string>& ascii, const std::vector<InfoLine>& info, const std::string& asciiColor = CYAN) {
    int termWidth = getTerminalWidth();
    int asciiMaxWidth = 0;
    for (const auto& s : ascii) asciiMaxWidth = std::max(asciiMaxWidth, (int)s.length());
    
    // Fixed threshold for vertical layout (safer for mobile)
    bool vertical = (termWidth < 100);

    if (vertical) {
        std::cout << "\n";
        for (const auto& line : ascii) {
            std::cout << asciiColor << BOLD << line << RESET << "\n";
        }
        std::cout << "\n";
        for (const auto& line : info) {
            if (line.isSeparator) {
                std::cout << GRAY << "────────────────────────────────────────────" << RESET << "\n";
            } else if (line.isHeader) {
                std::cout << CYAN << line.label << RESET << "\n";
            } else if (line.isColorBlock) {
                std::cout << line.value << "\n";
            } else if (line.label.empty() && line.value.empty()) {
                std::cout << "\n";
            } else {
                int labelWidth = line.isIndented ? 11 : 13;
                std::string prefix = line.isIndented ? "  " : "";
                std::cout << prefix << CYAN << std::left << std::setw(labelWidth) << line.label 
                          << RESET << line.value << "\n";
            }
        }
        return;
    }

    size_t maxLines = std::max(ascii.size(), info.size());
    int fixedAsciiWidth = std::max(asciiMaxWidth + 4, 35); 

    std::cout << "\n";

    for (size_t i = 0; i < maxLines; ++i) {
        if (i < ascii.size()) {
            int padding = fixedAsciiWidth - ascii[i].length();
            if (padding < 0) padding = 0;
            std::cout << asciiColor << BOLD << ascii[i] << std::string(padding, ' ') << RESET;
        } else {
            std::cout << std::string(fixedAsciiWidth, ' ');
        }

        if (i < info.size()) {
            const auto& line = info[i];
            
            if (line.isSeparator) {
                std::cout << GRAY << "────────────────────────────────────────────" << RESET;
            } else if (line.isHeader) {
                std::cout << CYAN << line.label << RESET;
            } else if (line.isColorBlock) {
                std::cout << line.value;
            } else if (line.label.empty() && line.value.empty()) {
            } else {
                int labelWidth = line.isIndented ? 11 : 13;
                std::string prefix = line.isIndented ? "  " : "";
                std::cout << prefix << CYAN << std::left << std::setw(labelWidth) << line.label 
                          << RESET << line.value;
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    enableANSI();
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    srand((unsigned int)time(NULL));

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    bool compactMode = false;
    for(int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--compact") {
            compactMode = true;
        } else if (std::string(argv[i]) == "--install") {
#ifdef _WIN32
            char path[MAX_PATH];
            GetModuleFileNameA(NULL, path, MAX_PATH);
            
            char winDir[MAX_PATH];
            GetWindowsDirectoryA(winDir, MAX_PATH);
            std::string target = std::string(winDir) + "\\netfetch.exe";
            
            if (CopyFileA(path, target.c_str(), FALSE)) {
                std::cout << "\n" << GREEN << "Success! netfetch was installed globally." << RESET << "\n";
                std::cout << "You can now open any terminal and just type 'netfetch'.\n\n";
            } else {
                std::cout << "\n" << RED << "Installation failed (Error: " << GetLastError() << ")." << RESET << "\n";
                std::cout << "Make sure you opened the terminal as Administrator!\n\n";
            }
            WSACleanup();
            return 0;
#else
            std::string cmd = "sudo cp " + std::string(argv[0]) + " /usr/local/bin/netfetch";
            if (system(cmd.c_str()) == 0) {
                std::cout << "\n" << GREEN << "Success! netfetch was installed to /usr/local/bin." << RESET << "\n";
                std::cout << "You can now open any terminal and just type 'netfetch'.\n\n";
            } else {
                std::cout << "\n" << RED << "Installation failed." << RESET << "\n";
                std::cout << "Make sure you have sudo privileges!\n\n";
            }
            return 0;
#endif
        }
    }

    NetworkStats stats = getTotalNetworkUsage();
    AdapterInfo adapter = getRealAdapterInfo();
    
    std::string publicIp = fetchUrl("http://ipinfo.io/ip");
    std::string asnInfo = fetchUrl("http://ipinfo.io/org");
    
    std::string pingTarget = (adapter.dns != "N/A") ? adapter.dns : "1.1.1.1";
    PingResult dnsPing = getPing(pingTarget.c_str());
    PingResult netPing = getPing("8.8.8.8");

    auto formatLat = [](int lat) {
        if (lat >= 0) return bar(lat) + " " + WHITE + std::to_string(lat) + "ms" + RESET;
        return bar(100) + " " + RED + "TIMEOUT" + RESET;
    };

    std::vector<std::string> asciiWavy = {
        R"(                                ..,   )",
        R"(                    ....,,:;+ccllll   )",
        R"(      ...,,+:;  cllllllllllllllllll   )",
        R"(,cclllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(                                      )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(llllllllllllll  lllllllllllllllllll   )",
        R"(`'ccllllllllll  lllllllllllllllllll   )",
        R"(       `' \\*::  :ccllllllllllllllll  )",
        R"(                       ````''*::cll   )",
        R"(                                 ``   )"
    };

    std::vector<std::string> asciiSquare = {
        R"(                                      )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(                                      )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(    llllllllllllll  llllllllllllll    )",
        R"(                                      )",
        R"(                                      )"
    };

    std::vector<std::string> asciiUbuntu = {
        R"(                             )",
        R"(         .-/+oossssoo+/-.    )",
        R"(     `:+ssssssssssssssssss+:`)",
        R"(   -+ssssssssssssssssssyyssss+-)",
        R"( .ossssssssssssssssssdMMMNysssso.)",
        R"(/ssssssssssshdmmNNmmyNMMMMhssssss/)",
        R"(+ssssssssshmydMMMMMMMNddddyssssssss+)",
        R"(/sssssssshNMMMyhhyyyyhmNMMMNhssssssss/)",
        R"(.ssssssssdMMMNhsssssssssshNMMMdssssssss.)",
        R"(+sssshhhyNMMNyssssssssssssyNMMMysssssss+)",
        R"(ossyNMMMNyMMhsssssssssssssshmmmhssssssso)",
        R"(ossyNMMMNyMMhsssssssssssssshmmmhssssssso)",
        R"(+sssshhhyNMMNyssssssssssssyNMMMysssssss+)",
        R"(.ssssssssdMMMNhsssssssssshNMMMdssssssss.)",
        R"(/sssssssshNMMMyhhyyyyhdNMMMNhssssssss/)",
        R"(+sssssssssdmydMMMMMMMMddddyssssssss+)",
        R"(/ssssssssssshdmmNNmmyNMMMMhssssss/)",
        R"( .ossssssssssssssssssdMMMNysssso.)",
        R"(   -+sssssssssssssssssyyyssss+-)",
        R"(     `:+ssssssssssssssssss+:`)"
    };

    std::vector<std::string> asciiDebian = {
        R"(       _,met$$$$$gg.      )",
        R"(    ,g$$$$$$$$$$$$$$$P.   )",
        R"(  ,g$$P"     """Y$$.".    )",
        R"( ,$$P'              `$$$. )",
        R"(',$$P       ,ggs.     `$$b:)",
        R"(`d$$'     ,$P"'   .    $$$)",
        R"( $$P      d$'     ,    $$P)",
        R"( $$:      $$.   -    ,d$$')",
        R"( $$;      Y$b._   _,d$P'  )",
        R"( Y$$.    `.`"Y$$$$P"'     )",
        R"( `$$b      "-.__          )",
        R"(  `Y$$                    )",
        R"(   `Y$$.                  )",
        R"(     `$$b.                )",
        R"(       `Y$$b.             )",
        R"(          `"Y$b._         )",
        R"(              `"""        )",
        R"(                          )",
        R"(                          )",
        R"(                          )"
    };

    std::vector<std::string> asciiFedora = {
        R"(          /:-------------:\          )",
        R"(       :-------------------::        )",
        R"(     :-----------/shhOHbmp---:\      )",
        R"(   /-----------omMMMNNNMMD  ---:     )",
        R"(  :-----------sMMMMNMNMP.    ---:    )",
        R"( :-----------:MMMdP-------    ---\   )",
        R"(,------------:MMMd------    ---:   )",
        R"(:------------:MMMd-------    .---: )",
        R"(:----    oNMMMMMMMMMNho     .----: )",
        R"(:--     .+shhhNNMMMMNhs   .------: )",
        R"(:---       :MMMd------:  .-------: )",
        R"(:----.     :MMMd-------.--------:  )",
        R"( \-----.    ---:       .---------  )",
        R"(  \------.   --.    .----------/   )",
        R"(   :--------.      .----------:    )",
        R"(     \---------..----------:/      )",
        R"(       \------------------/        )",
        R"(          \-------------/          )",
        R"(                                   )",
        R"(                                   )"
    };

    std::vector<std::string> asciiAlma = {
        R"(         .:oocc:oocc:.         )",
        R"(       ::               ::     )",
        R"(    ::                     ::  )",
        R"(   :                           :)",
        R"(  :   :ooooooooooooooooooooooo: )",
        R"(  :   :ooooooooooooooooooooooo: )",
        R"(   :   :                      : )",
        R"(    ::  :                    :  )",
        R"(       :: :                :    )",
        R"(         :: :            :      )",
        R"(            :: :       :        )",
        R"(               :: :  :          )",
        R"(                  ::            )",
        R"(                                )",
        R"(                                )",
        R"(                                )",
        R"(                                )",
        R"(                                )",
        R"(                                )",
        R"(                                )"
    };

    std::vector<std::string> asciiKali = {
        R"(..............                    )",
        R"(            ..,;:ccc,.            )",
        R"(          ......''';lxO.          )",
        R"(.....''''..........,:ld;          )",
        R"(           .';;;:::;,,.x,         )",
        R"(      ..'''.            0Nd       )",
        R"(    .....                kBa      )",
        R"(   ...                   .o;      )",
        R"(  ..                      ..      )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )",
        R"(                                  )"
    };

    std::vector<std::string> asciiLinux = {
        R"(        .---.         )",
        R"(       /     \        )",
        R"(       \.@-@./        )",
        R"(       /`\_/`\        )",
        R"(      //  _  \\       )",
        R"(     | \     )|_      )",
        R"(    /`\_`>  <_/ \     )",
        R"(    \__/'---'\__/     )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )",
        R"(                      )"
    };

    std::vector<std::string> asciiAndroid = {
        R"(        .        .        )",
        R"(         \      /         )",
        R"(        ..------..        )",
        R"(       /          \       )",
        R"(      |    o  o    |      )",
        R"(      |            |      )",
        R"(  ----------------------  )",
        R"(  |  |              |  |  )",
        R"(  |  |              |  |  )",
        R"(  |  |              |  |  )",
        R"(  |  |              |  |  )",
        R"(  |  |              |  |  )",
        R"(  ----------------------  )",
        R"(      |    ||    |        )",
        R"(      |    ||    |        )",
        R"(      '----''----'        )",
        R"(                          )",
        R"(                          )",
        R"(                          )",
        R"(                          )"
    };

    std::string osName = getOSVersion();
    std::string osNameLower = osName;
    std::transform(osNameLower.begin(), osNameLower.end(), osNameLower.begin(), ::tolower);

    bool isWin11 = (osName.find("Windows 11") != std::string::npos);
    bool useSquareLogo = isWin11;
    
    std::vector<std::string> ascii = useSquareLogo ? asciiSquare : asciiWavy;
    std::string asciiColor = useSquareLogo ? RED : CYAN;

    if (osNameLower.find("ubuntu") != std::string::npos) {
        ascii = asciiUbuntu;
        asciiColor = RED; 
    } else if (osNameLower.find("debian") != std::string::npos) {
        ascii = asciiDebian;
        asciiColor = RED;
    } else if (osNameLower.find("fedora") != std::string::npos) {
        ascii = asciiFedora;
        asciiColor = CYAN;
    } else if (osNameLower.find("alma") != std::string::npos) {
        ascii = asciiAlma;
        asciiColor = CYAN; 
    } else if (osNameLower.find("kali") != std::string::npos) {
        ascii = asciiKali;
        asciiColor = RED; 
    } else if (osNameLower.find("linux") != std::string::npos) {
        ascii = asciiLinux;
        asciiColor = WHITE;
    } else if (osNameLower.find("android") != std::string::npos) {
        ascii = asciiAndroid;
        asciiColor = GREEN;
    }

    auto formatVal = [](const std::string& val) {
        if (val == "N/A" || val == "Unknown" || val.empty()) {
            return WHITE + val + " " + RED + "(!)" + RESET;
        }
        return WHITE + val + RESET;
    };

    std::vector<InfoLine> info;
    info.push_back({"OS:", formatVal(osName)});
    
#ifndef _WIN32
    if (osName.find("Android") != std::string::npos) {
        FILE* p = popen("getprop ro.product.model", "r");
        if (p) {
            char buf[64];
            if (fgets(buf, sizeof(buf), p)) {
                std::string model(buf);
                model.erase(std::remove(model.begin(), model.end(), '\n'), model.end());
                if (!model.empty()) info.push_back({"Device:", formatVal(model)});
            }
            pclose(p);
        }
    }
#endif

    info.push_back({"Interface:", formatVal(adapter.name)});
    info.push_back({"Desc:", formatVal(adapter.description)});
    info.push_back({"Status:", colorizeStatus(adapter.status)});

    info.push_back({"", ""});
    info.push_back({"Data Rx:", WHITE + formatBytes(stats.rxBytes) + RESET});
    info.push_back({"Data Tx:", WHITE + formatBytes(stats.txBytes) + RESET});
    info.push_back({"Speed:", formatVal(adapter.speed)});
    info.push_back({"MTU:", formatVal(adapter.mtu)});
    
    if (adapter.signal != "N/A") {
        int dbm = atoi(adapter.signal.c_str());
        info.push_back({"Signal:", signalBar(dbm) + " " + WHITE + adapter.signal + RESET});
    }

    if (!compactMode) {
        info.push_back({"", ""});
        info.push_back({"Local IP:", formatVal(adapter.ipv4)});
        info.push_back({"Public IP:", formatVal(publicIp)});
        info.push_back({"ASN:", formatVal(asnInfo)});
        
        info.push_back({"", ""});
        info.push_back({"Gateway:", formatVal(adapter.gateway)});
        info.push_back({"IPv6:", colorizeStatus(adapter.ipv6)});
        info.push_back({"DNS:", formatVal(adapter.dns)});
    }

    if (!compactMode) {
        info.push_back({"", ""});
        info.push_back({"MAC:", formatVal(adapter.mac)});
        info.push_back({"DHCP:", colorizeStatus(adapter.dhcpEnabled ? "Yes" : "No")});
    }

    info.push_back({"", ""});
    
    info.push_back({"DNS Latency:", formatLat(dnsPing.latency)});
    info.push_back({"Net Latency:", formatLat(netPing.latency)});
    info.push_back({"Loss:", lossBar(netPing.lossPercent) + " " + WHITE + std::to_string(netPing.lossPercent) + "%" + RESET});

    info.push_back({"", ""});
    
    std::string colorBlocks1 = "\033[40m   \033[41m   \033[42m   \033[43m   \033[44m   \033[45m   \033[46m   \033[47m   \033[0m";
    std::string colorBlocks2 = "\033[100m   \033[101m   \033[102m   \033[103m   \033[104m   \033[105m   \033[106m   \033[107m   \033[0m";
    
    info.push_back({"", colorBlocks1, false, false, false, true});
    info.push_back({"", colorBlocks2, false, false, false, true});

    printNeofetch(ascii, info, asciiColor);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
