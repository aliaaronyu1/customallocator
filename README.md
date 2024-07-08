# Project 3: Memory Allocator

See: https://www.cs.usfca.edu/~mmalensek/cs326/assignments/project-3.html 

## Testing

To execute the test cases, use `make test`. To pull in updated test cases, run `make testupdate`. You can also run a specific test case instead of all of them:

```
# Run all test cases:
make test

# Run a specific test case:
make test run=4

# Run a few specific test cases (4, 8, and 12 in this case):
make test run='4 8 12'
```
## About

This program is a custom memory allocator that redefines how malloc() is implemented. This program also implements calloc and realloc as well.

## How It Works

 Instead of using the c library malloc, `we can use LD_PRELOAD` command to run our custom allocator instead. Just type:
 ```bash
make
LD_PRELOAD=$(pwd)/allocator.so ls /
```
(in this example, the command `ls /` is run with the custom memory allocator instead of the default).

## User Requests Memory

There are many moving parts in this custom allocator. Malloc is not implemented how an average user may think. 
1. Malloc requests memory from the kernel using mmap()
2. Malloc requests an entire region of memory rather then the requested size from user
3. region size is dependent on the number of pages which is dependent on the requested size. `num_pages * page_size`
4. After every request, the program will check the size requested and determine if we should reuse a free block in the region
`reuse(size_t size)`

## Memory is Split

1. then it will create a doubly linked list where this newly free memory is split by split_block()
2. The size of the split is equivalent to the alligned size from user requested size
3. As a result, after many allocations, there will be many blocks of memory in the region(s) that are implemented as a doubly linked list

## Free Memory

1. Once freeing a memory block is requested, it will set the free member variable in the struct to true.
2. Then it will check ajacent blocks to see if they are free as well and merge_block() with them.
3. Once all memory is free'd, the linked list will be one big merged block ready to be unmaped.
