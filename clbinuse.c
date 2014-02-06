/*
 * This software is Copyright (c) 2012 Sayantan Datta <std2048 at gmail dot com>
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without modification, are permitted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CL/cl.h>

#define MAX_THREADS 100

#define MAXGPUS 8
#define OCL_OPTIMIZATIONS "-cl-strict-aliasing -cl-mad-enable"
#define OCL_BINARY_OPTIONS "-fno-bin-llvmir -fno-bin-source -fno-bin-exe -fbin-amdil"

cl_device_id askWhichDevice( size_t nDevices, cl_device_id *devices );
void printDeviceName( size_t , cl_device_id  );
char *readKernelBin( size_t *  ,char *);
void checkErr( char * func, cl_int err );


	int
buildProgramFromAmdBin(unsigned int platform_id,unsigned int dev_id,char *binFile)
{
	int i = 0;
	cl_int err = CL_SUCCESS;

	cl_int nPlatforms = 0;
	cl_platform_id *platforms = NULL;
	cl_platform_id platform = (cl_platform_id)NULL;
	cl_context_properties cprops[3];
	cl_context context;
	size_t nDevices = 0;
	cl_device_id devices[MAXGPUS];
	cl_device_id device_id = 0;
	size_t binary_size = 0;
	char * binary = NULL;
	cl_program program = NULL;
	char pbuf[100];
	cl_command_queue cmdq;
	cl_mem iBuf,oBuf;
	cl_kernel kernel;
	cl_int *inBuf,*outBuf;
	inBuf=(cl_int*)malloc(MAX_THREADS*sizeof(cl_int));
	outBuf=(cl_int*)malloc(MAX_THREADS*sizeof(cl_int));
	size_t N=MAX_THREADS;
	cl_event evnt;
	char buildOptions[200];
	char opencl_log[1024*64];

	/* figure out the number of platforms on this system. */
	err = clGetPlatformIDs( 0, NULL, &nPlatforms );
	checkErr( "clGetPlatformIDs", err );
	printf( "Number of platforms found: %d\n", nPlatforms );
	if( nPlatforms == 0 )
	{
		fprintf( stderr, "Cannot continue without any platforms. Exiting.\n" );
		return( -1 );
	}
	platforms = (cl_platform_id *)malloc( sizeof(cl_platform_id) * nPlatforms );
	err = clGetPlatformIDs( nPlatforms, platforms, NULL );
	checkErr( "clGetPlatformIDs", err );

	/* Check for AMD platform. */

	err = clGetPlatformInfo( platforms[platform_id], CL_PLATFORM_VENDOR,
			sizeof(pbuf), pbuf, NULL );
	checkErr( "clGetPlatformInfo", err );
	if( strcmp(pbuf, "Advanced Micro Devices, Inc.") == 0 )
	{
		printf( "Found AMD platform\n" );
		platform = platforms[platform_id];

	}

	if( platform == (cl_context_properties)0 )
	{
		fprintf( stderr, "Could not find an AMD platform. Exiting.\n" );
		exit(0);
	}

	clGetDeviceIDs(platform,
			CL_DEVICE_TYPE_ALL,MAXGPUS, devices, &nDevices);

	cprops[0] = CL_CONTEXT_PLATFORM;
	cprops[1] = (cl_context_properties)platform;
	cprops[2] = (cl_context_properties)NULL; 

	context =   clCreateContext(cprops, 1, &devices[dev_id], NULL, NULL,
			&err);
	checkErr( "clCreateContext", err );

	printDeviceName(dev_id,devices[dev_id]);

	/* read in the binary kernel. */
	binary = readKernelBin( &binary_size, binFile );

	/* create an OpenCL program from the binary kernel. */
	program = clCreateProgramWithBinary( context, 1, &devices[dev_id], &binary_size,
			(const unsigned char**)&binary, NULL, &err );
	checkErr( "clCreateProgramWithBinary", err );

	sprintf(buildOptions,"%s %s",OCL_BINARY_OPTIONS ,OCL_OPTIMIZATIONS);

	/* build the kernel source for all available devices in the context. */
	err = clBuildProgram( program, 0, NULL,buildOptions , NULL, NULL );

	checkErr("clGetProgramBuildInfo",clGetProgramBuildInfo(program, devices[dev_id],
				CL_PROGRAM_BUILD_LOG, sizeof(opencl_log), (void *) opencl_log,
				NULL));

	/*Report build errors and warnings*/
	if (err != CL_SUCCESS)
	{	fprintf(stderr, "Compilation log: %s\n", opencl_log);
		exit(0);
	}  
#ifdef REPORT_OPENCL_WARNINGS
	else if (strlen(opencl_log) > 1)	
		fprintf(stderr, "Compilation log: %s\n", opencl_log);
#endif    


	/* IT IS APPLICATION-DEPENDENT WHAT TO DO AFTER THIS POINT. */
	printf( "\n*** REPLACE THIS WITH ACTUAL WORK ***\n" );

	for(i=0;i<MAX_THREADS;i++)
		inBuf[i]=i;

	kernel=clCreateKernel(program,"test",&err) ;

	if(err) {printf("Create Kernel test FAILED\n"); return 0;}

	cmdq=clCreateCommandQueue(context, devices[dev_id], CL_QUEUE_PROFILING_ENABLE,&err);
	checkErr("Create CMDQ FAILED\n",err);

	iBuf=clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,MAX_THREADS*sizeof(cl_int),inBuf,&err);
	if((iBuf==(cl_mem)0)) { checkErr("Create Buffer FAILED\n",err); }

	oBuf=clCreateBuffer(context,CL_MEM_WRITE_ONLY,MAX_THREADS*sizeof(cl_int),NULL,&err);
	if((oBuf==(cl_mem)0)) {checkErr("Create Buffer FAILED\n",err);  }


	checkErr("Set Kernel Arg FAILED arg0\n",clSetKernelArg(kernel,0,sizeof(cl_mem),&iBuf));

	checkErr("Set Kernel Arg FAILED arg1\n",clSetKernelArg(kernel,1,sizeof(cl_mem),&oBuf)); 

	err=clEnqueueNDRangeKernel(cmdq,kernel,1,NULL,&N,NULL,0,NULL,&evnt);

	clWaitForEvents(1,&evnt);

	checkErr("Write FAILED\n",clEnqueueReadBuffer(cmdq,oBuf,CL_TRUE,0,MAX_THREADS*sizeof(cl_uint),outBuf, 0, NULL, NULL));

	for(i=0;i<MAX_THREADS;i++)
		printf("%d\n",outBuf[i]);

	return (0);
}

	char *
readKernelBin( size_t * binary_size ,char *filename)
{
	int i = 0;
	int filename_size = 0;
	FILE * input = NULL;
	size_t size = 0;
	char * binary = NULL;



	printf( "Reading in binary kernel from %s\n", filename );

	input = fopen( filename, "rb" );
	if( input == NULL )
	{
		fprintf( stderr, "Unable to open %s for reading.\n", filename );

	}

	fseek( input, 0L, SEEK_END );    
	size = ftell( input );
	rewind( input );
	binary = (char *)malloc( size );
	fread( binary, sizeof(char), size, input );
	fclose( input );

	if( binary_size != NULL )
	{
		*binary_size = size;
	}

	return( binary );

}

	void
printDeviceName( size_t dev_id, cl_device_id device )
{
	int i = 0;
	cl_int err = CL_SUCCESS;

	cl_device_type devType;
	char pbuf[100];

	printf( "DEVICE[%d]: ", dev_id );

	err = clGetDeviceInfo( device, CL_DEVICE_TYPE, sizeof(devType),
			&devType, NULL );
	checkErr( "clGetDeviceInfo", err );
	switch( devType )
	{
		case CL_DEVICE_TYPE_CPU: printf( "CL_DEVICE_TYPE_CPU, " ); break;
		case CL_DEVICE_TYPE_GPU: printf( "CL_DEVICE_TYPE_GPU, " ); break;
		default:                 printf( "Unknown device type %d, ", devType );
					 break;
	}

	err = clGetDeviceInfo( device, CL_DEVICE_NAME, sizeof(pbuf),
			pbuf, NULL );
	checkErr( "clGetDeviceINfo", err );
	printf( "Name=%s\n", pbuf );

}

	void
checkErr( char *func, cl_int err )
{
	if( err != CL_SUCCESS )
	{
		fprintf( stderr, "%s(): ", func );
		switch( err )
		{
			case CL_BUILD_PROGRAM_FAILURE:  fprintf (stderr, "CL_BUILD_PROGRAM_FAILURE"); break;
			case CL_COMPILER_NOT_AVAILABLE: fprintf (stderr, "CL_COMPILER_NOT_AVAILABLE"); break;
			case CL_DEVICE_NOT_AVAILABLE:   fprintf (stderr, "CL_DEVICE_NOT_AVAILABLE"); break;
			case CL_DEVICE_NOT_FOUND:       fprintf (stderr, "CL_DEVICE_NOT_FOUND"); break;
			case CL_INVALID_BINARY:         fprintf (stderr, "CL_INVALID_BINARY"); break;
			case CL_INVALID_BUILD_OPTIONS:  fprintf (stderr, "CL_INVALID_BUILD_OPTIONS"); break;
			case CL_INVALID_CONTEXT:        fprintf (stderr, "CL_INVALID_CONTEXT"); break;
			case CL_INVALID_DEVICE:         fprintf (stderr, "CL_INVALID_DEVICE"); break;
			case CL_INVALID_DEVICE_TYPE:    fprintf (stderr, "CL_INVALID_DEVICE_TYPE"); break;
			case CL_INVALID_OPERATION:      fprintf (stderr, "CL_INVALID_OPERATION"); break;
			case CL_INVALID_PLATFORM:        fprintf (stderr, "CL_INVALID_PLATFORM"); break;
			case CL_INVALID_PROGRAM:        fprintf (stderr, "CL_INVALID_PROGRAM"); break;
			case CL_INVALID_VALUE:          fprintf (stderr, "CL_INVALID_VALUE"); break;
			case CL_OUT_OF_HOST_MEMORY:     fprintf (stderr, "CL_OUT_OF_HOST_MEMORY"); break;
			default:                        fprintf (stderr, "Unknown error code: %d", (int)err); break;
		}
		fprintf (stderr, "\n");

		exit( err );
	}
}
void main(int argc, char **argv)
{
	if(argc<2)
	{
		printf("USAGE <./clbinuse><KernelBin>\n");
		exit(-1);
	}


	buildProgramFromAmdBin(1,0,argv[1]);
}

