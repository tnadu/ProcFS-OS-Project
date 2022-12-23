# Operating Systems Project - ProcFS
## General Description
ProcFS is a FUSE-based virtual file system, which aims to provide a
snaphot of the running processes at mount-time, on the system on which
it is mounted. Unlike /proc, the process tree structure is maintained
and follows three basic rules:
- each directory represents a process and its name is the PID
of said process
- every directory contains a 'stats' file, in which relevant
data about the status of the corresponding process at mount-time is
stored
- the children processes of any given process are represented
as direct subdirectories (named as described above) of the parent 
directory associated with the given process

## Development Process
No further development will occur and the repository will not be
maintained in any way.

## Notable Dependencies
- libfuse-dev
	- in order to compile the source file, the fuse header must
be available on the target system
	- this project adheres to the fuse operation specification
of libfuse3 and is compatible with `v3.10` of said specification,
provided via the macro `FUSE_USE_VERSION`
	- on debian-based distributions, this package can be 
installed via the apt package manager, with the following command: 
`sudo apt install libfuse3-dev`

- fuse and libfuse
	- necessary in order to be able to actually run fuse-based
file systems
	- on debian-based ditributions, these packages are part
of the official package repository and come pre-installed
	- if, for any reason, they aren't already installed on your
system, they can be installed via the apt package manager, with the 
following command: `sudo apt install fuse3 libfuse3-3 -y`

## Building and Running
ProcFS can be compiled with gcc, using the following command, which
specifies the correct fuse header version to be used:
```gcc -Wall proc-fs.c `pkg-config fuse3 --cflags --libs\` -o proc-fs```

The file system can then be mounted by running the compiled executable
with the desired mount-point as an argument:
`./proc-fs /<path>/<to>/<desired>/<mount-point>`

In order to unmount the file system, the following command will be used:
`fusermount -u /<path>/<to>/<desired>/<mount-point>`

## Project Contributors
- playback0022
- Iradu15
- VladWero08
