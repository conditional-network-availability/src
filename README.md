# Conditional Network Availability

This repository contains the implementation of conditional network availability for an Ethernet NIC.
We use a Boundary Devices Nitrogen8M development board.
This repository contains a development environment for ARMv8-A system software development with the board.
The repository contains everything required to assemble an SD card image that can be flashed onto the Nitrogen8M.

In particular, this is:
  * Trusted Firmware A (TFA)
  * U-Boot boot loader
  * Linux kernel
  * Small BusyBox root file system

Every component (including the cross-compiler) is downloaded and built from its source.

## Warning

This is a proof-of-concept implementation of conditional network availability for a NIC.
It is not ready for use in production environments.

## Directory Structure

* `dep/`: Contains the main dependencies necessary to compile the resulting binary that can be flashed onto the Nitrogen8M development board.
  * `dep/buildroot`: A customized fork (origin revision `b5037ecffd5d42771ece111a220e011fec547764`) of the Buildroot framework that is responsible for retrieving, compiling, and assembling the resulting image that is flashed onto the Nitrogen8M development board. More information on Buildroot at https://buildroot.org/.
  * `dep/linux`: A customized fork (origin revision `950d5f1badc48d274efa9b7639c96dfd614c08b4`) of the I.MX Linux kernel. More information at https://github.com/boundarydevices/linux.
  * `dep/tfa`: A customized fork (origin revision `9f6114fde03ebed6ecc482989a7660adc5a41a9d`) of the I.MX ARM Trusted Firmware A. More information at https://github.com/boundarydevices/imx-atf.
  * `dep/uboot`: A customized fork (origin revision `f2c92d838665ec3e3cc8ce8865780df92877ce4e`) of I.MX U-Boot that is a boot loader commonly found in ARM systems. More information at https://github.com/boundarydevices/u-boot-imx6.

* `docker/`: Contains files for creating a uniform build environment relying on Docker images.
* `src/`: Contains the source code to demonstrate conditional network availability for a NIC.
  * `src/host`: Contains the source code to build the host-side benchmarking utilities. Those are meant to be executed on a GNU/Linux system connected to the Nitrogen8M via a LAN.
  * `src/nw`: Contains the changes and components for the normal world (NW).
    * `src/nw/arch`: Contains adjustments to the device tree to take secure memory regions (e.g., descriptor rings) into account.
    * `src/nw/drivers`: Contains the modified NW Ethernet driver files. During compilation, the toolchain copies those files to the kernel's build directory, overwriting the original files.
    * `src/nw/nwecho`: Benchmark that echoes back a packet in the NW.
    * `src/nw/nwlatency`: Benchmark that sends out an UDP test packet while measuring the RTT.
    * `src/nw/nwreceiver`: Benchmark that receives UDP packets in the NW and calculates the throughput.
    * `src/nw/nwsender`: Benchmark that generates UDP packets in the NW and puts them onto the link.
    * `src/nw/txctl`: Helper kernel module. Upon loading the kernel module and SMC is executed that instructs the SW to generate packets in the SW and saturate the link with SW packets. Upon unloading, the sending process is stopped.
  * `src/sw`: Contains the secure world (SW) component that is an TFA secure runtime service. It is linked to the corresponding directory in the ARM Trusted Firmware A.
    * `src/sw/app`: Contains the benchmarking application. One can either compile the application to benchmark throughput (`#define BENCHMARK_THROUGHPUT`) or latency (`// #define BENCHMARK_THROUGHPUT`). If you measure SW latency, you can either measure the latency if the SW initiates the UDP test packet (`#define BENCHMARK_LATENCY_2`) or if the host initiates the UDP test packet and the board echoes it back (`// #define BENCHMARK_LATENCY_2`). Remember to recompile the ARM Trusted Firmware A after changing the benchmark.
    * `src/sw/asm`: Contains optimized `memcpy` and `memset` routines.

## Requirements

### Target Hardware

We developed our system for the Boundary Devices Nitrogen8M development board ( https://boundarydevices.com/product/nitrogen8m/ ).
It features an NXP i.MX 8MQuad Core (Cortex-A53) CPU and 2GB of RAM.
The repository contains everything required to assemble an SD card image that can be flashed onto the Nitrogen8M.
In particular, this is the Trusted Firmware A, the U-Boot boot loader, the Linux kernel, and a small BusyBox root file system.
We use the I.MX versions of the mentioned dependencies which are provided by Boundary Devices.

### Build Host

We use a Linux AMD64 host (Ubuntu 20.04 LTS) to build the image (cross compilation) that can then be flashed onto the Nitrogen8M development board.
The compilation environment is provided as a Docker image to ease the setup.
Thus, Docker is required to build the software and the build process should succeed on other Linux distributions than Ubuntu as well.
Yet, we note that we have not tested to build the image on other hosts.

## Network Configuration

We assume that there is a LAN network with both the board and the host in a common subnet.
Moreover, we assume static IPv4 address assignment.
There are several files that need to be adjusted to the correct IP addresses.

Change the IP address of host in the following files:

* `src/app/app.c` (search for `192.168.188.29` and replace)


* `src/host/nwlatency.c`
* `src/host/nwsender.c`


Change the IP address of board/gateway in the following files:

* `dep/buildroot/overlay/etc/network/interfaces`
* `dep/buildroot/overlay/etc/resolv.conf`


* `src/host/echo.c`
* `src/host/nwecho.c`
* `src/host/latency.c`
* `src/host/nwlatency.c`
* `src/host/sender.c`
* `src/host/nwsender.c`


* `src/app/app.c` (search for `192.168.188.172` and replace)


## Build Process

To build the SD card image that can then be flashed onto the Boundary Devices Nitrogen8M development board, execute the following steps.
We assume that Docker is installed and the `docker` command can be used as a regular user.

Build the Docker image required for compilation:

```shell
$ cd docker
$ ./build_image.sh
```

Open up a shell in the Docker container:

```shell
$ ./shell.sh
```

In the shell **in the Docker container**, execute the following commands to start
the compilation process:

```shell
$ make all
```

Depending on your computing power and your network connection, the compilation process can take a considerable amount of time. On a workstation with an Intel(R) Core(TM) i7-7700 CPU @ 3.60GHz, 32 GB DDR4 RAM, 1TB Intel NVME SSD, and Ubuntu 20.04, the compilation process takes around 30 to 45 minutes.
The output image is stored in `dep/buildroot/output/images/sdcard.img` after the compilation process has finished.
The next step is to deploy the generated file system image to the Boundary Devices Nitrogen8M development board.

## Run Instructions

To run the software, we first need to deploy it to the Boundary Device Nitrogen8M development board.

* Connect the board with a micro USB cable to your computer.
* Connect the board using a serial console to your computer.
* Use `dmesg` on the host system to find out the TTY of the serial-to-USB converter:

```
[22327.771519] usb 1-9: pl2303 converter now attached to ttyUSB0
```

* On the computer, start a serial console with `sudo screen /dev/ttyUSB0 115200`.
  Use the TTY from the previous step. `115200` is the baud rate and should always
  be this value. Nothing is shown yet.

* Check that the physical `SWI / BOOT MODE` switch on the board is in position `OFF`.
* Plug the board into the power grid. It should now print output to the serial console
  in `screen`. You need to press any key to stop the invocation of `autoboot` before
  the countdown expires. You drop into a U-Boot shell:

```
U-Boot 2020.10-52931-gc95874b8b7 (Jun 24 2021 - 14:40:21 -0700), Build: jenkins-uboot_v2020.10-116

CPU:   i.MX8MQ rev2.1 at 1000 MHz
Reset cause: POR
Model: Boundary Devices i.MX8MQ Nitrogen8M
Board: nitrogen8m
       Watchdog enabled
DRAM:  2 GiB
MMC:   FSL_SDHC: 0
Loading Environment from MMC...
OK
Display: hdmi:1920x1080M@60 (1920x1080)
In:    serial
Out:   serial
Err:   serial

 BuildInfo:
  - ATF 63a678c

Hit any key to stop autoboot:  0
=>
```

* Using the U-Boot shell, connect the eMMC device to the computer:

```
ums 0 mmc 0
```

* Find out the right device on your computer with `dmesg`. In this case, it is `sdb`:

```
[24256.299948] scsi 6:0:0:0: Direct-Access     Linux    UMS disk 0       ffff PQ: 0 ANSI: 2
[24256.300670] sd 6:0:0:0: Attached scsi generic sg1 type 0
[24256.300950] sd 6:0:0:0: [sdb] 30621696 512-byte logical blocks: (15.7 GB/14.6 GiB)
[24256.301114] sd 6:0:0:0: [sdb] Write Protect is off
[24256.301120] sd 6:0:0:0: [sdb] Mode Sense: 0f 00 00 00
[24256.301320] sd 6:0:0:0: [sdb] Write cache: enabled, read cache: enabled, doesn't support DPO or FUA
[24256.326788]  sdb: sdb1
[24256.328857] sd 6:0:0:0: [sdb] Attached SCSI removable disk
```

* Without mounting the eMMC partition to the computer, flash the new image for the eMMC card.
The following commands are **NOT** to be executed in the Docker container but directly on your host!
The Docker container, which you enter by `./src/docker/shell.sh` is only intended for providing a uniform **compilation** environment.
The deployment needs to be done directly in a shell on your host.
Check twice that the output file is correct:

```shell
$ cd dep/buildroot
$ cat output/images/sdcard.img | sudo dd of=/dev/sdb bs=1M && /bin/sync
```

* After those commands, press `Ctrl-C` in the U-Boot shell to leave the USB mode.
  You are back in the U-Boot shell:

```
=> ums 0 mmc 0
UMS: LUN 0, dev 0, hwpart 0, sector 0x0, count 0x1d34000
CTRL+C - Operation aborted
=>
```

* You still need to flash the boot loader including ARM Trusted Firmware A as those reside on another internal partition of the eMMC card of the Nitrogen8M. To do this, execute the following command in the **U-Boot shell**. **Note:** There is no spelling error! There is a trailing `u`:

```
env set uboot_defconfig nitrogen8m
run upgradeu
```

* After the boot loader is flashed, the device is reset and should boot in the
  new version of your system software.
  Reset the board by pressing the physical reset button if it is not reset automatically.

* If the board comes up again, you can log into Linux with the user `root` and no password.


More information:
* https://boundarydevices.com/just-getting-started/
* https://wiki.boundarydevices.com/index.php/Nitrogen8M_SBC


## Measuring Throughput

* Check that `#define BENCHMARK_THROUGHPUT` is set in `src/sw/app/app.c`.
* Compile the SD card image.
* Deploy the SD card image to the Nitrogen8M.
* Log into Linux with the user `root` and no password.
* Test general network connectivity from the Linux shell (e.g., via `ping`).

### Normal World

**Nitrogen8M as Sender, Host as Receiver**:

On the host, execute:

```shell
cd src/host
make
./nwreceiver
```

On the Nitrogen8M, execute:

```shell
nwsender
```

Output on the host:

```
TX benchmark started
TX benchark finished. Bytes: 47403c38, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 4741e470, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 4741b188, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 47426c30, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 473da0c8, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 474200b8, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 4740a7b0, Time(s): 10
...
```

**Host as Sender, Nitrogen8M as Receiver**:

On the Nitrogen8M, execute:

```shell
nwreceiver
```

On the host, execute:

```shell
cd src/host
make
./nwsender
```

Output on the Nitrogen8M:

```
TX benchmark started
TX benchark finished. Bytes: 469dc028, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46a685b0, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b81c10, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b7c738, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b85a48, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b7a548, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b74520, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b77808, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b78ea8, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b82d08, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 46b88788, Time(s): 10
...
```


### Secure World

**Nitrogen8M as Sender, Host as Receiver**:

On the host, execute:

```shell
cd src/host
make
./receiver
```

On the Nitrogen8M, execute:

```shell
modprobe txctl
```

Output on the host:

```
TX benchmark started
TX benchark finished. Bytes: 45208190, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45ddb9b0, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45ddec98, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45de1430, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45dde148, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45ddbf58, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45ddf240, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45de2528, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45ddec98, Time(s): 10
TX benchmark started
TX benchark finished. Bytes: 45dd8c70, Time(s): 10
...
```

To stop the Nitrogen8M from sending packets, remove the kernel module:

```shell
rmmod txctl
```

**Host as Sender, Nitrogen8M as Receiver**:

On the host, execute:

```shell
cd src/host
make
./sender
```

Output on the board:

```
INFO:    RX benchark finished. Bytes: 0x4602f478, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46b68a78, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46ba3568, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46b4af58, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46baa0e0, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46ba51b0, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46bb06b0, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46bb1d50, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46b7b098, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46b6fb98, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46ba51b0, Time(s): 10
INFO:    RX benchmark started
INFO:    RX benchark finished. Bytes: 0x46bb3998, Time(s): 10
...
```

## Measuring Latency

* Log into Linux with the user `root` and no password.
* Test general network connectivity from the Linux shell (e.g., via `ping`).

### Normal World

**Nitrogen8M as Sender, Host as Receiver**:

On the host, execute:

```shell
cd src/host
make
./nwecho
```

On the Nitrogen8M, execute:

```shell
nwlatency
```

Output on the host:

```
Millis: 1.665283
Millis: 1.598145
Millis: 2.046387
Millis: 1.683838
Millis: 1.606689
Millis: 1.682129
Millis: 1.552734
Millis: 1.648682
Millis: 1.372314
Millis: 1.516846
Millis: 1.331299
Millis: 1.600098
Millis: 1.645508
Millis: 1.324463
Millis: 1.576904
...
```

**Host as Sender, Nitrogen8M as Receiver**:

On the Nitrogen8M, execute:

```shell
nwecho
```

On the host, execute:

```shell
cd src/host
make
./nwlatency
```

Output on the host:

```
Millis: 1.481201
Millis: 1.634033
Millis: 1.581055
Millis: 1.691406
Millis: 1.402100
Millis: 1.680176
Millis: 1.531006
Millis: 1.699951
Millis: 2.086182
Millis: 1.604004
Millis: 1.361816
Millis: 1.457275
Millis: 1.731934
Millis: 1.554199
Millis: 1.497559
Millis: 1.663574
...
```

### Secure World

**Nitrogen8M as Sender, Host as Receiver**:

* Check that `#define BENCHMARK_THROUGHPUT` is **NOT** set in `src/sw/app/app.c`, i.e., `// #define BENCHMARK_THROUGHPUT`.
* Check that `#define BENCHMARK_LATENCY_2` is set in `src/sw/app/app.c`.
* Compile the SD card image.
* Deploy the SD card image to the Nitrogen8M.
* Log into Linux with the user `root` and no password.
* Test general network connectivity from the Linux shell (e.g., via `ping`).

On the host, execute:

```shell
cd src/host
make
./echo
```

On the Nitrogen8M, execute:

```shell
modprobe txctl
```

Output on the Nitrogen8M:

```
# modprobe txctl
INFO:    RTT: Passed: 0x1458b, Frequency: 0x7f2815

# rmmod txctl && modprobe txctl
INFO:    RTT: Passed: 0x14593, Frequency: 0x7f2815

# rmmod txctl && modprobe txctl
INFO:    RTT: Passed: 0x14591, Frequency: 0x7f2815

# rmmod txctl && modprobe txctl
# INFO:    RTT: Passed: 0x14593, Frequency: 0x7f2815

# rmmod txctl && modprobe txctl
INFO:    RTT: Passed: 0x14591, Frequency: 0x7f2815

# rmmod txctl && modprobe txctl
INFO:    RTT: Passed: 0x1458a, Frequency: 0x7f2815

# rmmod txctl && modprobe txctl
INFO:    RTT: Passed: 0x14590, Frequency: 0x7f2815

# rmmod txctl && modprobe txctl
INFO:    RTT: Passed: 0x14591, Frequency: 0x7f2815
...
```

To get the RTT in seconds, divide the elapsed ticks by the frequency:

```
0x1458b / 0x7f2815 = 0.010000 s = 10.000 ms
```

**Host as Sender, Nitrogen8M as Receiver**:

* Check that `#define BENCHMARK_THROUGHPUT` is **NOT** set in `src/sw/app/app.c`, i.e., `// #define BENCHMARK_THROUGHPUT`.
* Check that `#define BENCHMARK_LATENCY_2` is **NOT** set in `src/sw/app/app.c`, i.e., `// #define BENCHMARK_LATENCY_2`.
* Compile the SD card image.
* Deploy the SD card image to the Nitrogen8M.
* Log into Linux with the user `root` and no password.
* Test general network connectivity from the Linux shell (e.g., via `ping`).

On the host, execute:

```shell
cd src/host
make
./latency
```

Output on the host:

```
Millis: 9.565674
Millis: 10.045166
Millis: 9.661865
Millis: 10.113525
Millis: 9.684082
Millis: 9.957275
Millis: 9.739746
Millis: 10.105225
Millis: 9.672607
Millis: 9.876221
Millis: 10.080811
Millis: 9.689209
Millis: 9.810303
Millis: 9.893799
Millis: 9.796875
Millis: 9.922852
Millis: 9.761475
...
```

## NIC Assignment

Currently, we only assign the NIC to the SW during the runtime of the kernel, i.e., during the initialization of the NIC driver.
This way we do not need to adjust U-Boot (which runs before Linux on our board) as well, which also configures the NIC.
If we would assign the NIC to the SW right from the start of the board, access of the U-Boot Ethernet driver to NIC registers would trap to EL3.
While this is a limitation of the current proof-of-concept, also adjusting the U-Boot bootloader is a pure engineering effort, which is left for future work.
Another solution to this is to directly boot into Linux after the SW is initialized, which is supported on many boards.

## License

The contents of the `docker` and `src` directories are currently licensed under GNU General Public License v2.0 (GPLv2).
Copyright: Jonas Röckl <joroec@gmx.net>

The contents of the `dep` directory are licensed under the respective licenses given in the header of the files.
