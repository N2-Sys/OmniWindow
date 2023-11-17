# OmniWindow

## 1. Introduction

OmniWindow is a general and efficient framework for network telemetry to support general and efficient window mechanisms. OmniWindow supports different types of window mechanisms (e.g., tumbling window and sliding window) and variable window sizes. OmniWindow can be integrated with different types of network telemetry solutions, e.g., query-driven network telemetry and sketch-based telemetry algorithms. We have integrated OmniWindow into Sonata's queries [SIGCOMM'18], i.e., both the C-language simulations and the Tofino P4 implementations.



## 2. Content

The OmniWindow project repository includes:

* **`apps/`**: OmniWindow source code of Sonata's queries.
* **`data/`**: directory that contains the processed CAIDA trace.
* **`Exp-1/`**: codes for Exp#1 in the paper.
* **`Exp-2/`**: codes for Exp#2 in the paper.
* **`Exp-dc/`**: codes for Exp#6 and Exp#8 in the paper.
* **`include/`, `lib/`**: supportive source code of the OmniWindow controller program.
* **`p4-include/`**: supportive source code of the OmniWindow switch program.
* **`results/`**: directory that contains the experiment results.
* **`tools/`**: A tool for sending trace traffic.
* `CMakeLists.txt`
* `README.md`
* `switch-shell.template.txt`, `switch-config.template.json`: configuration template files of the switch.




## 3. Environment requirement

**Note for SIGCOMM'23 artifact evaluation process: We can provide a testbed if needed (could help skip the preparation for the hardware and software environments). See <u>7. Contact</u>**

### 3.1 Testbed

- **Hardware requirements**

  - An Intel Tofino switch.
  - Two servers with a DPDK-compatible and RoCE-capable NIC (we used a Mellanox CX5 for 100GbE) and multi-core CPU, connected by the Tofino switch.
  - Network Topology: Server1 --- Switch --- Server2. Server 1 works as the OmniWindow controller, which (1) injects the network traffic to be monitored into the network; (2) collects the telemetry data of each sub-window; (3) merges the results of sub-windows into the complete window as needed. Server 2 receives the monitored network traffic.

- **Software requirements**

  The current version of OmniWindow is tested on:

  - Switch

    - OS: ONL 2 (kernel: 4.19.81)
    - Tofino SDE 9.7.0 (all other software dependencies should satisfy the requirements of SDE 9.7.0)
    - **Hint**: Newer versions of the OS and SDE (e.g., ubuntu 20.04 with SDE 9.13.1) should work correctly. However, we have not fully verified this in other environments.

  - Server (Ubuntu 20.04)

    - [MLNX_OFED_LINUX](https://network.nvidia.com/products/infiniband-drivers/linux/mlnx_ofed/):  **Make sure the OFED version is equal to or greater than the kernel version.**

        ```sh
        tar xf MLNX_OFED_LINUX-5.7-1.0.2.0-ubuntu20.04-x86_64.tgz
        cd MLNX_OFED_LINUX-5.7-1.0.2.0-ubuntu20.04-x86_64
        sudo ./mlnxofedinstall --dpdk
        ```

    - DPDK [21.11.5](http://core.dpdk.org/download/):

        ```sh
        sudo apt install libpcap-dev meson python3-pyelftools ninja-build libnuma-dev -y
        tar xf dpdk-21.11.5.tar.xz
        cd dpdk-stable-21.11.5
        meson build
        cd build
        ninja
        sudo ninja install
        ```

* **Trace data**

    We provide an archive including processed CAIDA trace file for running experiments. Due to the large size of the data, **please download and extract it to the `${OmniWindow_dir}/data/` directory**. [link1-PKU Drive](https://disk.pku.edu.cn:443/link/4F31A59379FCC985000CB4ED0FDC8129), [link2-Google Drive](https://drive.google.com/file/d/13DgBg6-xR1reF0Mmm5Tv34y4UwsxpcaL/view?usp=sharing)


### 3.2 Build and configure

**Build**

You need to clone and build the project on both the OmniWindow controller and the switch.

- On the OmniWindow controller:
    ```sh
    git clone git@github.com:N2-Sys/OmniWindow.git
    cd OmniWindow
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j
    ```

    If the compilation is successful, you can see the following output:

    ```
    $ make -j
    [ 10%] Building CXX object lib/CMakeFiles/dpdk-toolchain-cpp.dir/dpdk_toolchain_cpp.cpp.o
    [ 20%] Building CXX object lib/CMakeFiles/rdma-tools.dir/RDMA.cpp.o
    [ 30%] Building CXX object tools/CMakeFiles/trace.dir/trace.cpp.o
    [ 40%] Linking CXX executable trace
    [ 50%] Linking CXX static library librdma-tools.a
    [ 50%] Built target rdma-tools
    [ 50%] Built target trace
    [ 60%] Linking CXX static library libdpdk-toolchain-cpp.a
    [ 60%] Built target dpdk-toolchain-cpp
    [ 70%] Building CXX object apps/CMakeFiles/new-tcp-connections-sliding-host.dir/new-tcp-connections/host.cpp.o
    [ 80%] Building CXX object apps/CMakeFiles/new-tcp-connections-tumbling-host.dir/new-tcp-connections/host.cpp.o
    [ 90%] Linking CXX executable new-tcp-connections-sliding-host
    [100%] Linking CXX executable new-tcp-connections-tumbling-host
    [100%] Built target new-tcp-connections-sliding-host
    [100%] Built target new-tcp-connections-tumbling-host
    ```

- On the switch:

    ```
    git clone git@github.com:N2-Sys/OmniWindow.git
    cd OmniWindow
    mkdir build && cd build
    cmake .. -DIS_SWITCH=ON
    make
    ```

    If the compilation is successful, you can see the following output:

    ```sh
    ......
    Install the project...
    -- Install configuration: "RelWithDebInfo"
    -- Up-to-date: /root/bf-sde-9.7.0/install/share/p4/targets/tofino
    -- Installing: /root/bf-sde-9.7.0/install/share/p4/targets/tofino/switch.conf
    -- Installing: /root/bf-sde-9.7.0/install/share/p4/targets/tofino/omniwin-new-tcp-connections-tumbling-switch.conf
    -- Up-to-date: /root/bf-sde-9.7.0/install/share/tofinopd/omniwin-new-tcp-connections-tumbling-switch
    -- Up-to-date: /root/bf-sde-9.7.0/install/share/tofinopd/omniwin-new-tcp-connections-tumbling-switch/pipe
    -- Installing: /root/bf-sde-9.7.0/install/share/tofinopd/omniwin-new-tcp-connections-tumbling-switch/pipe/context.json
    -- Installing: /root/bf-sde-9.7.0/install/share/tofinopd/omniwin-new-tcp-connections-tumbling-switch/pipe/tofino.bin
    -- Installing: /root/bf-sde-9.7.0/install/share/tofinopd/omniwin-new-tcp-connections-tumbling-switch/bf-rt.json
    -- Installing: /root/bf-sde-9.7.0/install/share/tofinopd/omniwin-new-tcp-connections-tumbling-switch/source.json
    -- Installing: /root/bf-sde-9.7.0/install/share/tofinopd/omniwin-new-tcp-connections-tumbling-switch/events.json
    [100%] Built target new-tcp-connections-tumbling-switch
    ```

**Configuration files**

The two files are at root directory of the OmniWindow project **on the switch**.

You may clone and build the project on the switch:

* `switch-shell.template.txt`:
    ```
    ucli
    pm
    port-add 17/0 100G rs
    port-add 18/0 100G rs
    port-enb 17/0
    port-enb 18/0
    show
    end
    exit
    ```

    Here, our Server1 and Server2 are connected to the 17th and 18th ports of the switch, respectively. To run the experiments, you need to modify the port number and the bandwidth in port-add and port-enb commands according to your NICs. If your NICs are not 100 GbE, we recommend you modify the forwarding error correction setting (the third parameter of port-add command) from `rs` to `NONE`. After updating, please rename this script to `switch-shell.txt`.

* `switch-config.template.json`:
    ```json
    {
      "owhost-dp": 4,
      "recv-dp": 12,
      "owhost-mac": "0c:42:a1:67:8d:2e",
      "owhost-ip": "10.64.100.37"
    }
    ```

    The `owhost-dp` and `recv-dp` are the device ports within the switch of the two ports connected to the two servers. The device ports are printed in the `D_P` column of the table after running the switch program, which are 4 and 12 in our example.

    For example, if you run on the switch (as in <u>**4. Prototype**</u>) but without the `switch-config.json`:
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the switch.
    make new-tcp-connections-tumbling-switch
    ./apps/new-tcp-connections-tumbling-switch.sh
    ```
    you can get the table like:
    ```
    bfshell> ucli
    Cannot read termcap database;
    using dumb terminal settings.
    bf-sde> pm
    bf-sde.pm> port-add 17/0 100G rs
    bf-sde.pm> port-add 18/0 100G rs
    bf-sde.pm> port-enb 17/0
    bf-sde.pm> port-enb 18/0
    bf-sde.pm> show
    -----+----+---+----+-------+----+--+--+---+---+---+--------+----------------+----------------+-
    PORT |MAC |D_P|P/PT|SPEED  |FEC |AN|KR|RDY|ADM|OPR|LPBK    |FRAMES RX       |FRAMES TX       |E
    -----+----+---+----+-------+----+--+--+---+---+---+--------+----------------+----------------+-
    17/0 | 7/0|  4|1/ 4|100G   | RS |Au|Au|YES|ENB|DWN|  NONE  |               0|               0|
    18/0 | 6/0| 12|1/12|100G   | RS |Au|Au|YES|ENB|DWN|  NONE  |               0|               0|
    bf-sde.pm> end
    bfshell> exit
    Missing file /home/lijiaheng/OmniWindow-AE/switch-config.json
    Press Ctrl-C to quit...
    ```
    in which port `17/0` and `18/0` are corresponding to `D_P` of 4 and 12, repectively.

    The `owhost-mac` and `owhost-ip` are the Mac address of the IP address of the OmniWindow controller's NIC connected to the switch.

    After updating,  please rename this script to `switch-config.json`.



## 4. Prototype

We use the new-tcp-connections as an example to show the workflow and the time breakdown of OmniWindow (i.e., Exp#4 in our paper).

In this experiment, we will run OmniWindow the switch and the controller.
We will inject trace into the network by each sub-window, and manually trigger sub-window switching on the controller.
The data will be collected to the controller, and abnormalies will be detected. Some information of the collected data and detected abnormalies will be shown for testing.

1. **Terminal 1**: Run the switch program on the **switch**:

    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the switch.
    ./apps/new-tcp-connections-tumbling-switch.sh
    ```
    After running the above command, the screen prints the following information:
    ```
    ...
    Waiting for the OmniWindow host...
    ```

2. **Terminal 2**: Run the `new-tcp-connections` application on the **OmniWindow controller** :

    ```sh
    # Make sure you are at the ${OmniWindow-DIR}/build/ directory on the OmniWindow controller.
    # sudo ./apps/<program>-host <dpdk-port-id> <host-ip> <rdma-dev> <switch-ctrl-ip> <input-keys>
    sudo ./apps/new-tcp-connections-tumbling-host 0 10.64.100.37 mlx5_0 172.16.112.45 ../data/hotKeys-tumbling.txt
    ```
    * The first parameter is dpdk-port-id.

    * The second parameter is the IP for our NIC.

    * The third parameter is the RDMA device name.

    * The fourth parameter is the switch IP.
    * The fifth parameter is pre-processed hotkeys for testing.

    After running the above command, the OmniWindow controller screen prints the following information (Note that the `dst_qp`, `rkey`, and  `msg_vaddr_base` vary each time running):

    ```
    Init
    dst_qp=0x9e37, rkey=0x69c7b, msg_vaddr_base=0x7f1b09bcd000
    10000 keys inserted
    ```
    Wait until the following information is printed on the **Terminal 1** (and the CLI will then quit):
    ```
    ...
    10000 keys inserted
    Initialization done.
    ```

3. Open another terminal (**Terminal 3**) on the OmniWindow controller, and inject the trace.
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the OmniWindow controller.
    # For example, we are injecting the trace through device ens85f0.
    # The 100000 and 5 here are the subwindow length (in us) and the number of subwindows, repectively.
    sudo ./tools/trace ../data/trace.pcap ens85f0 100000 5
    ```

    Here, to simulate Exp #4 in our manuscript to investigate the breakdown of OmniWindow, we replay the trace in each sub-window manually.

    After each subwindow, press Enter (or input the number of recirculation packets, 16 by default, and then Enter) at **Terminal 2** for the OmniWindow controller to collect telemetry data. This will trigger a sub-window switching. Then, press Enter to the trace injector at **Terminal 3** to advance to the next subwindow and inject its trace.

    In the actual case, the manual trigger can be replaced by a time-based one to switch subwindows automatically.

    The result on **Terminal 2** should be like:

    ```
    0 overflow keys
    11153 messages received (2 Flushes)
    21151 keys in total
    recv: 0.000408s, ins: 0.001529s, merge: 0.000015s

    9215 overflow keys
    10865 messages received (2 Flushes)
    38248 keys in total
    recv: 0.000708s, ins: 0.001100s, merge: 0.000024s

    6751 overflow keys
    10737 messages received (2 Flushes)
    53028 keys in total
    recv: 0.000663s, ins: 0.001518s, merge: 0.000042s

    0 overflow keys
    9551 messages received (2 Flushes)
    59488 keys in total
    recv: 0.000402s, ins: 0.001565s, merge: 0.000050s

    5452 overflow keys
    10855 messages received (2 Flushes)
    70606 keys in total
    recv: 0.000693s, ins: 0.001977s, merge: 0.000068s
    27561 abnormals detected
    compare: 0.000165s
    ```

    `overflow keys` stands for keys that cannot be buffered in data plane; `messages` stands for AFRs sent from data plane, or other control messages (only a few); `recv`, `ins`, `merge`, and `compare` stands for the time for receiving AFRs, insertings keys, merging sub-windows, and detecting abnormalies by comparing merged data with the given threshold, respectively. `Flushes` are control messages that notify the control plane for receiving AFRs, whose logging is only for debugging purpose and does not need to be minded by users.

4. Press Ctrl-D to quit the OmniWindow controller program.

5. For sliding windows, you can repeat the procedure above, while replacing `new-tcp-connections-tumbling` with `new-tcp-connections-sliding` in steps 1 and 2, and replacing `hotKeys-tumbling.txt` with `hotKeys-sliding` in step 2. In addition, you may run the 10 subwindows and see the last 5 ones as the usual case for sliding windows. Therefore, you should set the parameter in step 3:
    ```sh
    sudo ./tools/trace ../data/trace.pcap ens85f0 100000 10
    ```

    The result should be like:

    ```
    0 overflow keys
    11765 messages received (2 Flushes)
    21763 keys in total
    recv: 0.000410s, ins: 0.001528s, merge: 0.000101s
    4630 abnormals detected
    compare: 0.000210s
    0 keys deleted (original 21763)
    sub: 0.000064s, find-del: 0.000579s, del: 0.000000s

    ......

    5080 overflow keys
    10682 messages received (2 Flushes)
    51938 keys in total
    recv: 0.000491s, ins: 0.001631s, merge: 0.000078s
    29168 abnormals detected
    compare: 0.000055s
    10678 keys deleted (original 62616)
    sub: 0.000055s, find-del: 0.000051s, del: 0.000776s

    1890 overflow keys
    11464 messages received (2 Flushes)
    51032 keys in total
    recv: 0.000435s, ins: 0.001914s, merge: 0.000144s
    35453 abnormals detected
    compare: 0.000119s
    11386 keys deleted (original 62418)
    sub: 0.000139s, find-del: 0.000100s, del: 0.001059s

    ```

### Hints for integrating OmniWindow framework into telemetry solutions

 The main additional parts of OmniWindow framework are the following ones. To integrate the framework, one may add these parts to the code. The additional code should be similar and we can take `apps/slowloris-attack/switch.p4` as reference and an example:
 - Management of multiple versions of resources for hiding sub-window switching loss, as line 601 to 613 in our example.
 - Implementation of `distinct` operators (which may have special logic of reset in our framework), as line 620 to 627 in our example.
 - The bloom filter to identify new keys, as line 640 to 647 in our example.
 - The key buffer, as line 653 ti 671 in our example.
 - The generation of messages to the controller, as line 676 to 713, and also line 743 to 821 for (RDMA), in our example.
 - The recirculating log of C&R packets, as line 828 to 848 in our example.


## 5. Experiments

### (Exp#1) Query-driven telemetry

See [Exp-1/README.md](Exp-1/README.md).

### (Exp#2) Sketch-based algorithms

See [Exp-2/README.md](Exp-2/README.md).

### (Exp#4) Controller time usage breakdown

See <u>**4. Prototype**</u>.

### (Exp#6 \& Exp#8) Micro-benchmark in data collection

See [Exp-dc/README.md](Exp-dc/README.md)



## 6. Known Issue

If you encounter the following output when you run the OmniWindow controller program.

```
EAL: Detected CPU lcores: 80
EAL: Detected NUMA nodes: 2
EAL: Detected shared linkage of DPDK
EAL: libmlx4.so.1: cannot open shared object file: No such file or directory
EAL: FATAL: Cannot init plugins
EAL: Cannot init plugins
```

Please delete the following files

```
cd /usr/local/lib/x86_64-linux-gnu/dpdk/pmds-22.0
sudo rm librte_net_mlx4.so.22
sudo rm librte_net_mlx4.so.22.0
sudo rm librte_net_mlx4.so
```



## 7. Contact

Feel free to drop us an email at `sunhaifeng at stu dot pku dot edu dot cn` if you have any questions.
