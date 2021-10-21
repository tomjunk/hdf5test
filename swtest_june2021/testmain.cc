#include <hdf5.h>
#include <iostream>
#include <list>
#include <string>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <stdio.h>
#include "FelixFormat.hh"

std::list<std::string> getTopLevelGroupNames(hid_t fd);

std::list<std::string> getMidLevelGroupNames(hid_t gid);

hid_t getGroupFromPath(hid_t fd, const std::string &path);

bool attrExists(hid_t object, const char *attrname);

int main(int argc, char **argv)
{

  bool dump_binary_files = false;
  bool dump_waveforms = false;

  std::string infilename = "/home/trj/dune/app/users/trj/splitter12/work/swt1oct.hdf5";
  // "sep24swtest.hdf5"
  // "swtest_run000333_0000_np04daq_20210608T155720.hdf5"

  hid_t fd = H5Fopen(infilename.c_str(),H5F_ACC_RDONLY,H5P_DEFAULT);
  hid_t grp = H5Gopen(fd,"/", H5P_DEFAULT);
  //char name[1000];
  //ssize_t len = H5Iget_name(grp,name,1000);
  //std::cout << name << std::endl;
  hid_t ga = H5Aopen_name(grp,"data_format_version");
  int dataformatversion=0;
  //hid_t type = H5Tcopy(H5T_IEEE_F32LE);

  herr_t ecode = H5Aread(ga,H5Aget_type(ga),&dataformatversion);
  std::cout << "Data Format verison: " << dataformatversion << " Error code: " << ecode << std::endl;
  H5Aclose(ga);

  //  int runnumber=0;
  //  hid_t grn = H5Aopen_name(grp,"run_number");
  //  ecode = H5Aread(grn,H5Aget_type(grn),&runnumber);
  //  std::cout << "Run Number: " << runnumber << " Error code: " << ecode << std::endl;
  //  ecode = H5Aclose(grn);

  H5Gclose(grp);

  // get top-level group names and print them out

  std::list<std::string> theList = getTopLevelGroupNames(fd);
  for (auto i : theList)
    {
      std::cout << "Top-Level Group Name: " << i << std::endl;
      std::string topLevelGroupName = i;
      // now look inside those groups for detector names
      hid_t requestedGroup = getGroupFromPath(fd,i);
      std::list<std::string> detectorTypeNames = getMidLevelGroupNames(requestedGroup);
      for (auto& detectorTypeName : detectorTypeNames)
        {
          std::cout << "  Detector type: " << detectorTypeName << std::endl;
          std::string subdetGroupPath = i + "/" + detectorTypeName;
	  if (detectorTypeName == "TPC")
	    {
              hid_t subdetGroup = getGroupFromPath(fd,subdetGroupPath);
	      std::list<std::string> subdetGeoNames = getMidLevelGroupNames(subdetGroup);
	      for (auto& subdetGeoName : subdetGeoNames) // loop over APAs
		{
		  std::string geoGroupPath = subdetGroupPath + "/" + subdetGeoName;
		  std::cout << "     Geo path: " << geoGroupPath << std::endl;
		  hid_t geoGroup = getGroupFromPath(fd,geoGroupPath);
	          std::list<std::string> dataSetNames = getMidLevelGroupNames(geoGroup);
	          for (auto& dataSetName : dataSetNames)  
		    {
		      std::string dataSetPath = geoGroupPath + "/" + dataSetName;
		      std::cout << "      Data Set Path: " << dataSetPath << std::endl;
		      std::string dspu = dataSetPath;
		      std::replace(dspu.begin(),dspu.end(),'/','_');
		      std::cout << "      Data Set Path (underscore version): " << dspu << std::endl;
		      hid_t datasetid = H5Dopen(geoGroup,dataSetName.data(),H5P_DEFAULT);
		      hsize_t ds_size = H5Dget_storage_size(datasetid);
		      std::cout << "      Data Set Size (bytes): " << ds_size << std::endl;
		      // todo -- check for zero size
		      if (ds_size < 80) 
			{
			  std::cout << "TPC datset too small for the fragment header" << std::endl; 
			}
		      size_t narray = ds_size / sizeof(char);
		      size_t rdr = ds_size % sizeof(char);
		      if (rdr > 0 || narray == 0) narray++;
		      char *ds_data = new char[narray];
		      ecode = H5Dread(datasetid, H5T_STD_I8LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, ds_data);
		      H5Dclose(datasetid);

		      // got all the data for this dataset, can work with it

		      int firstbyte = ds_data[0];
		      firstbyte &= 0xFF;
		      int lastbyte = ds_data[narray-1];
		      lastbyte &= 0xFF;
		      std::cout << std::hex << "      Retrieved data: ecode: " << ecode << "  first byte: " << firstbyte
				<< " last byte: " << lastbyte  << std::dec << std::endl;

                      if (dump_binary_files)
			{
			  std::string dspuname = dspu + ".dat";
		          FILE *ofile = fopen(dspuname.c_str(),"w");
		          fwrite(ds_data,1,narray,ofile);
		          fclose(ofile);
			}
		      if (dump_waveforms)
			{
			  //std::string dspuname = dspu + "_waveforms.txt";
			  std::cout << dspu << std::endl;
			  dune::FelixFrame* framedata = reinterpret_cast<dune::FelixFrame*>(ds_data+80);
			  size_t numframes = (ds_size - 80)/sizeof(dune::FelixFrame);
			  for (size_t iframe=0;iframe<numframes; ++iframe)
			    {
			      for (size_t ichan=0; ichan< 255; ++ichan)
				{
				  std::cout << iframe << " " << ichan << " " << framedata[iframe].channel(ichan) << std::endl;
				}
			    }
			}

		      int magic_word = 0;
		      memcpy(&magic_word,&ds_data[0],4);
		      std::cout << "   Magic word: 0x" << std::hex << magic_word << std::dec << std::endl;

		      int version = 0;
		      memcpy(&version, &ds_data[4],4);
		      std::cout << "   Version: " << std::dec << version << std::dec << std::endl;

		      uint64_t fragsize=0;
		      memcpy(&fragsize, &ds_data[8],8);
		      std::cout << "   Frag Size: " << std::dec << fragsize << std::dec << std::endl;

		      uint64_t trignum=0;
		      memcpy(&trignum, &ds_data[16],8);
		      std::cout << "   Trig Num: " << std::dec << trignum << std::dec << std::endl;

		      uint64_t trig_timestamp=0;
		      memcpy(&trig_timestamp, &ds_data[24],8);
		      std::cout << "   Trig Timestamp: " << std::dec << trig_timestamp << std::dec << std::endl;

		      uint64_t windowbeg=0;
		      memcpy(&windowbeg, &ds_data[32],8);
		      std::cout << "   Window Begin:   " << std::dec << windowbeg << std::dec << std::endl;

		      uint64_t windowend=0;
		      memcpy(&windowend, &ds_data[40],8);
		      std::cout << "   Window End:     " << std::dec << windowend << std::dec << std::endl;

		      int runno=0;
		      memcpy(&runno, &ds_data[48], 4);
		      std::cout << "   Run Number: " << std::dec << runno << std::endl;

		      int errbits=0;
		      memcpy(&errbits, &ds_data[52], 4);
		      std::cout << "   Error bits: " << std::dec << errbits << std::endl;

		      int fragtype=0;
		      memcpy(&fragtype, &ds_data[56], 4);
		      std::cout << "   Fragment type: " << std::dec << fragtype << std::endl;

		      int fragpadding=0;
		      memcpy(&fragtype, &ds_data[60], 4);
		      //std::cout << "   Fragment padding: " << std::dec << fragpadding << std::endl;

		      int geoidversion=0;
		      memcpy(&geoidversion, &ds_data[64], 4);
		      std::cout << "   GeoID version: " << std::dec << geoidversion << std::endl;

		      unsigned short geoidtype;
		      memcpy(&geoidtype, &ds_data[70], 1);
		      std::cout << "   GeoID type: " << geoidtype << std::endl;

		      unsigned short geoidregion=0;
		      memcpy(&geoidregion, &ds_data[71], 1);
		      std::cout << "   GeoID region: " << std::dec << geoidregion << std::endl;

		      int geoidelement=0;
		      memcpy(&geoidelement, &ds_data[72], 4);
		      std::cout << "   GeoID element: " << std::dec << geoidelement << std::endl;

		      int geoidpadding=0;
		      memcpy(&geoidpadding, &ds_data[76], 4);
		      //std::cout << "   GeoID padding: " << std::dec << geoidpadding << std::endl;

		      delete[] ds_data;  // free up memory
		    }
		  H5Gclose(geoGroup);
		}
	      H5Gclose(subdetGroup);
	    }
	  else if (detectorTypeName == "TriggerRecordHeader")
	    {
	      hid_t datasetid = H5Dopen(requestedGroup,detectorTypeName.data(),H5P_DEFAULT);
	      hsize_t ds_size = H5Dget_storage_size(datasetid);
	      std::cout << "      Data Set Size (bytes): " << ds_size << std::endl;
	      // todo -- check for zero size
	      if (ds_size < 64) 
		{
		  std::cout << "TriggerRecordHeader datset too small" << std::endl; 
		}
	      size_t narray = ds_size / sizeof(char);
	      size_t rdr = ds_size % sizeof(char);
	      if (rdr > 0 || narray == 0) narray++;
	      char *ds_data = new char[narray];
	      ecode = H5Dread(datasetid, H5T_STD_I8LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, ds_data);
	      int firstbyte = ds_data[0];
	      firstbyte &= 0xFF;
	      int lastbyte = ds_data[narray-1];
	      lastbyte &= 0xFF;
	      std::cout << std::hex << "      Retrieved data: ecode: " << ecode << "  first byte: " << firstbyte
			<< " last byte: " << lastbyte  << std::dec << std::endl;
	      H5Dclose(datasetid);


	      int magic_word = 0;
	      memcpy(&magic_word,&ds_data[0],4);
	      std::cout << "   Magic word: 0x" << std::hex << magic_word << std::dec << std::endl;

	      int version = 0;
	      memcpy(&version, &ds_data[4],4);
	      std::cout << "   Version: " << std::dec << version << std::dec << std::endl;

	      uint64_t trignum=0;
	      memcpy(&trignum, &ds_data[8],8);
	      std::cout << "   Trig Num: " << std::dec << trignum << std::dec << std::endl;

	      uint64_t trig_timestamp=0;
	      memcpy(&trig_timestamp, &ds_data[16],8);
	      std::cout << "   Trig Timestamp: " << std::dec << trig_timestamp << std::dec << std::endl;

	      uint64_t nreq=0;
	      memcpy(&nreq, &ds_data[24],8);
	      std::cout << "   No. of requested components:   " << std::dec << nreq << std::dec << std::endl;

	      int runno=0;
	      memcpy(&runno, &ds_data[32], 4);
	      std::cout << "   Run Number: " << std::dec << runno << std::endl;

	      int errbits=0;
	      memcpy(&errbits, &ds_data[36], 4);
	      std::cout << "   Error bits: " << std::dec << errbits << std::endl;

	      short triggertype=0;
	      memcpy(&triggertype, &ds_data[40], 2);
	      std::cout << "   Trigger type: " << std::dec << triggertype << std::endl;

	      delete[] ds_data;  // free up memory

	    }
	}
      H5Gclose(requestedGroup);
    }
  H5Fclose(fd);
  return(0);
}


// change to a fileinfo reference from a file pointer

std::list<std::string> getTopLevelGroupNames(hid_t fd)
{
  hid_t grp = H5Gopen(fd,"/", H5P_DEFAULT);
  std::list<std::string> theList = getMidLevelGroupNames(grp);
  H5Gclose(grp);
  return theList;
}

// get list of subgroups of a group

std::list<std::string> getMidLevelGroupNames(hid_t grp)
{
    
  std::list<std::string> theList;
  hsize_t nobj = 0;
  herr_t ecode = H5Gget_num_objs(grp,&nobj);
  for (hsize_t idx = 0; idx < nobj; ++idx)
    {
      hsize_t len = H5Gget_objname_by_idx(grp, idx, NULL, 0 );
      char *memb_name = new char(len+1);
      hsize_t len2 = H5Gget_objname_by_idx(grp, idx, memb_name, len+1 );
      theList.emplace_back(memb_name);
      delete[] memb_name;
    }

  return theList;
}

// change fd to a file info ptr

hid_t getGroupFromPath(hid_t fd, const std::string &path)
{
  hid_t grp = H5Gopen(fd,path.data(),H5P_DEFAULT);
  return grp;
}

// check to see if an attribute exists -- quiet the error messages when probing
// using example from the HDF5 documentation

bool attrExists(hid_t object, const char *attrname)
{
  // Save old error handler 
  H5E_auto_t old_func;
  void *old_client_data;
  H5Eget_auto(H5E_DEFAULT,&old_func, &old_client_data);

  // Turn off error handling */
  H5Eset_auto(H5E_DEFAULT,NULL, NULL);

  // Probe. On failure, retval is supposed to be negative

  hid_t retval = H5Aopen_name(object, attrname);

  // Restore previous error handler 
  H5Eset_auto(H5E_DEFAULT,old_func, old_client_data);

  bool result = (retval >= 0);
  return result;
}
