#ifndef __TEST_MEMORY_H__
#define __TEST_MEMORY_H__

#include "mem/port.hh"
#include "params/TestMemory.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class TestMemory: public SimObject
{
private:
  
  class CPUSidePort : public ResponsePort
  {
  private:
    TestMemory *owner;
    bool needRetry;
    PacketPtr blockedPacket;

  public:
    CPUSidePort(const std::string &name, TestMemory *owner):
      ResponsePort(name, owner), owner(owner), 
      needRetry(false), blockedPacket(nullptr)
    {}

    void sendPacket(PacketPtr pkt);

    //rewrite the pure virtual function of ResponsePort in mem/port.hh
    AddrRangeList getAddrRanges() const override;
  
  protected:
    Tick recvAtomic(PacketPtr pkt) override
    {
      panic("recvAtomic unimplemented.");
    }

    void recvFunctional(PacketPtr pkt) override;

    //rewrite the virtual function of TimingResponseProtocol in src/mem/protcol/timing.hh 
    bool recvTimingReq(PacketPtr pkt) override;
    void recvRespRetry() override;

  public:
    void trySendRetry();
  };

  class MemSidePort : public RequestPort
  {
  private:
    TestMemory *owner; 
    PacketPtr blockedPacket;

  public:
    MemSidePort(const std::string &name, TestMemory *owner):
      RequestPort(name, owner), owner(owner),
      blockedPacket(nullptr)
    {}

    void sendPacket(PacketPtr pkt);

  protected:
    //rewrite the virtual function of TimingRequestProtocol in src/mem/protcol/timing.hh 
    bool recvTimingResp(PacketPtr pkt) override;
    void recvReqRetry() override;
    void recvRangeChange() override;
  };

  bool handleRequest(PacketPtr pkt);
  bool handleResponse(PacketPtr pkt);
  void handleFunctional(PacketPtr pkt);
  AddrRangeList getAddrRanges() const;
  void sendRangeChange();

  CPUSidePort instPort;
  CPUSidePort dataPort;

  MemSidePort memPort;

  bool blocked;

public:
  TestMemory(const TestMemoryParams &params);

  //rewrite the virtual function of SimObject in src/sim/sim_object.hh
  Port &getPort(const std::string &if_name, PortID idx=InvalidPortID) override;
};

}
#endif
