#include "tutorial/test_memory.hh"
#include "base/trace.hh"
#include "debug/TestMemory.hh"

namespace gem5
{

TestMemory::TestMemory(const TestMemoryParams &params):
  SimObject(params),
  instPort(params.name + ".inst_port", this),
  dataPort(params.name + ".data_port", this),
  memPort(params.name + ".mem_port", this),
  blocked(false)
{
}

Port &
TestMemory::getPort(const std::string &if_name, PortID idx)
{
  panic_if(idx != InvalidPortID, "this object doesn't support vector ports");

  if(if_name == "mem_port")
    return memPort;
  else if(if_name == "inst_port")
    return instPort;
  else if(if_name == "data_port")
    return dataPort;
  else
    return SimObject::getPort(if_name, idx);
}

void 
TestMemory::CPUSidePort::sendPacket(PacketPtr pkt)
{
  DPRINTF(TestMemory, "CPUSidePort::sendPacket\n");
  //the response cannot be initiated if there port is blocked
  panic_if(blockedPacket != nullptr, "should never try to send if blocked");

  if(!sendTimingResp(pkt))
    blockedPacket = pkt; 
}

AddrRangeList 
TestMemory::CPUSidePort::getAddrRanges() const
{
  return owner->getAddrRanges(); 
}

void
TestMemory::CPUSidePort::recvFunctional(PacketPtr pkt)
{
  return owner->handleFunctional(pkt);
}

bool 
TestMemory::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
  if(!owner->handleRequest(pkt))
  {
    needRetry = true;
    return false;
  }
  else
  {
    return true;
  }
}

void 
TestMemory::CPUSidePort::recvRespRetry()
{
  assert(blockedPacket != nullptr);
  PacketPtr pkt = blockedPacket;
  blockedPacket = nullptr;

  sendPacket(pkt);
}

void 
TestMemory::CPUSidePort::trySendRetry()
{
  if(needRetry && blockedPacket != nullptr)
  {
    needRetry = false;
    sendRetryReq();
  }
}

void 
TestMemory::MemSidePort::sendPacket(PacketPtr pkt)
{
  panic_if(blockedPacket != nullptr, "should never try to send if blocked!!"); 

  if(!sendTimingReq(pkt))
    blockedPacket = pkt;
}

bool
TestMemory::MemSidePort::recvTimingResp(PacketPtr pkt)
{
  return owner->handleResponse(pkt); 
}

void 
TestMemory::MemSidePort::recvReqRetry()
{
  assert(blockedPacket != nullptr);
  PacketPtr pkt = blockedPacket;
  blockedPacket = nullptr;

  sendPacket(pkt);
}

void 
TestMemory::MemSidePort::recvRangeChange()
{
  owner->sendRangeChange();
}

bool
TestMemory::handleRequest(PacketPtr pkt)
{
  if(blocked)
    return false;

  blocked = true;
  memPort.sendPacket(pkt);
  return true;
}

bool
TestMemory::handleResponse(PacketPtr pkt)
{
  assert(blocked);

  blocked = false;

  if(pkt->req->isInstFetch())
    instPort.sendPacket(pkt);
  else
    dataPort.sendPacket(pkt);

  instPort.trySendRetry();
  dataPort.trySendRetry();

  return true;
}

void 
TestMemory::handleFunctional(PacketPtr pkt)
{
  memPort.sendFunctional(pkt);
}

AddrRangeList 
TestMemory::getAddrRanges() const
{
  return memPort.getAddrRanges();
}

void 
TestMemory::sendRangeChange()
{
  instPort.sendRangeChange();
  dataPort.sendRangeChange();
}

}
