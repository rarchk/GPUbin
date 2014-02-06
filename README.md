GPUbin
======

Low Level Tools for GPU programming

---
__AMD Low level kernel API__ 

Following library provides tools for manipulate AMD opencl kernels at intermediate language level(.il), and generating binaries from them. 

**il** is an intermediate language for amd gpus > 7series. Further, an attemp has been made, to optimize with a further lower level language **isa**

---
Code Index:
* build.sh      -- builds necessary tools 
* clbingenerate -- generates a binary file of a kernel function from its source
* clbinuse      -- executes binary on platform  with device id.
* exec.py       -- runs on a kernel, and generate corresponding binaries, and amd il|isa codes

