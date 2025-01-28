# Checking for Memory Leaks with *valgrind*

A number of the classes in the system are standalone and do not require access to the ESP32 hardware.  These classes can be tested in a desktop application meaning we can use standard desktop tools such and *valgrind* to analyse the code.  These tools are made available through a docker container.

## Dockerfile

A docker file has been provided as *valgrind* does not run on all operating systems.  The docker image can be built with the command:

```bash
docker build -t ubuntu/devtools:latest .
```

Once built, the docker image should be run in interactive mode with the command:

```bash
docker run --rm -tiv $PWD/..:/project ubuntu/devtools:latest
```

## runvalgrind.sh

This script runs the application using *valgrind* producing a summary report in the file valgrind.txt.  This analysis can be run by opening a terminal and  navigating to the *Desktop* directory and running the following commands:

```bash
make clean
make
./runvalgrind.sh
```
