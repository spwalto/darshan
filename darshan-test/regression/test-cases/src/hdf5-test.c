/************************************************************

  This example shows how to create a chunked dataset.  The
  program first writes integers in a hyperslab selection to
  a chunked dataset with dataspace dimensions of DIM0xDIM1
  and chunk size of CHUNK0xCHUNK1, then closes the file.
  Next, it reopens the file, reads back the data, and
  outputs it to the screen.  Finally it reads the data again
  using a different hyperslab selection, and outputs
  the result to the screen.

  This file is intended for use with HDF5 Library version 1.8

 ************************************************************/

#include "hdf5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <mpi.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILE            "/lscratch/spwalto/darshan-output/h5ex_d_chunk.h5"
#define DATASET         "DS1"
#define DIM0            6
#define DIM1            8
#define CHUNK0          4
#define CHUNK1          4

/* function prototypes */
static int parse_args(int argc, char **argv);
static void usage(void);

/* global vars */
static int mynod = 0;
static int nprocs = 1;

int
main (int argc, char **argv)
{

   int namelen;
   char processor_name[MPI_MAX_PROCESSOR_NAME];

/* startup MPI and determine the rank of this process */
   MPI_Init(&argc,&argv);
   MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
   MPI_Comm_rank(MPI_COMM_WORLD, &mynod);
   MPI_Get_processor_name(processor_name, &namelen);

    hid_t           file, space, dset, dcpl;    /* Handles */
    herr_t          status;
    H5D_layout_t    layout;
    hsize_t         dims[2] = {DIM0, DIM1},
                    chunk[2] = {CHUNK0, CHUNK1},
                    start[2],
                    stride[2],
                    count[2],
                    block[2];
    int             wdata[DIM0][DIM1],          /* Write buffer */
                    rdata[DIM0][DIM1],          /* Read buffer */
                    i, j;

	//H5Fset_mpi_atomicity(file, 1);

    /*
     * Initialize data to "1", to make it easier to see the selections.
     */
    for (i=0; i<DIM0; i++)
        for (j=0; j<DIM1; j++)
            wdata[i][j] = 1;

    /*
     * Print the data to the screen.
     */
    printf ("Original Data:\n");
    for (i=0; i<DIM0; i++) {
        printf (" [");
        for (j=0; j<DIM1; j++)
            printf (" %3d", wdata[i][j]);
        printf ("]\n");
    }

    /*
     * Create a new file using the default properties.
     */
    file = H5Fcreate(FILE, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

    /*
     * Create dataspace.  Setting maximum size to NULL sets the maximum
     * size to be the current size.
     */
    space = H5Screate_simple(2, dims, NULL);

    /*
     * Create the dataset creation property list, and set the chunk
     * size.
     */
    dcpl = H5Pcreate(H5P_DATASET_CREATE);
    status = H5Pset_chunk(dcpl, 2, chunk);

    /*
     * Create the chunked dataset.
     */
    dset = H5Dcreate (file, DATASET, H5T_STD_I32LE, space, H5P_DEFAULT, dcpl,
                H5P_DEFAULT);

    /*
     * Define and select the first part of the hyperslab selection.
     */
    start[0] = 0;
    start[1] = 0;
    stride[0] = 3;
    stride[1] = 3;
    count[0] = 2;
    count[1] = 3;
    block[0] = 2;
    block[1] = 2;
    status = H5Sselect_hyperslab (space, H5S_SELECT_SET, start, stride, count,
                block);

    /*
     * Define and select the second part of the hyperslab selection,
     * which is subtracted from the first selection by the use of
     * H5S_SELECT_NOTB
     */
    block[0] = 1;
    block[1] = 1;
    status = H5Sselect_hyperslab (space, H5S_SELECT_NOTB, start, stride, count,
                block);

    /*
     * Write the data to the dataset.
     */
    status = H5Dwrite (dset, H5T_NATIVE_INT, H5S_ALL, space, H5P_DEFAULT,
                wdata[0]);

    /*
     * Close and release resources.
     */
    status = H5Pclose (dcpl);
    status = H5Dclose (dset);
    status = H5Sclose (space);
    status = H5Fclose (file);


    /*
     * Now we begin the read section of this example.
     */

    /*
     * Open file and dataset using the default properties.
     */
    file = H5Fopen (FILE, H5F_ACC_RDONLY, H5P_DEFAULT);
    dset = H5Dopen (file, DATASET, H5P_DEFAULT);

    /*
     * Retrieve the dataset creation property list, and print the
     * storage layout.
     */
    dcpl = H5Dget_create_plist (dset);
    layout = H5Pget_layout (dcpl);
    printf ("\nStorage layout for %s is: ", DATASET);
    switch (layout) {
        case H5D_COMPACT:
            printf ("H5D_COMPACT\n");
            break;
        case H5D_CONTIGUOUS:
            printf ("H5D_CONTIGUOUS\n");
            break;
        case H5D_CHUNKED:
            printf ("H5D_CHUNKED\n");
    }

    /*
     * Read the data using the default properties.
     */
    status = H5Dread(dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                rdata[0]);

    /*
     * Output the data to the screen.
     */
    printf ("\nData as written to disk by hyberslabs:\n");
    for (i=0; i<DIM0; i++) {
        printf (" [");
        for (j=0; j<DIM1; j++)
            printf (" %3d", rdata[i][j]);
        printf ("]\n");
    }

    /*
     * Initialize the read array.
     */
    for (i=0; i<DIM0; i++)
        for (j=0; j<DIM1; j++)
            rdata[i][j] = 0;

    /*
     * Define and select the hyperslab to use for reading.
     */
    space = H5Dget_space(dset);
    start[0] = 0;
    start[1] = 1;
    stride[0] = 4;
    stride[1] = 4;
    count[0] = 2;
    count[1] = 2;
    block[0] = 2;
    block[1] = 3;
    status = H5Sselect_hyperslab(space, H5S_SELECT_SET, start, stride, count, block);

    /*
     * Read the data using the previously defined hyperslab.
     */
    status = H5Dread(dset, H5T_NATIVE_INT, H5S_ALL, space, H5P_DEFAULT,
                rdata[0]);

    /*
     * Output the data to the screen.
     */
    printf ("\nData as read from disk by hyperslab:\n");
    for (i=0; i<DIM0; i++) {
        printf (" [");
        for (j=0; j<DIM1; j++)
            printf (" %3d", rdata[i][j]);
        printf ("]\n");
    }

    /*
     * Close and release resources.
     */
    status = H5Pclose(dcpl);
    status = H5Dclose(dset);
    status = H5Sclose(space);
    status = H5Fclose(file);

    MPI_Finalize();
    return 0;
}

static int parse_args(int argc, char **argv)
{
   int c;

   while ((c = getopt(argc, argv, "f:")) != EOF) {
      switch (c) {
         case 'f': /* filename */
            strncpy(FILE, optarg, 255);
            break;
         case '?': /* unknown */
            if (mynod == 0)
                usage();
            exit(1);
         default:
            break;
      }
   }
   return(0);
}

static void usage(void)
{
    printf("Usage: stdio-test [<OPTIONS>...]\n");
    printf("\n<OPTIONS> is one of\n");
    printf(" -f       filename [default: h5ex_d_chunk.h5]\n");
    printf(" -h       print this help\n");
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 *
 * vim: ts=3
 * End:
 */
