#include "tutorial/test_cache.hh"

#include "base/compiler.hh"
#include "base/random.hh"
#include "debug/TestCache.hh"
#include "debug/CacheInfo.hh"
#include "sim/system.hh"

#include <stdio.h>

namespace gem5
{

TestCache::TestCache(const TestCacheParams &params) :
  ClockedObject(params),
  latency(params.latency),
  blockSize(params.system->cacheLineSize()),
  capacity(params.size / blockSize),
  memPort(params.name + ".mem_side", this),
  blocked(false),
  originalPacket(nullptr),
  waitingPortId(-1)
  //stats(this)
{
  DPRINTF(TestCache, "TestCache::TestCache\n");

  for(int i = 0; i < params.port_cpu_side_connection_count; ++i)
  {
    cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
  }
}

Port &
TestCache::getPort(const std::string &if_name, PortID idx)
{
  DPRINTF(TestCache, "TestCache::getPort\n");
  if(if_name == "mem_side")
  {
    panic_if(idx != InvalidPortID, "Mem side of simple cache not a vector port");
    return memPort;
  } else if(if_name == "cpu_side" && idx < cpuPorts.size()) {
    return cpuPorts[idx];
  } else {
    return ClockedObject::getPort(if_name, idx);
  }
}

void 
TestCache::CPUSidePort::sendPacket(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::CPUSidePort::sendPacket\n");
  panic_if(blockedPacket != nullptr, "should never try to send if blocked!");
  if(!sendTimingResp(pkt))
  {
    DPRINTF(CacheInfo, ">>>>>>failed\n");
    blockedPacket = pkt;
  }
}

AddrRangeList
TestCache::CPUSidePort::getAddrRanges() const
{
  DPRINTF(TestCache, "TestCache::CPUSidePort::getAddrRanges\n");
  return owner->getAddrRanges(); 
}

void
TestCache::CPUSidePort::trySendRetry()
{
  DPRINTF(TestCache, "TestCache::CPUSidePort::trySendRetry\n");
  if(needRetry && blockedPacket == nullptr)
  {
    needRetry = false;
    DPRINTF(CacheInfo, ">>>>>>sending retry req\n");
    sendRetryReq();
  }
}

void 
TestCache::CPUSidePort::recvFunctional(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::CPUSidePort::recvFunctional\n");
  return owner->handleFunctional(pkt); 
}

bool 
TestCache::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::CPUSidePort::recvTimingReq\n");
  DPRINTF(CacheInfo, ">>>>>>Got request %s\n", pkt->print());
  if(blockedPacket || needRetry) 
  {
    DPRINTF(CacheInfo, ">>>>>>Request blocked\n");
    needRetry = true;
    return false;
  }

  if(!owner->handleRequest(pkt, id))
  {
    DPRINTF(CacheInfo, ">>>>>>Request failed\n");
    needRetry = true;
    return false;
  } else {
    DPRINTF(CacheInfo, ">>>>>>Request succeeded\n");
    return true;
  }
}

void 
TestCache::CPUSidePort::recvRespRetry()
{
  DPRINTF(TestCache, "TestCache::CPUSidePort::recvRespRetry\n");
  assert(blockedPacket != nullptr);

  PacketPtr pkt = blockedPacket;
  blockedPacket = nullptr;

  DPRINTF(CacheInfo, ">>>>>>retrying response pkt %s\n", pkt->print());
  sendPacket(pkt);

  trySendRetry();
}

void
TestCache::MemSidePort::sendPacket(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::MemSidePort::sendPacket\n");
  panic_if(blockedPacket != nullptr, "should never try to send if blocked!");

  if(!sendTimingReq(pkt))
  {
    blockedPacket = pkt;
  }
}

bool
TestCache::MemSidePort::recvTimingResp(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::MemSidePort::recvTimingResp\n");
  return owner->handleResponse(pkt);
}

void 
TestCache::MemSidePort::recvReqRetry()
{
  DPRINTF(TestCache, "TestCache::MemSidePort::recvReqRetry\n");
  assert(blockedPacket != nullptr);

  PacketPtr pkt = blockedPacket;
  blockedPacket = nullptr;
  
  sendPacket(pkt);
}

void 
TestCache::MemSidePort:: recvRangeChange()
{
  DPRINTF(TestCache, "TestCache::MemSidePort::recvRangeChange\n");
  owner->sendRangeChange(); 
}

bool
TestCache::handleRequest(PacketPtr pkt, int port_id)
{
  DPRINTF(TestCache, "TestCache::handleRequest\n");
  if(blocked)
    return false;

  DPRINTF(CacheInfo, ">>>>>>got request for addr %#x\n", pkt->getAddr());
  //this cache is blocked waiting for the response to this packet
  blocked = true;

  assert(waitingPortId == -1);
  waitingPortId = port_id;

  schedule(new EventFunctionWrapper([this, pkt]{accessTiming(pkt);}, 
                                    name() + "accessEvent", true), clockEdge(latency));

  return true;
}

bool
TestCache::handleResponse(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::handleResponse\n");
  assert(blocked);

  DPRINTF(CacheInfo, ">>>>>>got response for addr %#x\n", pkt->getAddr());
  insert(pkt);

  if(originalPacket != nullptr)
  {
    DPRINTF(CacheInfo, ">>>>>>copying data from new packet to old\n");
    [[maybe_unused]] bool hit = accessFunctional(originalPacket);
    panic_if(!hit, "should always hit after inserting!");
    originalPacket->makeResponse();
    delete pkt;
    pkt = originalPacket;
    originalPacket = nullptr;
  }

  sendResponse(pkt);

  return true;
}

void 
TestCache::sendResponse(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::sendResponse\n");
  assert(blocked);

  DPRINTF(CacheInfo, ">>>>>>sending response for addr %#x\n", pkt->getAddr());
  int port = waitingPortId;

  blocked = false;
  waitingPortId = -1;

  cpuPorts[port].sendPacket(pkt);

  for(auto &port: cpuPorts)
    port.trySendRetry();
}

void
TestCache::handleFunctional(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::handleFunctional\n");
  if(accessFunctional(pkt))
    pkt->makeResponse();
  else
    memPort.sendFunctional(pkt);
}

void 
TestCache::accessTiming(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::accessTiming\n");
  bool hit = accessFunctional(pkt);

  DPRINTF(CacheInfo, ">>>>>>%s for packet %s\n", hit ? "Hit": "Miss", pkt->print());
  if(hit)
  {
    DDUMP(TestCache, pkt->getConstPtr<uint8_t>(), pkt->getSize());
    pkt->makeResponse();
    sendResponse(pkt);
  } else {
    missTime = curTick();

    //check the size of packet and whether it is aligned
    Addr addr = pkt->getAddr();
    Addr block_addr = pkt->getBlockAddr(blockSize);
    unsigned size = pkt->getSize();

    if(addr == block_addr && size == blockSize)
    {
      DPRINTF(CacheInfo, ">>>>>>forwarding packet\n");
      memPort.sendPacket(pkt);
    } else {
      DPRINTF(CacheInfo, ">>>>>>upgrading packet to block size\n");
      panic_if(addr - block_addr + size > blockSize, 
               "cannot handle access that span multiple cache lines");  
      assert(pkt->needsResponse());

      MemCmd cmd;
      if(pkt->isWrite() || pkt->isRead())
      {
        cmd = MemCmd::ReadReq;
      } else {
        panic("unknown pakcet type in upgrade size");
      }

      PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
      
      new_pkt->allocate();

      //should now be block aligned
      assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));

      originalPacket = pkt;

      DPRINTF(CacheInfo, ">>>>>>forwarding packet\n");
      memPort.sendPacket(new_pkt);
    }
  }
}

bool
TestCache::accessFunctional(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::accessFunctional\n");
  Addr block_addr = pkt->getBlockAddr(blockSize);
  auto it = cacheStore.find(block_addr);

  if(it != cacheStore.end()) {
    if(pkt->isWrite())
      pkt->writeDataToBlock(it->second, blockSize);
    else if(pkt->isRead())
      pkt->setDataFromBlock(it->second, blockSize);
    else
      panic("unkonw pakcet type");

    return true;
  }
  return false;
}

void 
TestCache::insert(PacketPtr pkt)
{
  DPRINTF(TestCache, "TestCache::insert\n");
  //the packet should be aligned
  assert(pkt->getAddr() == pkt->getBlockAddr(blockSize)); 
  //the address should not be in the cache
  assert(cacheStore.find(pkt->getAddr()) == cacheStore.end());
  //the pkt should be a response
  assert(pkt->isResponse());

  if(cacheStore.size() >= capacity)
  {
    int bucket, bucket_size;

    do {
      bucket = random_mt.random(0, (int)cacheStore.bucket_count() - 1);
    } while((bucket_size = cacheStore.bucket_size(bucket)) == 0);

    auto block = std::next(cacheStore.begin(bucket), random_mt.random(0, bucket_size - 1));

    DPRINTF(CacheInfo, ">>>>>>removing addr %#x\n", block->first);
    
    //write back the data
    //create a new request-packet pair
    RequestPtr req = std::make_shared<Request> (block->first, blockSize, 0, 0);

    PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize); 
    new_pkt->dataDynamic(block->second);

    DPRINTF(CacheInfo, ">>>>>>writing packet back %s\n", pkt->print());
    memPort.sendPacket(new_pkt);

    cacheStore.erase(block->first);
  }

  DPRINTF(CacheInfo, ">>>>>>inserting %s\n", pkt->print());
  DDUMP(TestCache, pkt->getConstPtr<uint8_t>(), blockSize);

  uint8_t *data = new uint8_t[blockSize];

  cacheStore[pkt->getAddr()] = data;

  pkt->writeDataToBlock(data, blockSize);
}

AddrRangeList
TestCache::getAddrRanges() const
{
  DPRINTF(TestCache, "TestCache::getAddrRanges\n");
  DPRINTF(CacheInfo, ">>>>>>sending new ranges\n");
  return memPort.getAddrRanges();
}

void
TestCache::sendRangeChange() const
{
  DPRINTF(TestCache, "TestCache::sendRangeChange\n");
  for(auto &port:cpuPorts)
  {
    port.sendRangeChange();
  }
}




}
