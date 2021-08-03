# hdf5test

test code for hdf5 data readin for DUNE

The directory readintest_march2021 contains code to exercise the
C API for HDF5 for unpacking a test HDF5 file created by the
DAQ for ProtoDUNE-SP.  It was written as a standalone application
in the process of converting an input source for art from using
the C++ API to the C API.  setuph.sh sets up the environment on
a DUNE machine with UPS, and build.sh and build2.sh compile and
link the programs.

The directory swtest_june2021 contains code to dump file
attributes from a software test file in a newer format with
fewer attributes and more header information packed in the
dataset.  The file hdf5_dump.py was written by the DAQ group
and the testmain.cc program replicates its functionality in C++,
using the C interface to hdf5.  setuph.sh sets up the environment
on a DUNE machine with UPS, and build.sh compiles it.