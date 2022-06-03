#ifndef __TEST_CACHE_H__
#define __TEST_CACHE_H__

#include <unordered_map>

#include "base/statistics.hh"
#include "mem/port.hh"
#include "params/TestCache.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

class TestCache : public ClockedObject
{
private:
  class CPUSidePort : public ResponsePort
  {
  private:
    //vector port id
    int id;

    TestCache *owner;

    bool needRetry;

    PacketPtr blockedPacket;

  public:
    CPUSidePort(const std::string &name, int id, TestCache *owner):
      ResponsePort(name, owner), id(id), owner(owner), needRetry(false),
      blockedPacket(nullptr)
    {}

    void sendPacket(PacketPtr pkt);

    AddrRangeList getAddrRanges() const override;

    void trySendRetry();

  protected:
    Tick recvAtomic(PacketPtr pkt) override
    {
      panic("recvAtomic unimpl.");
    }

    void recvFunctional(PacketPtr pkt) override; 

    bool recvTimingReq(PacketPtr pkt) override;

    void recvRespRetry() override;

  };

  class MemSidePort : public RequestPort 
  {
  private:
    TestCache *owner;
    PacketPtr blockedPacket;

  public:
    MemSidePort(const std::string &name, TestCache *owner):
      RequestPort(name, owner), owner(owner), blockedPacket(nullptr)
    {}

    void sendPacket(PacketPtr pkt);

  protected:
    bool recvTimingResp(PacketPtr pkt) override;

    void recvReqRetry() override;

    void recvRangeChange() override;
  };

  bool handleRequest(PacketPtr pkt, int port_id);
  bool handleResponse(PacketPtr pkt);
  void sendResponse(PacketPtr pkt);
  void handleFunctional(PacketPtr pkt);
  void accessTiming(PacketPtr pkt);
  bool accessFunctional(PacketPtr pkt);
  void insert(PacketPtr pkt);
  AddrRangeList getAddrRanges() const;
  void sendRangeChange() const;

  const Cycles latency;
  const unsigned blockSize;
  const unsigned capacity;
  std::vector<CPUSidePort> cpuPorts;
  MemSidePort memPort;
  bool blocked;
  PacketPtr originalPacket;
  int waitingPortId;
  Tick missTime;
  std::unordered_map<Addr, uint8_t*> cacheStore;

protected:
  /*
  struct TestCacheStats : public statistics::Group
  {
    TestCacheStats(statistics::Group *parent);
    statistics::Scalar hits;
    statistics::Scalar misses;
    statistics::Histogram missLatency;
    statistics::Formula hitRatio;
  } stats;
  */

public:
  TestCache(const TestCacheParams &params);
  
  Port &getPort(const std::string &if_name, PortID idx=InvalidPortID) override;
};

}

#endif
