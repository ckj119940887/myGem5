from m5.params import *
from m5.SimObject import SimObject

class TestMemory(SimObject):
    type = 'TestMemory'
    cxx_header = "tutorial/test_memory.hh"
    cxx_class = 'gem5::TestMemory' 

    inst_port = ResponsePort("cpu side port, receive requests from cpu")
    data_port = ResponsePort("cpu side port, receive requests from cpu")
    mem_port = RequestPort("mem side port, receive requests from memory")
