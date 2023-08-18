# (Exp #6 \& Exp#8) Micro-benchmark in data collection

## Requirement

The requirement and configuration is the same as [the prototype](../README.md).

In addition, you will need `libcrcutil` on the OmniWindow host:
```sh
# On the OmniWindow host
sudo apt install libcrcutil-dev
```

## Build

You can build OmniWindow with this experiment by adding the option `-DENABLE_EXP_DC=ON`. You may also specify the number of hash functions (from 1 to 4, and 4 by default) by option `-DCM_NUM=<num>`. Note that you have to specify the same value on the switch and the host. For example:

- On the OmniWindow host:

    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory.
    cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_EXP_DC=ON -DCM_NUM=4
    make -j
    ```

    Programs in thie experiment include: `dc-cpc-rdma-host`, `dc-dpc-host`, `dc-dpc-rdma-host`, `dc-ow-host`, `dc-ow-rdma-host`, `reset-os-host`, and `reset-ow-host`.

- On the switch:

    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory.
    cmake .. -DIS_SWITCH=ON -DENABLE_EXP_DC=ON -DCM_NUM=4
    make
    ```

    Programs in thie experiment include: `dc-os-switch`, `dc-cpc-switch`, `dc-cpc-rdma-switch`, `dc-dpc-switch`, `dc-dpc-rdma-switch`, `dc-ow-switch`, `dc-ow-rdma-switch`, `reset-os-switch`, and `reset-ow-switch`.

## Experiment

### For OS experiments (`dc-os` and `reset-os`)

1. Start the switch program:
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the swtich.
    # ./Exp-dc/<name>-switch.sh
    ./Exp-dc/dc-os-switch.sh
    ```
    It should print:
    ```
    Inject the trace, and then press Enter... (Ctrl-D to quit)
    ```

2. As indicated by the prompt, inject the trace on the host, similar to that in [the prototype](../README.md):
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the OmniWindow host.
    sudo ./tools/trace ../data/subwindow.pcap ens85f0 100000 1
    ```
    and press Enter on the switch program.

    The program will print the time usage (in seconds):
    ```
    Time: 9.274723134934902
    ```

3. You may repeat step 2 mutiple times. After that, press Ctrl-D on the switch program to quit.

### For `reset-ow`

1. Start the switch program:
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the swtich.
    ./Exp-dc/reset-ow-switch.sh
    ```

    It should print (and the CLI will then quit):
    ```
    Initialization done.
    ```

2. Start the host program:
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the host.
    # sudo ./Exp-dc/reset-ow-host <dpdk-port-id> <host-ip>
    sudo ./Exp-dc/reset-ow-host 0 10.64.100.37
    ```

3. Open another terminal on the host to inject the trace. The same as before:
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the OmniWindow host.
    sudo ./tools/trace ../data/subwindow.pcap ens85f0 100000 1
    ```
    and press Enter on the host program.

    The program will print the time usage:
    ```
    done 0.015071 (s)
    ```

4. You may repeat step 3 mutiple times. After that, press Ctrl-D on the host program to quit.

### For other collection experiments

1. Generate keys. You only need to do this once for each `CM_NUM`.
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the host.
    ./Exp-dc/get-keys ../data/subwindow.pcap 100000 1 keys.bin
    ```

2. Start the switch program:
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the swtich.
    # ./Exp-dc/<program>-switch.sh
    ./Exp-dc/dc-ow-rdma-switch.sh
    ```
    - For RDMA solutions, it should print:

        ```
        ...
        Waiting for the OmniWindow host...
        ```
    - For non-RDMA solutions, it should print:

        ```
        ...
        Initialization done.
        Press Enter to reset. Ctrl-D to quit.
        ```

3. Start the host program:

    Make sure you are at the `${OmniWindow_DIR}/build/` directory on the **host**.

    The general format is like:
    ```sh
    sudo ./Exp-dc/<program>-host <dpdk-port-id> <host-ip> [rdma-dev] [switch-ctrl-ip] <input-keys> <result-path>
    ```
    where `<rdma-dev>` and `<switch-ctrl-ip>` is only for RDMA solutions.
    - For RDMA solutions, for example:

        ```sh
        sudo ./Exp-dc/dc-ow-rdma-host 0 10.64.100.37 mlx5_0 172.16.112.45 keys.bin /dev/null
        ```
        The program on the switch should then print:
        ```
        Initialization done.
        Press Enter to reset. Ctrl-D to quit.
        ```
    - For non-RDMA solutions, for example:

        ```sh
        sudo ./Exp-dc/dc-ow-host 0 10.64.100.37 keys.bin /dev/null
        ```
        The host program should print:
        ```
        init
        ```

4. Open another terminal on the host to inject the data. The same as before:
    ```sh
    # Make sure you are at the ${OmniWindow_DIR}/build/ directory on the OmniWindow host.
    sudo ./tools/trace ../data/subwindow.pcap ens85f0 100000 1
    ```

5. Press Enter on the host program (for solutions other than `dpc` and `dpc-rdma`, you may also input the number of recirculation packets before pressing Enter, which is by default 3 for non-RDMA solutions and 16 for RDMA solutions) to collect the data. It will then print the result like:
    ```
    done (59094 keys) 0.013545s
    ```

6. To repeat the experiment, you may first press Enter on the switch program to reset. After that, you can repeat step 4 and 5. When finished, you can press Ctrl-D to both programs to quit.
