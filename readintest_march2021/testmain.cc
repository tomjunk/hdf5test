#include <hdf5.h>
#include <iostream>
#include <list>
#include <string>
#include <sstream>

std::list<std::string> getTopLevelGroupNames(hid_t fd);

std::list<std::string> getMidLevelGroupNames(hid_t gid);

hid_t getGroupFromPath(hid_t fd, const std::string &path);

bool attrExists(hid_t object, const char *attrname);

int main(int argc, char **argv)
{

  hid_t fd = H5Fopen("np04_timeSliceData_run011765_0034_dl1.hdf5",H5F_ACC_RDONLY,H5P_DEFAULT);
  hid_t grp = H5Gopen(fd,"/", H5P_DEFAULT);
  //char name[1000];
  //ssize_t len = H5Iget_name(grp,name,1000);
  //std::cout << name << std::endl;
  hid_t ga = H5Aopen_name(grp,"attribute_style_version");
  float styleversion=0;
  //hid_t type = H5Tcopy(H5T_IEEE_F32LE);

  herr_t ecode = H5Aread(ga,H5Aget_type(ga),&styleversion);
  std::cout << "style verison: " << styleversion << " Error code: " << ecode << std::endl;
  H5Aclose(ga);

  if (styleversion > 0.8)
    {
      int runnumber=0;
      hid_t grn = H5Aopen_name(grp,"run_number");
      ecode = H5Aread(grn,H5Aget_type(grn),&runnumber);
      std::cout << "Run Number: " << runnumber << " Error code: " << ecode << std::endl;
      ecode = H5Aclose(grn);
    }

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
          std::cout << "   Detector type: " << detectorTypeName << std::endl;
          std::string subdetGroupPath = i + "/" + detectorTypeName;
          hid_t subdetGroup = getGroupFromPath(fd,subdetGroupPath);
          std::list<std::string> subdetGeoNames = getMidLevelGroupNames(subdetGroup);
          for (auto& subdetGeoName : subdetGeoNames)
            {
              std::string dataSetPath = subdetGroupPath + "/" + subdetGeoName;
              std::cout << "      Data Set Path: " << dataSetPath << std::endl;
              hid_t datasetid = H5Dopen(subdetGroup,subdetGeoName.data(),H5P_DEFAULT);
              hsize_t ds_size = H5Dget_storage_size(datasetid);
              std::cout << "      Data Set Size (bytes): " << ds_size << std::endl;
              // todo -- check for zero size
              size_t narray = ds_size / sizeof(unsigned long long);
              size_t rdr = ds_size % sizeof(unsigned long long);
              if (rdr > 0 || narray == 0) narray++;
              unsigned long long *ds_data = new unsigned long long [narray];
              ecode = H5Dread(datasetid, H5T_STD_U64LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, ds_data);
              std::cout << "      Retrieved data: ecode: " << ecode << "  first word: " << ds_data[0]
                        << " last word: " << ds_data[narray-1] << std::endl;

              if (detectorTypeName == "FELIX")
                {
                  uint64_t windowStart = 0;
                  uint64_t windowEnd = 0;
                  uint64_t triggerTime = 0;
		  int32_t numFrames = 0;

                  if (attrExists(requestedGroup,"timeslice_start"))
                    {
                      hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_start");
                      ecode = H5Aread(a1, H5Aget_type(a1), &windowStart);
                      H5Aclose(a1);
                    }
                  else
                    {
                      if (attrExists(datasetid,"timestamp_of_first_frame"))
                        {
                          hid_t a1 = H5Aopen_name(datasetid,"timestamp_of_first_frame");
                          ecode = H5Aread(a1, H5Aget_type(a1), &windowStart);
                          H5Aclose(a1);
                        }
                    }
                  std::cout << "   timeslice start: " << windowStart << " ecode: " << ecode << std::endl;              

                  if (attrExists(requestedGroup,"timeslice_end"))
                    {
                      hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_end");
                      ecode = H5Aread(a1, H5Aget_type(a1), &windowEnd);
                      H5Aclose(a1);
                    }
                  else
                    {
                      if (attrExists(datasetid,"timestamp_of_last_frame"))
                        {
                          hid_t a1 = H5Aopen_name(datasetid,"timestamp_of_last_frame");
                          ecode = H5Aread(a1, H5Aget_type(a1), &windowEnd);
                          H5Aclose(a1);
                        }
                    }
                  std::cout << "   timeslice end: " << windowEnd << " ecode: " << ecode << std::endl;

		  if (attrExists(datasetid,"trigger_timestamp"))
		    {
		      hid_t a1 = H5Aopen_name(datasetid,"trigger_timestamp");
		      ecode = H5Aread(a1, H5Aget_type(a1), &triggerTime);
		      H5Aclose(a1);
		    }
		  else
		    {
		      if (attrExists(requestedGroup,"trigger_timestamp"))
			{
			  hid_t a1 = H5Aopen_name(requestedGroup,"trigger_timestamp");
			  ecode = H5Aread(a1, H5Aget_type(a1), &triggerTime);
			  H5Aclose(a1);
			}
		      else
			{
			  if (attrExists(requestedGroup,"timeslice_start"))
			    {
			      hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_start");
			      ecode = H5Aread(a1, H5Aget_type(a1), &triggerTime);
			      H5Aclose(a1);
			    }

			}
		    }
                  std::cout << "   Trigger timestamp: " << triggerTime << " ecode: " << ecode << std::endl;

                  if (attrExists(datasetid,"number_of_frames"))
		    {
		      hid_t a1 = H5Aopen_name(datasetid,"number_of_frames");
		      ecode = H5Aread(a1, H5Aget_type(a1), &numFrames);
		      H5Aclose(a1);
		    }
                  std::cout << "   Number of frames: " << numFrames << " ecode: " << ecode << std::endl;
                }

	      uint64_t pdsPayloadHeaderWord = 0;
              if (detectorTypeName == "PHOTON")
		{
		  if (attrExists(requestedGroup,"trigger_timestamp"))
		    {
		      uint64_t timestamp;
		      hid_t a1 = H5Aopen_name(requestedGroup,"trigger_timestamp");
		      ecode = H5Aread(a1, H5Aget_type(a1), &timestamp);
		      H5Aclose(a1);
		      //pdsMetadata.triggerTime = timestamp * 3;
		      std::cout << "PHOTON timestamp: " << timestamp << std::endl;
		    }
		  if (attrExists(requestedGroup,"timeslice_start"))
		    {
		      uint64_t timestamp;
		      hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_start");
		      ecode = H5Aread(a1, H5Aget_type(a1), &timestamp);
		      H5Aclose(a1);
		      //pdsMetadata.startTime = timestamp * 3;
		      std::cout << "PHOTON timeslice_start: " << timestamp << std::endl;
		    }
		  else
		    {
		      if (attrExists(datasetid,"timestamp_of_first_packet"))
			{
			  uint64_t timestamp;
			  hid_t a1 = H5Aopen_name(datasetid,"timestamp_of_first_packet");
			  ecode = H5Aread(a1, H5Aget_type(a1), &timestamp);
			  H5Aclose(a1);
			  //pdsMetadata.startTime = timestamp * 3;
			  std::cout << "PHOTON timestamp_of_first_packet: " << timestamp << std::endl;
			}
		    }
		  if (attrExists(requestedGroup,"timeslice_end"))
		    {
		      uint64_t timestamp;
		      hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_end");
		      ecode = H5Aread(a1, H5Aget_type(a1), &timestamp);
		      H5Aclose(a1);
		      //pdsMetadata.endTime = timestamp * 3;
		      std::cout << "PHOTON timeslice_end: " << timestamp << std::endl;
		    }
		  else
		    {
		      if (attrExists(datasetid,"timestamp_of_last_packet"))
			{
			  uint64_t timestamp;
			  hid_t a1 = H5Aopen_name(datasetid,"timestamp_of_last_packet");
			  ecode = H5Aread(a1, H5Aget_type(a1), &timestamp);
			  H5Aclose(a1);
			  //pdsMetadata.endTime = timestamp * 3;
			  std::cout << "PHOTON timestamp_of_last_packet: " << timestamp << std::endl;
			}
		    }
		  if (attrExists(datasetid,"number_of_packets"))
		    {
		      uint32_t packetCount;
		      hid_t a1 = H5Aopen_name(datasetid,"number_of_packets");
		      ecode = H5Aread(a1, H5Aget_type(a1), &packetCount);
		      H5Aclose(a1);
		      std::cout << "PHOTON number_of_packets: " << packetCount << std::endl;
		    }
		  if (attrExists(datasetid,"size_in_bytes"))
		    {
		      uint32_t sizeBytes;
		      hid_t a1 = H5Aopen_name(datasetid,"size_in_bytes");
		      ecode = H5Aread(a1, H5Aget_type(a1), &sizeBytes);
		      H5Aclose(a1);
		      std::cout << "PHOTON size_in_bytes: " << sizeBytes << std::endl;
		      //pdsMetadata.length = 9 + (sizeBytes / sizeof(uint32_t));
		      //pdsPayloadHeaderWord |= ((2 + (sizeBytes / sizeof(uint32_t))) & 0xffffffff);
		    }
		  else
		    {
		      //pdsMetadata.length = 9 + (ds_size / sizeof(uint32_t));
		      //pdsPayloadHeaderWord |= ((2 + (ds_size / sizeof(uint32_t))) & 0xffffffff);
		      size_t pdsMetadataLength = 9 + (ds_size / sizeof(uint32_t));
		      pdsPayloadHeaderWord |= ((2 + (ds_size / sizeof(uint32_t))) & 0xffffffff);
		    }
		}
	      
	      size_t seqID;
	      size_t fragID;
	      uint64_t headerTimestamp;

	      std::stringstream seqStr(topLevelGroupName);
	      seqStr >> seqID;
	      seqID &= 0xffffffffffff;

		      if (attrExists(requestedGroup,"trigger_timestamp"))
			{
			  uint64_t timestamp;
			  hid_t a1 = H5Aopen_name(requestedGroup,"trigger_timestamp");
			  ecode = H5Aread(a1, H5Aget_type(a1), &timestamp);
			  H5Aclose(a1);
			  headerTimestamp = timestamp;
			}
		      else
			{
			   std::stringstream timeStr(topLevelGroupName);
		           timeStr >> headerTimestamp;
			}
		      std::cout << "Header timestamp: " << headerTimestamp << std::endl;


              H5Dclose(datasetid);
              delete[] ds_data;  // free up memory
            }
          H5Gclose(subdetGroup);
        }
      H5Gclose(requestedGroup);
    }


  H5Fclose(fd);

  return 0;
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
