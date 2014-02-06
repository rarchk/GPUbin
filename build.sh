#works only on .cl file having only one kernel declaratinon  
gcc -o clbingenerate -w clbingenerate.c -lelf /usr/lib/libOpenCL.so -I$AMDAPPSDKROOT/include
gcc -o clbinuse -w clbinuse.c /usr/lib/libOpenCL.so -I$AMDAPPSDKROOT/include
exit 0;
