/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdf.ncsa.uiuc.edu/HDF5/doc/Copyright.html.  If you do not have     *
 * access to either file, you may request a copy from hdfhelp@ncsa.uiuc.edu. *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "H5PTprivate.h"
#include "H5TBprivate.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/*-------------------------------------------------------------------------
 * Packet Table API test
 *
 *-------------------------------------------------------------------------
 */

#define NRECORDS 8
#define BIG_TABLE_SIZE  8000
#define NFIELDS 5
#define TEST_FILE_NAME "test_packet_table.h5"
#define PT_NAME "Test Packet Table"
#define VL_TABLE_NAME "Varlen Test Table"
#define H5TB_TABLE_NAME "Table1"
#define TESTING(WHAT)   {printf("%-70s", "Testing " WHAT); fflush(stdout);}
#define PASSED()        {puts(" PASSED");fflush(stdout);}
#define H5_FAILED()     {puts("*FAILED*");fflush(stdout);}

/*-------------------------------------------------------------------------
 * structure used for some tests, a particle
 *-------------------------------------------------------------------------
 */
typedef struct particle_t
{
 char   name[16];
 int    lati;
 int    longi;
 float  pressure;
 double temperature;
} particle_t;

/*-------------------------------------------------------------------------
 * a static array of particles for writing and checking reads
 *-------------------------------------------------------------------------
 */
static particle_t testPart[NRECORDS] = {
    {"zero", 0,0, 0.0f, 0.0},
    {"one",  10,10, 1.0f, 10.0},
    {"two",  20,20, 2.0f, 20.0},
    {"three",30,30, 3.0f, 30.0},
    {"four", 40,40, 4.0f, 40.0},
    {"five", 50,50, 5.0f, 50.0},
    {"six",  60,60, 6.0f, 60.0},
    {"seven",70,70, 7.0f, 70.0}
  };

/*-------------------------------------------------------------------------
 * function that compares one particle
 *-------------------------------------------------------------------------
 */
static int cmp_par(hsize_t i, hsize_t j, particle_t *rbuf, particle_t *wbuf )
{
 if ( ( strcmp( rbuf[i].name, wbuf[j].name ) != 0 ) ||
  rbuf[i].lati != wbuf[j].lati ||
  rbuf[i].longi != wbuf[j].longi ||
  rbuf[i].pressure != wbuf[j].pressure ||
  rbuf[i].temperature != wbuf[j].temperature ) {
  return -1;
 }
 return 0;
}

/*-------------------------------------------------------------------------
 * function to create a datatype representing the particle struct
 *-------------------------------------------------------------------------
 */
static hid_t
make_particle_type(void)
{
 hid_t type_id;
 hid_t string_type;
 size_t type_size = sizeof(particle_t);

 /* Create the memory data type. */
 if ((type_id = H5Tcreate (H5T_COMPOUND, type_size )) < 0 )
  return -1;

 /* Insert fields. */
 string_type = H5Tcopy( H5T_C_S1 );
 H5Tset_size( string_type, 16 );

 if ( H5Tinsert(type_id, "Name", HOFFSET(particle_t, name) , string_type ) < 0 )
     return -1;
 if ( H5Tinsert(type_id, "Lat", HOFFSET(particle_t, lati) , H5T_NATIVE_INT ) < 0 )
     return -1;
 if ( H5Tinsert(type_id, "Long", HOFFSET(particle_t, longi) , H5T_NATIVE_INT ) < 0 )
     return -1;
 if ( H5Tinsert(type_id, "Pressure", HOFFSET(particle_t, pressure) , H5T_NATIVE_FLOAT ) < 0 )
     return -1;
 if ( H5Tinsert(type_id, "Temperature", HOFFSET(particle_t, temperature) , H5T_NATIVE_DOUBLE ) < 0 )
     return -1;

 return type_id;
}

    /* Create a normal HL table just like the HL examples do */
static int create_hl_table(hid_t fid)
{
  /* Calculate the offsets of the particle struct members in memory */
  size_t part_offset[NFIELDS] = { HOFFSET( particle_t, name ),
                                  HOFFSET( particle_t, lati ),
                                  HOFFSET( particle_t, longi ),
                                  HOFFSET( particle_t, pressure ),
                                  HOFFSET( particle_t, temperature )};

    /* Define field information */
    const char *field_names[NFIELDS]  =
		  { "Name","Latitude", "Longitude", "Pressure", "Temperature" };
    hid_t      field_type[NFIELDS];
    hid_t      string_type;
    hsize_t    chunk_size = 10;
    int        *fill_data = NULL;
    int        compress  = 0;
    herr_t     status;

    /* Initialize the field field_type */
    string_type = H5Tcopy( H5T_C_S1 );
    H5Tset_size( string_type, 16 );
    field_type[0] = string_type;
    field_type[1] = H5T_NATIVE_INT;
    field_type[2] = H5T_NATIVE_INT;
    field_type[3] = H5T_NATIVE_FLOAT;
    field_type[4] = H5T_NATIVE_DOUBLE;


  /*------------------------------------------------------------------------
  * H5TBmake_table
  *-------------------------------------------------------------------------
  */

  status=H5TBmake_table( "Table Title", fid, H5TB_TABLE_NAME, (hsize_t) NFIELDS,
                        (hsize_t)NRECORDS, sizeof(particle_t),
                        field_names, part_offset, field_type,
                        chunk_size, fill_data, compress, testPart  );

if(status<0)
  return -1;
else
  return 0;
}



/*-------------------------------------------------------------------------
 * test_create_close
 *
 * Tests creation and closing of an FL packet table
 *
 *-------------------------------------------------------------------------
 */

int test_create_close(hid_t fid)
{
    herr_t err;
    hid_t table;
    hid_t part_t;

    TESTING("H5PTcreate_fl and H5PTclose");

    /* Create a datatype for the particle struct */
    part_t = make_particle_type();

    assert(part_t != -1);

    /* Create the table */
    table = H5PTcreate_fl(fid, PT_NAME, part_t, 100);
    H5Tclose(part_t);
    if( H5PTis_valid(table) < 0)
      goto out;
    if( H5PTis_varlen(table) != 0)
      goto out;

    /* Close the table */
    err = H5PTclose(table);
    if( err < 0)
      goto out;

    PASSED();
    return 0;

    out:
      H5_FAILED();
      return -1;
}

/*-------------------------------------------------------------------------
 * test_open
 *
 * Tests opening and closing a FL packet table
 *
 *-------------------------------------------------------------------------
 */
int test_open(hid_t fid)
{
    herr_t err;
    hid_t table;

    TESTING("H5PTopen");

    /* Open the table */
    table = H5PTopen(fid, PT_NAME);
    if( H5PTis_valid(table) < 0)
      goto out;
    if( H5PTis_varlen(table) != 0)
      goto out;

    /* Close the table */
    err = H5PTclose(table);
    if( err < 0)
      goto out;

    PASSED();
    return 0;

    out:
      H5_FAILED();
      return -1;
}

/*-------------------------------------------------------------------------
 * test_append
 *
 * Tests appending packets to a FL packet table
 *
 *-------------------------------------------------------------------------
 */
int test_append(hid_t fid)
{
    herr_t err;
    hid_t table;
    hsize_t count;

    TESTING("H5PTappend");

    /* Open the table */
    table = H5PTopen(fid, PT_NAME);
    if( H5PTis_valid(table) < 0)
        goto out;

    /* Count the number of packets in the table  */
    err = H5PTget_num_packets(table, &count);
    if( err < 0)
        goto out;
    /* There should be 0 records in the table */
    if( count != 0 )
        goto out;

    /* Append one particle */
    err = H5PTappend(table, 1, &(testPart[0]));
    if( err < 0)
        goto out;

    /* Append several particles */
    err = H5PTappend(table, 6, &(testPart[1]));
    if( err < 0)
        goto out;

    /* Append one more particle */
    err = H5PTappend(table, 1, &(testPart[7]));
    if( err < 0)
        goto out;

    /* Count the number of packets in the table  */
    err = H5PTget_num_packets(table, &count);
    if( err < 0)
        goto out;
    /* There should be 8 records in the table now */
    if( count != 8 )
        goto out;

    /* Close the table */
    err = H5PTclose(table);
    if( err < 0)
        goto out;

    PASSED();
    return 0;

    out:
        H5_FAILED();
        if( H5PTis_valid(table) < 0)
            H5PTclose(table);
        return -1;
}

/*-------------------------------------------------------------------------
 * test_read
 *
 * Tests that the packets appended by test_append can be read back.
 *
 *-------------------------------------------------------------------------
 */
int test_read(hid_t fid)
{
    herr_t err;
    hid_t table;
    particle_t readBuf[NRECORDS];
    int c;

    TESTING("H5PTread_packets");

    /* Open the table */
    table = H5PTopen(fid, PT_NAME);
    if( H5PTis_valid(table) < 0)
        goto out;

    /* Read several particles */
    err = H5PTread_packets(table, 0, 3, &(readBuf[0]));
    if( err < 0)
        goto out;

    /* Read one particle */
    err = H5PTread_packets(table, 3, 1, &(readBuf[3]));
    if( err < 0)
        goto out;

    /* Read several particles */
    err = H5PTread_packets(table, 4, (NRECORDS - 4 ), &(readBuf[4]));
    if( err < 0)
        goto out;

    /* Ensure that particles were read correctly */
    for(c=0; c<NRECORDS; c++)
    {
        if( cmp_par(c%8, c, testPart, readBuf) != 0)
            goto out;
    }

    /* Close the table */
    err = H5PTclose(table);
    if( err < 0)
        goto out;

    PASSED();

    return 0;

    out:
        H5_FAILED();
        if( H5PTis_valid(table) < 0)
            H5PTclose(table);
        return -1;
}

/*-------------------------------------------------------------------------
 * test_get_next
 *
 * Tests the packets written by test_append can be read by
 * H5PTget_next().
 *
 *-------------------------------------------------------------------------
 */
int test_get_next(hid_t fid)
{
    herr_t err;
    hid_t table;
    particle_t readBuf[NRECORDS];
    particle_t readBuf2[NRECORDS];
    int c;

    TESTING("H5PTget_next");

    /* Open the table */
    table = H5PTopen(fid, PT_NAME);
    if( H5PTis_valid(table) < 0)
        goto out;

    /* Read several particles consecutively */
    for(c=0; c < NRECORDS; c++)
    {
        err = H5PTget_next(table, 1, &readBuf[c]);
        if(err < 0)
            goto out;
    }

    /* Ensure that particles were read correctly */
    for(c=0; c<NRECORDS; c++)
    {
        if( cmp_par(c, c, testPart, readBuf) != 0)
            goto out;
    }

    H5PTcreate_index(table);

    /* Read particles two by two */
    for(c=0; c < NRECORDS / 2; c++)
    {
        err = H5PTget_next(table, 2, &readBuf2[c * 2]);
        if(err < 0)
            goto out;
    }

    /* Ensure that particles were read correctly */
    for(c=0; c<NRECORDS; c++)
    {
        if( cmp_par(c, c, testPart, readBuf2) != 0)
            goto out;
    }

    /* Close the table */
    err = H5PTclose(table);
    if( err < 0)
        goto out;

    PASSED();
    return 0;

    out:
        H5_FAILED();
        if( H5PTis_valid(table) < 0)
            H5PTclose(table);
        return -1;
}

/*-------------------------------------------------------------------------
 * test_big_table
 *
 * Ensures that a FL packet table will not break when many (BIG_TABLE_SIZE)
 * packets are used.
 *
 *-------------------------------------------------------------------------
 */
int    test_big_table(hid_t fid)
{
    herr_t err;
    hid_t table;
    hid_t part_t;
    int c;
    particle_t readPart;
    hsize_t count;

    TESTING("large packet table");

    /* Create a datatype for the particle struct */
    part_t = make_particle_type();

    assert(part_t != -1);

    /* Create a new table */
    table = H5PTcreate_fl(fid, "Packet Test Dataset2", part_t, 33);
    H5Tclose(part_t);
    if( H5PTis_valid(table) < 0)
        goto out;

        /* Add many particles */
    for(c = 0; c < BIG_TABLE_SIZE ; c+=8)
    {
        /* Append eight particles at once*/
        err = H5PTappend(table, 8, &(testPart[0]));
        if( err < 0)
            goto out;
    }

    /* Count the number of packets in the table  */
    err = H5PTget_num_packets(table, &count);
    if( err < 0)
        goto out;
    if( count != BIG_TABLE_SIZE )
        goto out;

    /* Read particles to ensure that all of them were written correctly  */
    /* Also, ensure that H5PTcreate_fl set the current packet to */
    /* the first packet in the table                                     */
    for(c = 0; c < BIG_TABLE_SIZE; c++)
    {
        err = H5PTget_next(table, 1, &readPart);
        if(err < 0)
            goto out;

        /* Ensure that particles were read correctly */
        if( cmp_par(c % 8, 0, testPart, &readPart) != 0)
            goto out;
    }

    /* Close the table */
    err = H5PTclose(table);
    if( err < 0)
        goto out;

    PASSED();
    return 0;

    out:
        H5_FAILED();
        if( H5PTis_valid(table) < 0)
            H5PTclose(table);
        return -1;
}

/*-------------------------------------------------------------------------
 * test_varlen
 *
 * Tests creation, opening, closing, writing, reading, etc. on a
 * variable-length packet table.
 *
 *-------------------------------------------------------------------------
 */
int    test_varlen(hid_t fid)
{
  herr_t err;
  hid_t table=H5I_BADID;
  hsize_t count;

   /* Buffers to hold data */
  hvl_t writeBuffer[NRECORDS];
  hvl_t readBuffer[NRECORDS];

    /* This example has three different sizes of "record": longs, shorts, and particles */
  long longBuffer[NRECORDS];
  short shortBuffer[NRECORDS];
  int x;

  TESTING("variable-length packet tables");

   /* Initialize buffers */
  for(x=0; x<NRECORDS; x++)
  {
    longBuffer[x] = -x;
    shortBuffer[x] = x;
  }

    /* Fill the write buffer with a mix of variable types */
  for(x=0; x<8; x+=4)
  {
    writeBuffer[x].len = sizeof(long);
    writeBuffer[x].p = &(longBuffer[x]);
    writeBuffer[x+1].len = sizeof(short);
    writeBuffer[x+1].p = &(shortBuffer[x+1]);
    writeBuffer[x+2].len = sizeof(long);
    writeBuffer[x+2].p = &(longBuffer[x+2]);
    writeBuffer[x+3].len = sizeof(particle_t);
    writeBuffer[x+3].p = &(testPart[x+3]);
  }

  /* Create the table */
  table = H5PTcreate_vl(fid, VL_TABLE_NAME, 1001);
  if( H5PTis_valid(table) < 0)
    goto out;
  if( H5PTis_varlen(table) != 1)
    goto out;

  /* Count the number of packets in the table  */
  err = H5PTget_num_packets(table, &count);
  if( err < 0)
      goto out;
  if( count != 0 )
      goto out;

  /* Close the table */
  err = H5PTclose(table);
  if( err < 0)
    goto out;

  /* Re-open the table */
  table = H5PTopen(fid, VL_TABLE_NAME);
  if( H5PTis_valid(table) < 0)
    goto out;
  if( H5PTis_varlen(table) != 1)
    goto out;

  /* Count the number of packets in the table  */
  err = H5PTget_num_packets(table, &count);
  if( err < 0)
      goto out;
  if( count != 0 )
      goto out;

  /* Add several variable-length packets */
  err = H5PTappend(table, 8, writeBuffer );
  if(err < 0)
    goto out;

  /* Read them back */
  err = H5PTread_packets(table, 0, 4, &(readBuffer[0]));
  if( err < 0)
    goto out;
  err = H5PTread_packets(table, 4, 1, &(readBuffer[4]));
  if( err < 0)
    goto out;
  err = H5PTread_packets(table, 5, (NRECORDS - 5 ), &(readBuffer[5]));
  if( err < 0)
    goto out;

  /* Ensure that packets were read correctly */
  for(x=0; x<NRECORDS; x++)
  {
    if( readBuffer[x].len != writeBuffer[x%4].len)
      goto out;
    switch(x%4)
    {
    case 0:
    case 2:
      if( *((long*)(readBuffer[x].p)) != *((long*)(writeBuffer[x].p)))
        goto out;
      break;
    case 1:
      if( *((short*)(readBuffer[x].p)) != *((short*)(writeBuffer[x].p)))
        goto out;
      break;
    case 3:
      if( cmp_par(0, 0, readBuffer[x].p, writeBuffer[x].p) < 0)
        goto out;
      break;
    default:
      goto out;
    }
  }

  /* Free memory used by read buffer */
  if(H5PTfree_vlen_readbuff(table, NRECORDS, readBuffer) <0)
    goto out;

  /* Read packets back using get_next */
  for(x=0; x < NRECORDS; x++)
  {
    err = H5PTget_next(table, 1, &readBuffer[x]);
    if(err < 0)
      goto out;
  }

  /* Ensure that packets were read correctly */
  for(x=0; x<NRECORDS; x++)
  {
    if( readBuffer[x].len != writeBuffer[x%4].len)
      goto out;
    switch(x%4)
    {
    case 0:
    case 2:
      if( *((long*)(readBuffer[x].p)) != *((long*)(writeBuffer[x].p)))
        goto out;
      break;
    case 1:
      if( *((short*)(readBuffer[x].p)) != *((short*)(writeBuffer[x].p)))
        goto out;
      break;
    case 3:
      if( cmp_par(0, 0, readBuffer[x].p, writeBuffer[x].p) < 0)
        goto out;
      break;
    default:
      goto out;
    }
  }

  /* Free memory used by read buffer */
  if(H5PTfree_vlen_readbuff(table, NRECORDS, readBuffer) <0)
    goto out;

  /* Close the table */
  err = H5PTclose(table);
  if( err < 0)
    goto out;

  PASSED();
  return 0;

  out:
    H5_FAILED();
    H5E_BEGIN_TRY
      H5PTclose(table);
    H5E_END_TRY
    return -1;
}
/*-------------------------------------------------------------------------
 * test_opaque
 *
 * Tests that the packet table works with an opaque datatype.
 *
 *-------------------------------------------------------------------------
 */
int    test_opaque(hid_t fid)
{
    herr_t err;
    hid_t table;
    hid_t part_t;
    int c;
    particle_t readBuf[NRECORDS];

    TESTING("opaque data");

    /* Create an opaque datatype for the particle struct */
    if ((part_t = H5Tcreate (H5T_OPAQUE, sizeof(particle_t) )) < 0 )
        return -1;

    assert(part_t != -1);

    /* Tag the opaque datatype */
    if ( H5Tset_tag(part_t,  "Opaque Particle"  ) < 0)
        return -1;

    /* Create a new table */
    table = H5PTcreate_fl(fid, "Packet Test Dataset3", part_t, 1);
    H5Tclose(part_t);
    if( H5PTis_valid(table) < 0)
        goto out;

    /* Append several particles, starting at particle 1 */
    err = H5PTappend(table, NRECORDS - 1, &(testPart[1]));
    if( err < 0)
        goto out;

    /* Read the particles back */
    err = H5PTread_packets(table, 0, 7, &(readBuf[0]));
    if( err < 0)
        goto out;

    /* Ensure that particles were read correctly */
    for(c=0; c<NRECORDS - 1; c++)
    {
        if( cmp_par(c+1, c, testPart, readBuf) != 0)
            goto out;
    }

    /* Close the table */
    err = H5PTclose(table);
    if( err < 0)
        goto out;

    PASSED();
    return 0;

    out:
        H5_FAILED();
        if( H5PTis_valid(table) < 0)
            H5PTclose(table);
        return -1;
}

/*-------------------------------------------------------------------------
 * test_error
 *
 * ensures that the packet table API throws the correct errors used on
 * objects that are not packet tables.
 *
 *-------------------------------------------------------------------------
 */
int test_error(hid_t fid)
{
  hid_t id = H5I_BADID;
  int id_open=0;
  particle_t readBuf[1];

  TESTING("error conditions");

  /* Create a HL table */
  if(create_hl_table(fid) < 0)
    goto out;

  /* Try to open things that are not packet tables */
  H5E_BEGIN_TRY
  if(H5PTopen(fid, "Bogus_name") >= 0)
    goto out;
  if(H5PTopen(fid, "group1") >= 0)
    goto out;
  H5E_END_TRY

  /* Try to execute packet table commands on an invalid ID */
  H5E_BEGIN_TRY
  if(H5PTis_valid(id) >= 0)
    goto out;
  if(H5PTis_varlen(id) >= 0)
    goto out;
  if(H5PTclose(id) >= 0)
    goto out;
  if(H5PTappend(id, 1, testPart) >= 0)
    goto out;
  if(H5PTread_packets(id, 0, 1, readBuf) >= 0)
    goto out;
  if(H5PTcreate_index(id) >= 0)
    goto out;
  H5E_END_TRY

  /* Open a high-level non-packet (H5TB) table and try to */
  /* execute commands on it. */
  if((id=H5Dopen(fid, H5TB_TABLE_NAME)) <0)
    goto out;
  id_open = 1;

  H5E_BEGIN_TRY
  if(H5PTis_valid(id) >= 0)
    goto out;
  if(H5PTis_varlen(id) >= 0)
    goto out;
  if(H5PTclose(id) >= 0)
    goto out;
  if(H5PTappend(id, 1, testPart) >= 0)
    goto out;
  if(H5PTread_packets(id, 0, 1, readBuf) >= 0)
    goto out;
  if(H5PTcreate_index(id) >= 0)
    goto out;
  H5E_END_TRY

  id_open=0;
  if(H5Dclose(id) <0)
    goto out;

  /* Open and close a packet table.  Try to execute */
  /* commands on the closed ID. */
  if((id=H5PTopen(fid, PT_NAME))<0)
    goto out;
  if(H5PTclose(id) <0)
    goto out;

  H5E_BEGIN_TRY
  if(H5PTis_valid(id) >= 0)
    goto out;
  if(H5PTis_varlen(id) >= 0)
    goto out;
  if(H5PTclose(id) >= 0)
    goto out;
  if(H5PTappend(id, 1, testPart) >= 0)
    goto out;
  if(H5PTread_packets(id, 0, 1, readBuf) >= 0)
    goto out;
  if(H5PTcreate_index(id) >= 0)
    goto out;
  H5E_END_TRY

  PASSED();
  return 0;

out:
  H5_FAILED();
  if(id_open)
    H5Dclose(id);
  return -1;
}


int test_packet_table(hid_t fid)
{

    if( test_create_close(fid) < 0 )
        return -1;

    if( test_open(fid) < 0 )
        return -1;

    /* test_append must be run before test_count and test_read, as it */
    /* creates the packet table they use. */
    if( test_append(fid) < 0 )
        return -1;

    /* These tests will not necessarily cause failures in each other,
           so we don't abort the other tests if one fails. */
    test_read(fid);
    test_get_next(fid);
    test_big_table(fid);
    test_varlen(fid);
    test_opaque(fid);
    test_error(fid);

    return 0;
}

int main(void)
{
 /* identifier for the file */
 hid_t       fid;
 int         status = 0;

/*-------------------------------------------------------------------------
 * Packet test: test each function of the packet table
 *-------------------------------------------------------------------------
 */

 /* create a file using default properties */
 fid=H5Fcreate(TEST_FILE_NAME, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

 puts("Testing packet table");

 /* run tests */
 if ( test_packet_table(fid) < 0)
     status = 1;

 /* close */
 H5Fclose(fid);

 return status;
}
