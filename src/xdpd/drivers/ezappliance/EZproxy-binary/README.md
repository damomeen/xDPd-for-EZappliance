EZproxy and Hardware Datapath (binary)
==========
### EZproxy

EZ Proxy is a wrapper around EZchip EZdriver library. EZ Proxy simplifies EZchip vendor C++ library API, hiding the complexity of NP-3 network processor controlling and exposes the required functionality by IPC Corba technology. EZ Proxy offers direct access to search structures and statistic counters located in NP-3 memory, allows for TOP MicroPrograms deploying inside NP-3 and controlling MicroPrograms behavior. API allows also for bidirectional frame transmission between Control CPU and NP-3, and for initial configuration of network processor and its memories. EZ Proxy has been developed in C++ language.

### Hardware Datapath
Hardware Datapath is an OpenFlow pipeline implementation for NP-3 network processor. It is processing frames with a full speed of the network processor. Hardware Datapath is implemented as a set of TOP MicroPrograms using EZchip assembler language. In Hardware Datapath, the OpenFlow pipeline is implemented directly using one search structure dedicated for all flow tables and second search structure dedicated for mapping of output ports into physical resources (queues in NP-3 Traffic Manager). The flow tables are stored in TCAM memory supporting wildcard matching which imposes limitations for amount of supported header fields. The packets which were not matched successfully in Hardware Datapath are sent to EZappliance hardware driver instance in xDPd framework. 

### How to run the binary
EZ Proxy is hosted on EZappliance Power PC embedded Linux where EZchip libraries are installed and direct access to EZppliance PCI interface is provided. 

####EZ Proxy compilation 
For the compilation process the proprietary EZChip development environment is necessary. Final compiled program during run time on Control CPU loads dedicated MicroPrograms into TOP cores, sets up the whole environment (create channel, run driver) and runs EZ Proxy API. After successful compilation the new created binary can be found here: 
```bash
/home/alien/HAL/EZproxy/run
```

####How to mount EZproxy to the EZappliance 
EZ Proxy is accessible from EZappliance Control CPU through the samba software and smb/cifs protocol. To mount the VM with EZproxy to the Control CPU system:
- Login as a root to EZappliance (IP address of the Control CPU board) and check that /etc/profile file contains: 
```bash
  sh# export LD_LIBRARY_PATH=/mnt/ezsamba/HAL/EZproxy/cross-compile/omniorb-cross-sx/lib:/mnt/ezsamba/HAL/EZproxy/cross-compile/lib
  sh# export PATH=$PATH:/mnt/ezsamba/HAL/EZproxy/cross-compile/omniorb-cross-sx/bin
  sh# export LIBS=$LIBS:/mnt/ezsamba/HAL/EZproxy/cross-compile/lib
```

Then run the script: 
```bash
  sh# cd /home/user1
  sh# ./ezsamba_alien.sh
```

####How to run microcode for TOPs and EZproxy 
ALIEN EZ Proxy executable binary should be run from Control CPU system. EZ Proxy uses proprietary shared object libraries from EZChip company. That's why Control CPU must be connected through SAMBA with additional Virtual Machine with EZChip libraries. EZ Proxy is accessible from EZappliance Control CPU through directory: /mnt/ezsamba.
EZ Proxy can be run in normal or in debug mode. Debug mode is needed for debugging TOP processors using MDE (EZChip MicroCode Development Environment). It can be configured in the file located at:
```bash
/mnt/ezsamba/HAL/EZproxy/run/AHE_MODE_INI
```
#####Normal mode configuration (AHE_MODE_INI file)
```bash
  REAL
  MCODE_HOST
  CHAN_CRT_HOST
  FRAMES_FROM_HOST_ALSO
  MULTI_ENGINE
```

#####Debug mode configuration (AHE_MODE_INI file)
```bash
  REAL
  MCODE_MDE
  CHAN_CRT_HOST
  FRAMES_FROM_HOST_ALSO
  SINGLE_ENGINE
```
All event are logged to EZproxy.log file in /mnt/ezsamba/HAL/EZproxy/run/

####How to run EZ Proxy in debug mode (trace information mode):
Login as a root to EZappliance (IP address of the Control CPU board)
```bash
  sh# cd /mnt/ezsamba/HAL/EZproxy/run
  sh# ./ALIEN d
```
When binary file is executed it performs the following tasks:
- load microcode into NP-3 TOP cores,
- initialize the whole environment to work
- create channel
- create CORBA interfaces and related ior files:
  - /mnt/ezsamba/HAL/EZproxy/iors/EZapi_struct.ior – ior for manipulation of search entries in NP-3
  - /mnt/ezsamba/HAL/EZproxy/iors/EZapi_monitor.ior – ior for gathering information about data plane ports of EZappliance
  - /mnt/ezsamba/HAL/EZproxy/iors/EZapi_tm.ior – ior for NP-3 Traffic Managers as well as Tocken Bucket mechanism management 
- create TCP listening socket for network frames exchange between Hardware Datapath and xDPd driver for EZappliance. 


