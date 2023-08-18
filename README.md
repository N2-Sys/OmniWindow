# OmniWindow

## 1. Introduction

OmniWindow is a general and efficient framework for network telemetry to support general and efficient window mechanisms. OmniWindow supports different types of window mechanisms (e.g., tumbling window and sliding window) and variable window size. OmniWindow can be integrated with different types of network telemetry solutions, e.g., query-driven network telemetry and sketch-based telemetry algorithms.



## 2. Content

The OmniWindow project repository includes:

* **`apps/`**: OmniWindow source code of a sample application.
* **`data/`**: directory that contains the processed CAIDA trace.
* **`Exp-1/`**: codes for Exp#1 in the paper.
* **`Exp-2/`**: codes for Exp#2 in the paper.
* **`Exp-dc/`**: codes for Exp#6 and Exp#8 in the paper.
* **`include/`, `lib/`**: supportive source code of the OmniWindow host program.
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
  - Network Topology: Server1 --- Switch --- Server2, where Server1 and Server2 work as the OmniWindow host controller and a receiver, respectively.

- **Software requirements**

  The current version of OmniWindow is tested on:

  - Switch

    - Tofino SDE 9.7.0

  - Server (Ubuntu 20.04)

    - [MLNX_OFED_LINUX](https://network.nvidia.com/products/infiniband-drivers/linux/mlnx_ofed/):  **Make sure the OFED version is equal to or greater than the kernel version.**

        ```sh
        tar xf MLNX_OFED_LINUX-5.7-1.0.2.0-ubuntu20.04-x86_64.tgz
        cd MLNX_OFED_LINUX-5.7-1.0.2.0-ubuntu20.04-x86_64
        sudo ./mlnxofedinstall --dpdk
        ```

    - DPDK [21.11.4](http://core.dpdk.org/download/):

        ```sh
        sudo apt install libpcap-dev meson python3-pyelftools ninja-build libnuma-dev -y
        tar xf dpdk-21.11.4.tar.xz
        cd dpdk-stable-21.11.4
        meson build
        cd build
        ninja
        sudo ninja install
        ```

* **Trace data**

    We provide an archive including processed CAIDA trace file for running experiments. Due to the large size of the data, **please download and extract it to the `${OmniWindow_dir}/data/` directory**. [link1-PKU Drive](https://disk.pku.edu.cn:443/link/4F31A59379FCC985000CB4ED0FDC8129), [link2-Google Drive](https://drive.google.com/file/d/13DgBg6-xR1reF0Mmm5Tv34y4UwsxpcaL/view?usp=sharing)


### 3.2 Build and configure

**Build**

You need to clone and build the project on both the OmniWindow host and the switch.

- On the OmniWindow host:
    ```sh
    git clone git@github.com:N2-Sys/OmniWindow.git
    cd OmniWindow
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j
    ```

- On the switch:
    ```sh
    git clone git@github.com:N2-Sys/OmniWindow.git
    cd OmniWindow
    mkdir build && cd build
    cmake .. -DIS_SWITCH=ON
    make
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

    The `owhost-mac` and `owhost-ip` are the Mac address of the IP address of the OmniWindow host's NIC connected to the switch.

    After updating,  please rename this script to `switch-config.json`.



## 4. Prototype

We use the new-tcp-connections as an example to show the workflow and the time breakdown of OmniWindow (i.e., Exp#4 in our paper).

1. Run the switch program on the **switch**:
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the switch.
    ./apps/new-tcp-connections-tumbling-switch.sh
    ```
    After running the above command, the screen prints the following information:
    ```
    ...
    Waiting for the OmniWindow host...
    ```

2. Run the `new-tcp-connections` application on the OmniWindow **host**:
    ```sh
    # Make sure you are at the ${OmniWindow-DIR}/build/ directory on the OmniWindow host.
    # sudo ./apps/<program>-host <dpdk-port-id> <host-ip> <rdma-dev> <switch-ctrl-ip> <input-keys>
    sudo ./apps/new-tcp-connections-tumbling-host 0 10.64.100.37 mlx5_0 172.16.112.45 ../data/hotKeys-tumbling.txt
    ```
    * The first parameter is dpdk-port-id.
    
    * The second parameter is the ip for our NIC.
    
    * The third parameter is RDMA device name.
    
    * The fourth parameter is the switch ip.
    * The fifth parameter is pre-processed hot keys for testing.
    
    After running the above command, the host screen prints the following information (Note that the `dst_qp`, `rkey`, and  `msg_vaddr_base` vary each time running):
    
    ```
    Init
    dst_qp=0x9e37, rkey=0x69c7b, msg_vaddr_base=0x7f1b09bcd000
    10000 keys inserted
    ```
    And wait until the following information is printed on the **switch** terminal (and the CLI will then quit):
    ```
    ...
    10000 keys inserted
    Initialization done.
    ```
    
3. Open another terminal on the OmniWindow host, and inject the data.
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the OmniWindow host.
    # For example, we are injecting the trace through device ens85f0.
    # The 100000 and 5 here are the subwindow length (in us) and the number of subwindows, repectively.
    sudo ./tools/trace ../data/trace.pcap ens85f0 100000 5
    ```

    Here, to simulate Exp #4 in our manuscript to investigate the breakdown of OmniWindow, we replay the trace in each sub-window manually.

    After each subwindow, press Enter (or input the number of recirculation packets, 16 by default, and then Enter) to the host program to collect. After finished, press Enter to the trace injector to advance to the next subwindow.

    In the actual case, the manual trigger can be replaced by a time-based one to switch subwindows automatically.

    The result should be like [results/tumbling.txt](results/tumbling.txt).

4. Press Ctrl-D to quit the host program.

5. For sliding windows, you can repeat the procedure above, while replacing `new-tcp-connections-tumbling` with `new-tcp-connections-sliding` in step 1 and 2, and replacing `hotKeys-tumbling.txt` with `hotKeys-sliding` in step 2. In addition, you may run the 10 subwindows and see the last 5 ones as the usual case for sliding windows. Therefore, you should set the parameter in step 3:
    ```sh
    sudo ./tools/trace ../data/trace.pcap ens85f0 100000 10
    ```

    The result should be like [results/sliding.txt](results/sliding.txt).



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

If your encounter the following output when you run the OmniWindow host program.

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
