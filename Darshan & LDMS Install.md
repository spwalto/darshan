Darshan and LDMSD Streams Installation Instructions
=======================================================
This page provides a brief step-by-step instructions on how to install Darshan on V.

This page contains the current installation process used to install Darshan with LDMS's library in order to implement the ldmsd\_streams functionality to the Darshan code. With this being said, the path directories to ldms install and darshan install can be modified/changes.

The following github repos for this installation process are being used: **ldms repo**: [https://github.com/Snell1224/ovis/tree/netlink-notifier_sw](https://github.com/Snell1224/ovis/tree/netlink-notifier_sw) and **darshan repo**: [https://github.com/darshan-hpc/darshan.git](https://github.com/darshan-hpc/darshan.git)

The current code changes made to the Darshan Code (and it's makefiles) can be found here: [https://github.com/Snell1224/darshan.git](https://github.com/Snell1224/darshan.git) (darshanConnector branch).

Additional information and instructions on this page include:

*   How to run an existing Darshan test.
*   How to run and ldmsd\_streams daemon with the current Darshan code changes. 
*   Current status of publishing the DXT module data to an ldmsd\_streams daemon.

Setup Environment Variables
===========================

Below are the ENV variables set in order to build and install Darshan. This file can be found in /projects/ovis/darshanConnector/setup-darshan.sh. Run "source" to set the env variables.

**NOTE:** Please keep in mind the current LDMS\_install directory is a git branch version of OVIS containing the ldms\_sps library. This library is used for the ldmsd_streams implementation (i.e. darshanConnector branch from [https://github.com/Snell1224/darshan.git](https://github.com/Snell1224/darshan.git)). If you wish to implement the ldmsd_streams using this library then please make sure an OVIS version with ldms/src/ldmsd/ldms\_sps.c code is cloned and installed. Otherwise, the Darshan will throw an error when exporting the LD\_PRELOAD and LD\_LIBRARY\_PATH variables.

* If you wish to install darshan by itself (without LDMS) then please remove "LD_LIBRARY_PATH".
```
module unload PrgEnv-intel/6.0.9
module load PrgEnv-gnu
export LD_PRELOAD=/projects/darshan/install/lib/libdarshan.so
export DXT_ENABLE_IO_TRACE=1
export LD_LIBRARY_PATH=/projects/ovis/darshanConnector/ovis-sps/LDMS_install/lib/
#set env variables for ldms_streams daemon testing
export DARSHAN_LDMS_STREAM="stream=darshanConnector"
export DARSHAN_LDMS_XPRT=sock
export DARSHAN_LDMS_HOST=<hostname>
export DARSHAN_LDMS_PORT=<port-number>
export DARSHAN_LDMS_AUTH=none
```

Setup Darshan Makefiles
-----------------------

*   For darshan-runtime and darshan-util to see the ldms install directory and ldms\_sps library (helper for ldms\_streams plugin api), please include the following to BOTH darshan-runtime and darshan-util Makefile.in.

**NOTE:** Please keep in mind the current LDMS\_install directory is a git branch version of OVIS containing the ldms\_sps library. This library is used for the ldmsd_streams implementation (i.e. darshanConnector branch from [https://github.com/Snell1224/darshan.git](https://github.com/Snell1224/darshan.git)). If you wish to implement the ldmsd_streams using this library then please make sure an OVIS version with ldms/src/ldmsd/ldms\_sps.c code is cloned and installed.

*   If you wish to build and install darshan by itself (without LDMSD) then please skip this step. No changes will need to be made to the Makefiles.
```
vi /projects/ovis/darshanConnector/darshan/darshan-runtime/Makefile.am
LDFLAGS = -L/projects/ovis/darshanConnector/ovis-sps/LDMS_install/lib -Wl,-rpath=/projects/ovis/darshanConnector/ovis-sps/LDMS_install/lib
....
ldms_dir = /projects/ovis/darshanConnector/ovis-sps/LDMS_install
LDFLAGS += -L$(ldms_dir)/lib -Wl,-rpath=$(ldms_dir)/lib

CFLAGS = -I$(ldms_dir)/include
LIBS = -lldms_sps
```

Configure & Install: Darshan-runtime 
-------------------------------------

*   Below are the commands to configure and build darshan-runtime. This is for tracking MPI applications and creating the log files. 

**NOTE:** This configuration is for the build and install of Darshan with LDMSD streams integration. To configure Darshan-runtime by itself (not including LDMSD) remove the "CFLAGS" "LDFLAGS" and "LIBS" variables (i.e. follow the install instructions for darshan-runtime here: [https://www.mcs.anl.gov/research/projects/darshan/docs/darshan-runtime.html](https://www.mcs.anl.gov/research/projects/darshan/docs/darshan-runtime.html))

```
cd darshan/darshan-runtime/
CFLAGS="-I/projects/ovis/darshanConnector/ovis-sps/LDMS_install/include" LDFLAGS="-L/projects/ovis/darshanConnector/ovis-sps/LDMS_install/lib -Wl,-rpath=/projects/ovis/darshanConnector/ovis-sps/LDMS_install/lib" LIBS="-lldms_sps" ./configure --with-log-path=/projects/darshan/logs --prefix=/projects/darshan/install --with-jobid-env=PBS_JOBID CC=cc
make && make install
```

Configure & Install: Darshan-util
---------------------------------

*   Below are the commands to configure and build darshan-runtime. This is for analyzing the log files.

**NOTE:** This configuration is for the build and install of Darshan with LDMSD streams integration.  To configure Darshan-util by itself (not including LDMSD) remove the "CFLAGS" "LDFLAGS" and "LIBS" variables (i.e. follow the install instructions for darshan-util here: [https://www.mcs.anl.gov/research/projects/darshan/docs/darshan-util.html](https://www.mcs.anl.gov/research/projects/darshan/docs/darshan-util.html))

```
cd darshan/darshan-util/ 
CFLAGS="-I/projects/ovis/darshanConnector/ovis-sps/LDMS_install/include" LDFLAGS="-L/projects/ovis/darshanConnector/ovis-sps/LDMS_install/lib -Wl,-rpath=/projects/ovis/darshanConnector/ovis-sps/LDMS_install/lib/" LIBS="-lldms_sps" ./configure --prefix=/projects/darshan/install
make && make install
```

Running A Test
--------------

*   To run all Darshan example tests, you will need to first edit the following file to assign the desired partition name for the sbatch command.

```cd /projects/ovis/darshanConnector/darshan/darshan-test/regression
vi cray-module-nersc/runjob.sh

\*INSIDE RUNJOB.SH\*
# submit job and wait for it to return
sbatch --wait -N 1 -t 10 -p <name-of-partition> $NODE_CONSTRAINTS --output $DARSHAN_TMP/$$-tmp.out --error $DARSHAN_TMP/$$-tmp.err $DARSHAN_TESTDIR/$DARSHAN_PLATFORM/slurm-submit.sl "$@"
```

*   Next, if you want to run all tests then run the following commands with in the directory: /darshan/darshan-test/regression:

```
cd /projects/ovis/darshanConnector/darshan/darshan-test/regression/
./run-all.sh /projects/darshan/install /projects/darshan/test cray-module-nersc
```

The sbatch output runs will be saved to /projects/darshan/test (this can be any directory) along with Darshan's output files (binary and parsed) for all Darshan tests located in darshan/darshan-test/regression/test-cases/....

**NOTE:** If you would only like to run a single test within /test-cases/ then please edit the following "\*.sh" (line 50) to _<name-of-test-from-test-case-dir.sh>_ in the "run-all.sh" file :

```
cd /projects/ovis/darshanConnector/darshan/darshan-test/regression
vi run-all.sh

\*INSIDE RUN-ALL.SH (line 50-58)\*
for i in \`ls $DARSHAN_TESTDIR/test-cases/<name-of-test-from-test-case-dir.sh>\`; do
echo Running ${i}...
$i
if \[ $? -ne 0 \]; then
echo "Error: failed to execute test case $i"
failure_count=$((failure\_count+1))
fi
echo Done.
done
```

Setting Up ldmsd\_streams
-------------------------

If you wish to build and install darshan with ldmsd\_streams and test the ldmsd\_streams log file output of the DXT module data then please follow the instructions below:

*   Clone the repo with ldms\_sps library and current edits to the Darshan code: [https://github.com/baallan/ovis.git](https://github.com/baallan/ovis.git) (netlink-notifier branch) & [https://github.com/Snell1224/darshan.git](https://github.com/Snell1224/darshan.git) (darshanConnector branch).
*   Checkout to the correct branch →  LDMSD: **_git checkout netlink-notifier_** Darshan: **_git checkout darshanConnector_** 
*   Install ldmsd and darshan by following the instructions above.
*   cd to the ldms install path and create the following .conf file **"hello\_stream\_store.conf"**:

```
load name=hello_sampler
config name=hello_sampler producer=host1 instance=host1/hello_sampler stream=darshanConnector component_id=1
start name=hello_sampler interval=1000000 offset=0

#store data to csv under ./streams/store (this directory must already exist).
load name=l2_stream_csv_store
config name=l2_stream_csv_store path=./streams/store container=csv stream=darshanConnector
```

*    In the same directory, allocate a commute node and run an ldmsd daemon with the following commands:

```
salloc -N 1 --time=1:00:00 -p <partition-name>
ldmsd -x sock:10444 -c hello_stream_store.conf -l /tmp/hello_stream_store.log -v DEBUG -r ldmsd.pid
```

**NOTE:** To check that the ldmsd daemon is running please run ```ps auwx | grep ldmsd | grep -v grep``` or ```cat /tmp/hello_stream_store.log```

*    Open another terminal window and make sure the ENV variables are set up to the correct <port-number> and <host-name> of the current ldmsd daemon running (e.g. in this case it is port 10444 and the hostname is <username>@hostname).
    *   cd to the installed darshan code from the darshanConnector branch and follow the instructions from **"Running A Test".**
*   Once the tests have been executed, run ```cat /tmp/hello_stream_store.log``` in the terminal window where the ldmsd daemon is running. You should see a similar output to the one below.
```
Fri Aug 06 12:12:27 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_POSIX","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"write","count":1,"rank":0,"hostname":"nid00052","operation":"writes_segment_0","segment":[{"offset":0,"length":16777216,"start_time":"34.735724s","end_time":"35.069703s"}]}", msg_len: 301, entity: 0x2aaabc002030
Fri Aug 06 12:12:27 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_MPIIO","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"write","count":1,"rank":0,"hostname":"nid00052","operation":"writes_segment_0","segment":[{"offset":0,"length":16777216,"start_time":"34.735713s","end_time":"35.070885s"}]}", msg_len: 301, entity: 0x2aaabc0031a0
Fri Aug 06 12:12:27 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_POSIX","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"write","count":1,"rank":3,"hostname":"nid00052","operation":"writes_segment_0","segment":[{"offset":50331648,"length":16777216,"start_time":"35.069728s","end_time":"35.306505s"}]}", msg_len: 308, entity: 0x2aaac4301f90
Fri Aug 06 12:12:27 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_MPIIO","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"write","count":1,"rank":3,"hostname":"nid00052","operation":"writes_segment_0","segment":[{"offset":50331648,"length":16777216,"start_time":"34.735718s","end_time":"35.306757s"}]}", msg_len: 308, entity: 0x2aaac4303100
Fri Aug 06 12:12:27 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_POSIX","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"write","count":1,"rank":1,"hostname":"nid00052","operation":"writes_segment_0","segment":[{"offset":16777216,"length":16777216,"start_time":"34.735734s","end_time":"35.533115s"}]}", msg_len: 308, entity: 0x2aaab4301f90
Fri Aug 06 12:12:27 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_MPIIO","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"write","count":1,"rank":1,"hostname":"nid00052","operation":"writes_segment_0","segment":[{"offset":16777216,"length":16777216,"start_time":"34.735720s","end_time":"35.533313s"}]}", msg_len: 308, entity: 0x2aaab4303100
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_POSIX","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"write","count":1,"rank":2,"hostname":"nid00052","operation":"writes_segment_0","segment":[{"offset":33554432,"length":16777216,"start_time":"34.735735s","end_time":"35.755328s"}]}", msg_len: 308, entity: 0x2aaab8301f90
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_MPIIO","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"write","count":1,"rank":2,"hostname":"nid00052","operation":"writes_segment_0","segment":[{"offset":33554432,"length":16777216,"start_time":"34.735722s","end_time":"35.755541s"}]}", msg_len: 308, entity: 0x2aaab8303100
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_POSIX","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"read","count":1,"rank":1,"hostname":"nid00052","operation":"reads_segment_0","segment":[{"offset":16777216,"length":16777216,"start_time":"35.758814s","end_time":"35.767273s"}]}", msg_len: 306, entity: 0x2aaab4303100
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_POSIX","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"read","count":1,"rank":2,"hostname":"nid00052","operation":"reads_segment_0","segment":[{"offset":33554432,"length":16777216,"start_time":"35.758814s","end_time":"35.767676s"}]}", msg_len: 306, entity: 0x2aaab8303100
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_MPIIO","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"read","count":1,"rank":1,"hostname":"nid00052","operation":"reads_segment_0","segment":[{"offset":16777216,"length":16777216,"start_time":"35.758809s","end_time":"35.767342s"}]}", msg_len: 306, entity: 0x2aaab43030d0
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_POSIX","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"read","count":1,"rank":0,"hostname":"nid00052","operation":"reads_segment_0","segment":[{"offset":0,"length":16777216,"start_time":"35.758803s","end_time":"35.767815s"}]}", msg_len: 299, entity: 0x2aaabc002030
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_POSIX","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"read","count":1,"rank":3,"hostname":"nid00052","operation":"reads_segment_0","segment":[{"offset":50331648,"length":16777216,"start_time":"35.758810s","end_time":"35.767931s"}]}", msg_len: 306, entity: 0x2aaac4303100
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_MPIIO","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"read","count":1,"rank":2,"hostname":"nid00052","operation":"reads_segment_0","segment":[{"offset":33554432,"length":16777216,"start_time":"35.758809s","end_time":"35.767748s"}]}", msg_len: 306, entity: 0x2aaab83030d0
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_MPIIO","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"read","count":1,"rank":3,"hostname":"nid00052","operation":"reads_segment_0","segment":[{"offset":50331648,"length":16777216,"start_time":"35.758805s","end_time":"35.768177s"}]}", msg_len: 306, entity: 0x2aaac43030d0
Fri Aug 06 12:12:28 2021: CRITICAL  : stream_type: JSON, msg: "{ "module_name":"X_MPIIO","record_id":6222542600266098259,"file_name":"/projects/darshan/test/mpi-io-test.tmp.dat","operation":"read","count":1,"rank":0,"hostname":"nid00052","operation":"reads_segment_0","segment":[{"offset":0,"length":16777216,"start_time":"35.758800s","end_time":"35.768072s"}]}", msg_len: 299, entity: 0x2aaabc002030
```

LDMS\_STREAMS\_STORE OUTPUT
--------------------

* To view the data stored in the generated CSV file from the streams store plugin, kill the ldmsd daemon first by running: ```killall ldmsd```
* Then ```cat``` the file in which the CSV file is located. Below is the stored DXT module data from LDMS's l2\_streams\_store plugin for the mpi-io-test-dxt.sh test case.  
```
> cat streams/store/csv/darshanConnector.1627940179
#rank,file_name,record_id,hostname,count,operation,module_name,segment:offset,segment:start_time,segment:length,segment:end_time,store_recv_time
0,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"writes_segment_0","X_POSIX",0,"34.735724s",16777216,"35.069703s",1628273547.339400
0,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"writes_segment_0","X_MPIIO",0,"34.735713s",16777216,"35.070885s",1628273547.345317
3,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"writes_segment_0","X_POSIX",50331648,"35.069728s",16777216,"35.306505s",1628273547.575436
3,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"writes_segment_0","X_MPIIO",50331648,"34.735718s",16777216,"35.306757s",1628273547.579893
1,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"writes_segment_0","X_POSIX",16777216,"34.735734s",16777216,"35.533115s",1628273547.801820
1,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"writes_segment_0","X_MPIIO",16777216,"34.735720s",16777216,"35.533313s",1628273547.806359
2,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"writes_segment_0","X_POSIX",33554432,"34.735735s",16777216,"35.755328s",1628273548.023990
2,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"writes_segment_0","X_MPIIO",33554432,"34.735722s",16777216,"35.755541s",1628273548.028557
1,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"reads_segment_0","X_POSIX",16777216,"35.758814s",16777216,"35.767273s",1628273548.035725
2,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"reads_segment_0","X_POSIX",33554432,"35.758814s",16777216,"35.767676s",1628273548.036121
1,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"reads_segment_0","X_MPIIO",16777216,"35.758809s",16777216,"35.767342s",1628273548.036409
0,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"reads_segment_0","X_POSIX",0,"35.758803s",16777216,"35.767815s",1628273548.040726
3,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"reads_segment_0","X_POSIX",50331648,"35.758810s",16777216,"35.767931s",1628273548.041034
2,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"reads_segment_0","X_MPIIO",33554432,"35.758809s",16777216,"35.767748s",1628273548.041339
3,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"reads_segment_0","X_MPIIO",50331648,"35.758805s",16777216,"35.768177s",1628273548.041615
0,"/projects/darshan/test/mpi-io-test.tmp.dat",6222542600266098259,"nid00052",1,"reads_segment_0","X_MPIIO",0,"35.758800s",16777216,"35.768072s",1628273548.042147
```
