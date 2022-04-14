Three parts of a simple simulation of an operating system

1. a shell that takes UNIX commands, parses and processes them
  i) some functionality is processed within the program, some are turned into syscalls and sent to the underlying OS
  
2. a simple, mountable, file system
  i) operates on top of the underlying file system
  ii) creates a 'file store' that is managed by the program but is stored on the disk in the form of one large file
  
3. a thread scheduler
  i) creates two kernel-level threads to handle multiple user-level threads
  ii) one kernel thread for handling IO, the other for handling computation
