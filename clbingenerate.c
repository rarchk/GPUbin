/*
 * This software is Copyright (c) 2012 Sayantan Datta <std2048 at gmail dot com>
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without modification, are permitted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CL/cl.h>
#include <err.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>

#define MAXGPUS 8
#define OCL_OPTIMIZATIONS "-cl-strict-aliasing -cl-mad-enable"
#define OCL_BINARY_OPTIONS "-fno-bin-llvmir -fno-bin-source -fno-bin-exe -fbin-amdil"

char *readProgramSrc( char * filename );
void writeProgramBin( size_t , char *, char * );
void printDeviceName( size_t , cl_device_id  );
void checkErr( char * func, cl_int err );

int sizeofIL=0;

	char *
readProgramSrc( char *filename )
{
	FILE * input = NULL;
	size_t size = 0;
	char * programSrc = NULL;

	input = fopen( filename, "rb" );
	if( input == NULL )
	{	printf("Invalid input file %s\n",filename);
		exit(0);
	}
	fseek( input, 0L, SEEK_END );
	size = ftell( input );
	rewind( input );
	programSrc = (char *)malloc( size + 1 );
	fread( programSrc, sizeof(char), size, input );
	programSrc[size] = 0;
	fclose (input);
	sizeofIL=size;
	return( programSrc );
}

	int
updateBinSecSymtab(char *filename)
{	
	Elf         *elf;
	Elf_Scn     *scn = NULL;
	GElf_Shdr   shdr;
	Elf_Data    *oldData;
	int         fd, ii, count;

	if ( elf_version ( EV_CURRENT ) == EV_NONE )
		errx ( EXIT_FAILURE , " ELF library initialization "
				" failed : %s " , elf_errmsg ( -1));
	if (( fd = open ( filename , O_RDWR , 0)) < 0)
		err ( EXIT_FAILURE , " open \%s \" failed " , filename);
	if (( elf = elf_begin ( fd , ELF_C_RDWR , NULL )) == NULL )
		errx ( EXIT_FAILURE , " elf_begin () failed : % s . " ,
				elf_errmsg ( -1));
	if ( elf_kind ( elf ) != ELF_K_ELF )
		errx ( EXIT_FAILURE , " %s is not an ELF object . " ,
				filename);

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		if ( gelf_getshdr ( scn , & shdr ) != & shdr )
			errx ( EXIT_FAILURE , " getshdr () failed : %s . " ,
					elf_errmsg ( -1));
		if (shdr.sh_type == SHT_SYMTAB) {
			break;
		}
	}

	oldData=NULL;
	if((oldData = elf_getdata ( scn , oldData )) == NULL )
		errx ( EXIT_FAILURE , " getdata () failed : %s . " ,
				elf_errmsg ( -1));

	count       = shdr.sh_size / shdr.sh_entsize;

	for (ii=0; ii < count; ++ii) {

		GElf_Sym sym;

		if(gelf_getsym(oldData, ii, &sym)!=&sym)
			errx ( EXIT_FAILURE , " gelf_getsym() failed : %s . " ,
					elf_errmsg ( -1));
		if(strstr(elf_strptr(elf, shdr.sh_link, sym.st_name),"amdil")||strstr(elf_strptr(elf, shdr.sh_link, sym.st_name),"AMDIL"))
			sym.st_size=sizeofIL;
		if(gelf_update_sym(oldData,ii,&sym)==0)
			errx ( EXIT_FAILURE , " gelf_update_sym() failed : %s . " ,
					elf_errmsg ( -1));
	}

	if ( elf_update (elf , ELF_C_WRITE ) < 0)
		errx ( EXIT_FAILURE , " elf_update ( NULL ) failed : %s . " ,
				elf_errmsg ( -1));

	elf_end(elf);
	close(fd);

	return 1;
} 


	int
updateBinSecAmdil(char *binFile,char *ilFile)
{ 	
	char* IL = NULL;
	int fd,ii ;
	Elf * e ;
	char * name , *p , pc [4* sizeof ( char )];
	Elf_Scn * scn ;
	Elf_Data *newData, *oldData,data;
	GElf_Shdr shdr ;
	size_t n , shstrndx , sz ;
	IL = readProgramSrc(ilFile);
	if(IL==NULL)
		exit(0);

	if ( elf_version ( EV_CURRENT ) == EV_NONE )
		errx ( EXIT_FAILURE , " ELF library initialization "
				" failed : % s " , elf_errmsg ( -1));
	if (( fd = open ( binFile , O_RDWR , 0)) < 0)
		err ( EXIT_FAILURE , " open \%s \" failed " , binFile);
	if (( e = elf_begin ( fd , ELF_C_RDWR , NULL )) == NULL )
		errx ( EXIT_FAILURE , " elf_begin () failed : %s . " ,
				elf_errmsg ( -1));
	if ( elf_kind ( e ) != ELF_K_ELF )
		errx ( EXIT_FAILURE , " %s is not an ELF object . " ,
				binFile);
	if ( elf_getshdrstrndx (e , & shstrndx ) != 0)
		errx ( EXIT_FAILURE , " elf_getshdrstrndx () failed : %s . " ,
				elf_errmsg ( -1));	
	scn = NULL ;

	while (( scn = elf_nextscn (e , scn )) != NULL ) {

		if ( gelf_getshdr ( scn , & shdr ) != & shdr )
			errx ( EXIT_FAILURE , " getshdr () failed : %s . " ,
					elf_errmsg ( -1));
		if (( name = elf_strptr (e , shstrndx , shdr . sh_name ))
				== NULL )
			errx ( EXIT_FAILURE , " elf_strptr () failed : %s . " ,
					elf_errmsg ( -1));
		if(strstr(name,"amdil")||strstr(name,"AMDIL")) 
			break;
	}

	oldData=NULL;
	if((oldData = elf_getdata ( scn , oldData )) == NULL )
		errx ( EXIT_FAILURE , " getdata () in update_amdil() failed : %s . " ,
				elf_errmsg ( -1));

	data.d_align = oldData -> d_align;
	data.d_off=oldData->d_off;    
	data.d_type=oldData -> d_type ;
	data.d_version= oldData->d_version; 

	if((newData = elf_newdata ( scn ))==NULL)
		errx ( EXIT_FAILURE , " elf_newdata () failed : %s . " ,
				elf_errmsg ( -1));

	newData -> d_align = 0;
	newData -> d_buf = IL ;
	newData->  d_off = data.d_off;
	newData -> d_type = data.d_type ;
	newData -> d_size = sizeofIL ;
	newData -> d_version = data.d_version ;

	if ( elf_update (e , ELF_C_WRITE ) < 0)
		errx ( EXIT_FAILURE , " elf_update ( NULL ) failed : %s . " ,
				elf_errmsg ( -1));

	( void ) elf_end (e);
	( void ) close ( fd );
	return 1;
}

	int 
generateDefaultAmdBin(unsigned int dev_id, unsigned int platform_id,char *clFile,char *binFile,char *ilFile)
{
	int i = 0;
	cl_int err = CL_SUCCESS;

	char * programSrc = NULL;

	cl_int nPlatforms = 0;
	cl_platform_id *platforms = NULL;
	cl_platform_id platform = (cl_platform_id)NULL;
	cl_context_properties cprops[3];
	cl_context context;
	size_t nDevices = MAXGPUS;
	cl_device_id devices[MAXGPUS];
	cl_program program = NULL;
	size_t binary_size = 0;
	char * binary = NULL;
	char buildOptions[200];
	char opencl_log[1024*64];

	/* read in the kernel source. */
	programSrc = readProgramSrc( clFile );
	if( programSrc == NULL )
	{
		fprintf( stderr, "Unable to open %s. Exiting.\n", clFile );
		return( -1 );
	}

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
	char pbuf[100];
	err = clGetPlatformInfo( platforms[platform_id], CL_PLATFORM_VENDOR,
			sizeof(pbuf), pbuf, NULL );
	checkErr( "clGetPlatformInfo", err );
	if( strcmp(pbuf, "Advanced Micro Devices, Inc.") == 0 )
	{
		printf( "Found AMD platform\n" );
		platform = platforms[platform_id];
	}

	if( platform == (cl_context_properties)0)
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

	/* create an OpenCL program from the kernel source. */
	program = clCreateProgramWithSource( context, 1, (const char**)&programSrc,
			NULL, &err );
	checkErr( "clCreateProgramWithSource", err );

	sprintf(buildOptions,"%s %s %s",OCL_BINARY_OPTIONS ,OCL_OPTIMIZATIONS,(ilFile==NULL)?"-save-temps":"" );

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

	/* figure out the sizes of binary. */
	err = clGetProgramInfo( program, CL_PROGRAM_BINARY_SIZES,
			sizeof(size_t), &binary_size, NULL );
	checkErr( "clGetProgramInfo", err );

	if(binary_size!=0)
	{
		binary = (char *)malloc( sizeof(char)*binary_size );
	}
	else
	{ 
		printf("Invalid binary size.Abort.\n");
		exit(0);
	}

	err = clGetProgramInfo( program, CL_PROGRAM_BINARIES,
			sizeof(char*), &binary, NULL );
	checkErr( "clGetProgramInfo", err );

	/* dump out each binary into its own separate file. */
	writeProgramBin(  binary_size, binary, binFile );

	return (0);  
}


	void
writeProgramBin(  size_t binary_size, char *binary,
		char *filename )
{
	FILE *output = NULL;
	printf( "Writing out binary kernel to %s\n", filename );
	output = fopen( filename, "wb" );
	fwrite( binary, sizeof(char), binary_size, output );
	if( output == NULL )
	{
		fprintf( stderr, "Unable to open %s for write. Exiting.\n",
				filename );
		exit( -1 );
	}
	fclose( output );
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
			case CL_INVALID_PLATFORM:       fprintf (stderr, "CL_INVALID_PLATFORM"); break;
			case CL_INVALID_PROGRAM:        fprintf (stderr, "CL_INVALID_PROGRAM"); break;
			case CL_INVALID_VALUE:          fprintf (stderr, "CL_INVALID_VALUE"); break;
			case CL_OUT_OF_HOST_MEMORY:     fprintf (stderr, "CL_OUT_OF_HOST_MEMORY"); break;
			default:                        fprintf (stderr, "Unknown error code: %d", err); break;
		}
		fprintf (stderr, "\n");

		exit( err );
	}
}
int generateAmdBin(unsigned int platform_id , unsigned int dev_id,char *clFile,char *binFile,char *ilFile)
{ 
	if(ilFile==NULL)
	{
		generateDefaultAmdBin(dev_id,platform_id,clFile,binFile,NULL);
	}
	else{
		generateDefaultAmdBin(dev_id,platform_id,clFile,binFile,ilFile); 
		updateBinSecAmdil(binFile,ilFile);
		updateBinSecSymtab(binFile);
	}
	return 1;
}
int main(int argc, char** argv)
{
	if(argc<3)
	{
		printf("USAGE <./clbingenerate><SourceKernel><ILfile>\n ILFile=0 for NULL\n");
		exit(-1);
	}
	char bin[20];
	strcpy(bin,argv[1]);
	strcat(bin,".bin");
	if(strcmp(argv[2],"0")==0)
		generateAmdBin(1 ,0,argv[1],bin,NULL);
	else
		generateAmdBin(1 ,0,argv[1],bin,argv[2]);
	return 0;
}
