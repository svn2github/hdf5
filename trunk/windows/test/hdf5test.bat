@REM Copyright by The HDF Group.
@REM Copyright by the Board of Trustees of the University of Illinois.
@REM All rights reserved.
@REM
@REM This file is part of HDF5.  The full HDF5 copyright notice, including
@REM terms governing use, modification, and redistribution, is contained in
@REM the files COPYING and Copyright.html.  COPYING can be found at the root
@REM of the source code distribution tree; Copyright.html can be found at the
@REM root level of an installed copy of the electronic HDF5 document set and
@REM is linked from the top-level documents page.  It can also be found at
@REM http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have
@REM access to either file, you may request a copy from help@hdfgroup.org.

@ECHO OFF
REM This batch file is used to HDF5 C Library.
REM Last Modified: 05/19/2005 by Fang GUO

echo ============================
echo Testing testhdf5 
echo ============================

testhdf5%2\%1\testhdf5%2

echo ============================
echo Testing lheap 
echo ============================

lheap%2\%1\lheap%2

echo.
echo ============================
echo Testing ohdr 
echo ============================

ohdr%2\%1\ohdr%2

echo.
echo ============================
echo Testing stab 
echo ============================

stab%2\%1\stab%2

echo.
echo ============================
echo Testing gheap 
echo ============================

gheap%2\%1\gheap%2

echo.
echo ============================
echo Testing cache 
echo ============================

cache%2\%1\cache%2

echo.
echo ============================
echo Testing cache_api
echo ============================

cache_api%2\%1\cache_api%2

echo.
echo ============================
echo Testing btree2 
echo ============================

btree2%2\%1\btree2%2

echo.
echo ============================
echo Testing pool 
echo ============================

pool%2\%1\pool%2

echo.
echo ============================
echo Testing hyperslab 
echo ============================

hyperslab%2\%1\hyperslab%2

echo.
echo ============================
echo Testing istore 
echo ============================

istore%2\%1\istore%2

echo.
echo ============================
echo Testing bittests 
echo ============================

bittests%2\%1\bittests%2

echo.
echo ============================
echo Testing dt_arith 
echo ============================

dt_arith%2\%1\dt_arith%2

echo.
echo ============================
echo Testing dtypes 
echo ============================

dtypes%2\%1\dtypes%2

echo.
echo ============================
echo Testing dsets 
echo ============================

dsets%2\%1\dsets%2

echo.
echo ============================
echo Testing cmpd_dset 
echo ============================

cmpd_dset%2\%1\cmpd_dset%2

echo.
echo ============================
echo Testing extend 
echo ============================

extend%2\%1\extend%2

echo.
echo ============================
echo Testing external 
echo ============================

external%2\%1\external%2

echo.
echo ============================
echo Testing objcopy 
echo ============================

objcopy%2\%1\objcopy%2

echo.
echo ============================
echo Testing links  
echo ============================

links%2\%1\links%2

echo.
echo ============================
echo Testing unlink 
echo ============================

unlink%2\%1\unlink%2

echo.
echo ============================
echo Testing big 
echo ============================

echo.
echo Test skipped because it is not supported on Windows.
echo.

echo ============================
echo Testing mtime 
echo ============================

mtime%2\%1\mtime%2

echo.
echo ============================
echo Testing fillval 
echo ============================

fillval%2\%1\fillval%2

echo.
echo ============================
echo Testing mount 
echo ============================

mount%2\%1\mount%2

echo.
echo ============================
echo Testing flush1 
echo ============================

flush1%2\%1\flush1%2

echo.
echo ============================
echo Testing flush2 
echo ============================

flush2%2\%1\flush2%2

echo.
echo ============================
echo Testing enum 
echo ============================

enum%2\%1\enum%2

echo ============================
echo Testing set_extent 
echo ============================

set_extent%2\%1\set_extent%2

echo ============================
echo Testing ttsafe 
echo ============================

echo.
echo Test skipped because THREADSAFE is not supported on Windows.
echo.

echo ============================
echo Testing stream_test 
echo ============================

echo.
echo Test skipped because it is not supported on Windows.
echo.

echo ============================
echo Testing getname 
echo ============================

getname%2\%1\getname%2

echo.
echo ============================
echo Testing vfd 
echo ============================

vfd%2\%1\vfd%2

echo.
echo ============================
echo Testing ntypes 
echo ============================

ntypes%2\%1\ntypes%2

echo.
echo ============================
echo Testing dangle 
echo ============================

dangle%2\%1\dangle%2

echo.
echo ============================
echo Testing dtransform 
echo ============================

dtransform%2\%1\dtransform%2

echo.
echo ============================
echo Testing reserved 
echo ============================

reserved%2\%1\reserved%2

echo.
echo ============================
echo Testing cross_read 
echo ============================

cross_read%2\%1\cross_read%2

echo ============================
echo Testing fheap 
echo ============================

fheap%2\%1\fheap%2


echo.
echo ============================
echo Testing ./testerr.bat  
echo ============================
echo Test skipped because sed is not universal supported.
echo To test the HDF5 error API, you need to have sed on your machine.
echo Please remove echo on the following line in this batch file to do the test.
echo.

echo call testerr %1 %2
echo.
echo ============================
echo Testing talign 
echo ============================

..\tools\talign%2\%1\talign%2

