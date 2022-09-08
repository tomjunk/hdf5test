#include <iostream>
#include <string>
#include "hdf5.h"
#include "detdataformats/pacman/PACMANFrame.hpp"

double      get_attr_f64(hid_t group, std::string attrname);
std::string get_attr_varstring(hid_t group, std::string attrname);

int main(int argc, char **argv)
{
  std::string infilename = "raw_2021_04_03_10_53_48_CEST.h5";
  hid_t fd = H5Fopen(infilename.c_str(),H5F_ACC_RDONLY,H5P_DEFAULT);

  hid_t metagroup = H5Gopen(fd,"meta",H5P_DEFAULT);
  std::cout << "attribute: created: " << get_attr_f64(metagroup, "created") << std::endl;
  std::cout << "attribute: io_version: " << get_attr_varstring(metagroup, "io_version") << std::endl;
  std::cout << "attribute: modified: " << get_attr_f64(metagroup, "modified") << std::endl;
  std::cout << "attribute: version: " << get_attr_varstring(metagroup, "version") << std::endl;
  H5Gclose(metagroup);

  // read the variable-length arrays of messages
  // special thanks to https://svn.ssec.wisc.edu/repos/geoffc/C/HDF5/Examples_by_API/h5ex_t_vlen.c
  // for the example C API variable-length array reader

  hid_t datasetid = H5Dopen(fd,"msgs",H5P_DEFAULT);
  hsize_t ds_size = H5Dget_storage_size(datasetid);
  std::cout << "msgs dataset size: " << ds_size << std::endl;
  hid_t dataspaceid = H5Dget_space(datasetid);
  hsize_t     dims[1] = {2};
  int ndims = H5Sget_simple_extent_dims (dataspaceid, dims, NULL);
  hid_t vlt = H5Tvlen_create(H5T_STD_U8LE);
  hvl_t *rdata = (hvl_t *) malloc (dims[0] * sizeof (hvl_t));
  H5Dread(datasetid, vlt, H5S_ALL, H5S_ALL, H5P_DEFAULT, rdata);

  // print info about the messages
  std::cout << "Number of messages: " << dims[0] << std::endl;
  dunedaq::detdataformats::pacman::PACMANFrame pmf;
  for (size_t imessage = 0; imessage < dims[0]; ++imessage)
    {
      void *messageptr = rdata[imessage].p;
      dunedaq::detdataformats::pacman::PACMANFrame::PACMANMessageHeader *hdr = pmf.get_msg_header(messageptr);
      
      std::cout << "PACMAN message[" << imessage << "] nwords: " << hdr->words << " message type: " << hdr->type << std::endl;
      dunedaq::detdataformats::pacman::PACMANFrame::msg_type mtype = hdr->type;
      if (mtype == dunedaq::detdataformats::pacman::PACMANFrame::msg_type::DATA_MSG)
	{
	  std::cout << "unpacking data words" << std::endl;
	  for (size_t iword = 0; iword<hdr->words; ++iword)
	    {
	      dunedaq::detdataformats::pacman::PACMANFrame::PACMANMessageWord* mwp = pmf.get_msg_word(messageptr,iword);
	      std::cout << "word[" << iword << "] type: " 
			<< mwp->data_word.type 
	                << " uart chan: " << (int) mwp->data_word.channel_id 
	         	<< " chipid: " << mwp->data_word.larpix_word.data_packet.chipid 
	         	<< " channelid: " << mwp->data_word.larpix_word.data_packet.channelid 
                  	<< " adc: " << mwp->data_word.larpix_word.data_packet.dataword 
	         	<< " timestamp: " << mwp->data_word.larpix_word.data_packet.timestamp 
	         	<< " trigger_type: " << mwp->data_word.larpix_word.data_packet.trigger_type 
			<< std::endl;
	    }
	}
    }

  H5Dvlen_reclaim (vlt, dataspaceid, H5P_DEFAULT, rdata);
  free(rdata);
  H5Dclose(datasetid);
  H5Sclose(dataspaceid);
  H5Tclose(vlt);

  H5Fclose(fd);
  return(0);
}

double get_attr_f64(hid_t group, std::string attrname)
{
  double value=0;
  hid_t ga = H5Aopen_name(group, attrname.c_str());
  H5Aread(ga, H5Aget_type(ga), &value);
  H5Aclose(ga);
  return value;
}

std::string get_attr_varstring(hid_t group, std::string attrname)
{
  char *value;
  hid_t ga = H5Aopen_name(group, attrname.c_str());
  H5Aread(ga, H5Aget_type(ga), &value);
  std::string ret(value);
  H5free_memory(value);
  H5Aclose(ga);
  return ret;
}
