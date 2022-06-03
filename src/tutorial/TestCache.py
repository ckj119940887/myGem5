from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject 

class TestCache(ClockedObject):
    type = 'TestCache'
    cxx_header = 'tutorial/test_cache.hh'
    cxx_class = 'gem5::TestCache'

    cpu_side = VectorResponsePort("CPU side port, recevices requests")
    mem_side = RequestPort("CPU side port, recevices requests")

    latency = Param.Cycles(1, "cycles taken on a hit or to resovel a miss")

    size = Param.MemorySize('16kB', "The size of the cache")

    system = Param.System(Parent.any, "The system this cache is part of")
