## Introduction

This repository contains the artifact for the SOSP'21 paper:

*Sishuai Gong, Deniz Altınbüken, Pedro Fonseca, Petros Maniatis*, "*Snowboard: Finding Kernel Concurrency Bugs through Systematic Inter-thread Communication Analysis*", *In Proceedings of the 24th ACM Symposium on Operating Systems Principles (SOSP), Virtual, 2021*



## Getting started

### Prerequisite

- A Linux machine (Ubuntu-18.04 recommended).
- Network access (for downloading data on a public server)

### Note

- Memory & disk requirement

  The memory and disk usage of Snowboard varies depending on the size of the workload -- the number of sequential test inputs that will be analyzed. For example, analyzing one sequential input takes  0.35 GB storage space on average.

  The following instructions focus on a small workload (100 sequential test inputs) to demonstrate the usage of Snowboard. Running them takes about 16GB memory and  50GB disk on a Google cloud e2-standard-8 (8 vCPUs, 32 GB memory) machine.

  For running larger workloads, we recommend (1) using a machine that has more memory (e.g., the full workload in our experiments takes about 300GB memory) and (2) using a large disk with filesystem compression enabled (e.g., we used a 10TB disk with btrfs + compress=zstd:3).

- These instructions have been tested in Ubuntu-18.04 so we recommend using the same environment.

- These instructions only require **sudo privilege** for `scripts/setup.sh` to install dependencies.




## Build Snowboard

Run the following commands to prepare everything needed for Snowboard:

```bash
$ cd $ARTIFACT_HOME # the path of this package
$ cd scripts
# source setup.sh $SNOWBOARD_STORAGE
$ source setup.sh /storage/snowboard-tmp/
```

- Please specify `$SNOWBOARD_STORAGE`  to store all the outputs of Snowboard.

Specifically, `scripts/setup.sh` does the following:

1. Set up the necessary environment variables, which **all the other** scripts rely on;

2. Prepare dependencies and compile Snowboard hypervisor;

3. Download testsuite and artifact data. We store the data on a public server (<u>data.cs.purdue.edu</u>) instead of this repo because it exceeds Github's individual file size limit. Please find an overview of the download data in [artifact-data](docs/artifact-data.md).

   

## Run Snowboard

### Prepare Snowboard input (Optional)

The user of Snowboard needs to prepare the following:

- A set of kernel sequential tests
- A kernel to be tested
- A VM snapshot that contains a fixed state of the target kernel

This package already includes all the above so it is not necessary to create them. 



### Analyze sequential test inputs and analyze PMCs

After `source setup.sh $SNOWBOARD_STORAGE` , run the following commands to start analyzing sequential test inputs:

```bash
$ cd $artifact_package
$ cd scripts/test
# ./run-all.sh $start_id $end_id
# Profile and analyze 100 sequential inputs
$ ./run-all.sh 100 200 
```

- **Input**

  `start_id` and `end_id` together specify a range of sequential test inputs that will be analyzed. For example, `./run-all.sh 100 200` will profile the execution of sequential test ID-100, test ID-101, test ID-102, ..., and test ID-200. According to the sequential tests included in this package, any valid ranges should be within [2, 129876].

- **Output**

  A folder named `sequential-analysis-XXXX` ("XXXX" is a timestamp) will be created in `$SNOWBOARD_STORAGE`, which stores the data required for the next step.

  ```bash
  $ cd $SNOWBOARD_STORAGE/
  $ find -name "sequential-analysis-*" -type -d
  ./sequential-analysis-2021-08-20-14-16-39
  ```

  

### Generate and run concurrent tests

Snowboard supports distributed kernel concurrency testing that enables us to use multiple machines for faster testing. It achieves this by leveraging Redis and a Python library -- [Redis Queue](https://python-rq.org) for queuing and distributing concurrent inputs.

Snowboard has two types of processes for running concurrency testing: 

- **Generator**: There is only one generator machine in the network and it is responsible for continuously generating concurrent inputs under the guidance of selected clustering strategies and sending inputs to corresponding strategy queues; 
- **Worker**: a worker machine can test concurrent inputs generated under a clustering strategy by subscribing its task queue on Redis.

In practice, we run the **Generator** and **Worker** processes on different machines. However, the following instructions only run them in the same host machine due to a conservative Redis configuration. 

To actually distribute concurrent inputs across machines, please (1) properly configure Redis for network access; (2) run the generator on one host machine and launch other machines as workers.

1. Launching Redis

   By running `source setup.sh $SNOWBOARD_STORAGE`, a Redis server whose configuration is `Host: localhost`, `Port: 6380` and `Password: snowboard-testing` is already running as a daemon.

   Besides, an environment variable `$REDIS_URL` is also exported.

   

2. Run the generator

   Run the following command to launch a generator:

   ```bash
   $ cd $artifact_package
   $ cd scripts/test
   $ python3 ./generator.py $$SNOWBOARD_STORAGE/sequential-analysis-XXXX/
   The generator can generate concurrent inputs based on the following strategies:
   [1]: M-CH strategy
   [2]: M-CH-DOUBLE strategy
   [3]: M-CH-NULL strategy
   [4]: M-CH-UNALIGNED strategy
   [5]: M-INS-PAIR strategy
   [6]: M-INS strategy
   [7]: M-OBJ strategy
   Please select strategy(s) by entering the strategy ID(s) and delimiting them by a space (e.g. 1 2 3)
   
   ```

   Make your selection and it will generate concurrent inputs under selected strategies.

   

3. Inspecting the queue status

   To check the statues of all concurrent input queues, run:

   ``````bash
   $ cd $artifact_package
   $ cd scripts/test
   $ rq info --url $REDIS_URL
   double-read  |██████████ 10
   mem-addr     |██████████ 10
   channel      |██████████ 10
   ins          |███████ 7
   object-null  |██████████ 10
   unaligned-channel |██████████ 10
   ins-pair     |██████████ 10
   7 queues, 67 jobs total
   
   0 workers, 7 queues
   
   Updated: 2021-08-20 15:48:25.468313
   ``````

   

4. Run the worker

   Open another shell and `source setup.sh $SNOWBOARD_STORAGE`.

   Start to execute concurrent inputs generated by a certain strategy (e.g. INS-PAIR) by running:

   ```bash
   $ cd $ARTIFACT_HOME # the path of this package
   $ cd scripts
   $ source setup.sh /storage/snowboard-tmp/
   $ cd test
   # $ rq worker --url $REDIS_URL $QUEUE_NAME
   $ rq worker --url $REDIS_URL ins-pair
   ```

   Here is the queue name for each strategy:

   | Clustering strategy     | Redis Queue name  |
   | ----------------------- | ----------------- |
   | M-CH strategy           | channel           |
   | M-CH-DOUBLE strategy    | double-read       |
   | M-CH-NULL strategy      | object-null       |
   | M-CH-UNALIGNED strategy | unaligned-channel |
   | M-INS-PAIR strategy     | ins-pair          |
   | M-INS strategy          | ins               |
   | M-OBJ strategy          | mem-addr          |

   

5. End testing

   Simply use `ctrl+c` to kill the worker and manager. Generated concurrent inputs will be backed-up by Redis and failure tasks on the worker side will be recorded by Redis Queue.

   **Output**

   For each task (a task contains 500 concurrent inputs), a folder named `concurrent-input-XXXX` ("XXXX" is a timestamp) will be created under `$SNOWBOARD_STORAGE` to store execution results.

   ```bash
   $ cd $SNOWBOARD_STORAGE/
   $ find -name "concurrent-test-*" -type -d
   ./concurrent-test-2021-08-20-15-06-31
   ```

   

### 4. Inspecting concurrency testing results

The occurrence of a bug can be checked either in the guest kernel console history or the data race detection result.

- Guest kernel console output

  ```bash
  $ cd $SNOWBOARD_STORAGE/concurrent-test-2021-08-20-15-06-31
  $ find -name "console*" | xargs cat
  ```

  Running the above commands will display all the console output (if any) collected from the guest kernel.

- Data race detection result

  Detection result can be found by running:

  ```bash
  $ cd $SNOWBOARD_STORAGE/concurrent-test-2021-08-20-15-06-31
  $ find -name "*race_detect*"
  ./20210820_150904_forkall_race_detector.txt
  ```

  We provide a script to analyze the detection result by showing the kernel code of the two data-racing instructions.

  ```bash
  # We denote the full path of  as $detection_result
  $ cd $artifact_package
  $ cd scripts/analysis
  $ ./data-race.sh $SNOWBOARD_STORAGE/concurrent-test-XXXX
  ```

  **Output**

  The output file is stored back to `$SNOWBOARD_STORAGE/concurrent-test-XXXX`  and we can find it by:

  ```bash
  $ cd $SNOWBOARD_STORAGE/concurrent-test-2021-08-20-15-06-31
  $ find -name "*race_detection*source"
  ./20210820_150904_forkall_race_detector.txt.source
  ```

  

## Validate results

### Reproducing concurrency bugs

Reproducing a concurrency bug found by Snowboard entails re-executing specific concurrent input and inspecting the test results.

This package provides a script to reproduce concurrency bugs given concurrent inputs, as explained below:

1. Re-run failure-inducing concurrent inputs

   ```bash
   $ cd scripts/reproduce
   $ ./reproduce.py $concurrent_input
   ```

   **Output**

   The above command will execute Snowboard on the concurrent input and the execution result will be stored in `SNOWBOARD_STORAGE/reproduce-XXXX`.

2. Checking the occurrence of bugs

   Please follow the instruction in Section: Inspecting-concurrency-testing-results.

   


### Patches for fixed concurrency bugs

`Table 1` in the paper lists the testing results by Snowboard and below is a list of concurrency bugs which (1) were found by Snowboard and (2) are already fixed. 

| ID   | Summary                                                      | Link                                                         |
| ---- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| #1   | BUG: unable to handle page fault for address                 | https://github.com/torvalds/linux/commit/1748f6a2cbc4694523f16da1c892b59861045b9d |
| #9   | Data race: dev_ifsioc_locked() / eth_commit_mac_addr_change() | https://github.com/torvalds/linux/commit/3b23a32a63219f51a5298bc55a65ecee866e79d0 |
| #12  | BUG: kernel NULL pointer dereference                         | https://github.com/torvalds/linux/commit/69e16d01d1de4f1249869de342915f608feb55d5 |
| #15  | Data race: snd_ctl_elem_add()                                | https://patches.linaro.org/patch/421808/                     |
| #17  | Data race: fanout_demux_rollover() / __fanout_unlink()       | https://github.com/torvalds/linux/commit/94f633ea8ade8418634d152ad0931133338226f6 |



### Measuring PMC cluster size

This package provides the PMC cluster data we collected in `data/pmc/`.

In `paper:Table 2`, we list the number of unique clusters under each clustering strategy (Column "Exemplar PMCs"). Those numbers can be validated by running `wc -l ` on the cluster file, as shown below:

```bash
$ cd data/pmc
$ ll
total 9.8G
drwxrwxr-x 2 sishuai sishuai 4.0K Aug 20 02:17 ./
drwxrwxr-x 3 sishuai sishuai 4.0K Aug 20 16:38 ../
-rw-r--r-- 1 sishuai sishuai 6.8G Aug 20 02:05 mem-dict-2021-03-27-14-47-44
-rw-rw-r-- 1 sishuai sishuai 1.8G Aug 19 12:47 uncommon-channel.txt
-rw-rw-r-- 1 sishuai sishuai 138M Aug 19 12:45 uncommon-double-read-channel.txt
-rw-rw-r-- 1 sishuai sishuai  19M Aug 19 12:47 uncommon-ins-pair.txt
-rw-rw-r-- 1 sishuai sishuai 856K Aug 19 12:45 uncommon-ins.txt
-rw-rw-r-- 1 sishuai sishuai  78M Aug 19 12:45 uncommon-mem-addr.txt
-rw-rw-r-- 1 sishuai sishuai 369M Aug 19 12:45 uncommon-object-null-channel.txt
-rw-rw-r-- 1 sishuai sishuai 673M Aug 19 12:44 uncommon-unaligned-channel.txt
$ wc -l uncommon-ins-pair
738467 uncommon-ins-pair.txt
```

More information about the format of PMC data is in [artifact-data](docs/artifact-data.md).

### Analyze PMCs & PMC clusters

To understand a PMC or a PMC cluster, the kernel source code of the writer and reader will be very helpful.

The following script outputs the source code of every writer and reader we identified in Linux 5.12-rc3.

```bash
$ cd scripts/analysis
$ ./ins-pair-analysis.py ../../data/pmc/uncommon-ins-pair
```



## License

Code of Snowboard hypervisor (`vmm/src`) in this repository is licensed under the GLPv2 license (`vmm/src/LICENSE`).The rest of Snowboard implementation in `scripts/` is under Apache 2 license (`scripts/LICENSE`).

