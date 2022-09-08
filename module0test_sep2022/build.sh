
g++ -g -std=c++17 -o tpu tpu.C -I${HDF5_INC} -I${DUNEDETDATAFORMATS_INC} -L${HDF5_FQ_DIR}/lib -lhdf5 -lz -lm

# from https://svn.ssec.wisc.edu/repos/geoffc/C/HDF5/Examples_by_API/h5ex_t_vlen.c

gcc -g -o tvl tvl.c -I${HDF5_INC} -L${HDF5_FQ_DIR}/lib -lhdf5
