Some experiments into writing a FUSE filesystem in C. Creates a virtual file named ftest in the same folder as the executable which can be treated as a raw block of data, redirecting reads and writes to a block of memory in another process. The storage process runs at 30 ticks per second which slows the data transfer down such that it can be visualized in the terminal window. (depending on the defines - see FUSE_IS_PARENT and SHOW_FULL_STORAGE)

Probably full of issues. Still trying to understand the nuances of the assorted system calls etc. Comments are a bit patchy. Not exactly something to use for production... Needs libfuse3 and probably some other stuff I can't remember. Linux only.

I've used this to do various things like read/write /dev/random data, text data, and had it working as a virtual floppy disk in freedos via qemu (where it reported as 2.8 MB, probably fine.....). It's fun to watch the data move around!

Bonus: Includes my cursed Rust-style data typedefs header from my slowly growing nvstd collection of stuff I use
