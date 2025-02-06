# read_alias

A kernel module that allows direct read/write access to physical memory. See `./include/` for the user space API. We build a static lib to make this available to other tools.

See `common-code` for loading/storing and calculating aliased addresses.

If you want to build the kernel module for a kernel different from the currently running one, set the `KERNEL_PATH` environment variable to the header files of the targeted kernel.