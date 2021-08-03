#include "dune-raw-data/HDFUtils/HDFFileReader.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "dune-raw-data/Overlays/SSPFragment.hh"
#include "artdaq-core/Utilities/TimeUtils.hh"
#include <boost/algorithm/string.hpp>
#include <regex>
#include <set>
#include <sstream>

#include <sys/time.h>
#include <sys/resource.h>

namespace dune {
  namespace HDFFileReader {

    // *****************************************
    // *** General-purpose data and routines ***
    // *****************************************

    HDFFileInfoPtr openFile(const std::string& fileName)
    {
      HDFFileInfoPtr hdfFileInfoPtr(new HDFFileInfo());
      hdfFileInfoPtr->filePtr = H5Fopen(fileName.data(), H5F_ACC_RDONLY, H5P_DEFAULT);
      hdfFileInfoPtr->bytesWritten = 0;
      hdfFileInfoPtr->fileName = fileName;

      hid_t grp = H5Gopen(hdfFileInfoPtr->filePtr,"/", H5P_DEFAULT);
      if (attrExists(grp,"attribute_style_version"))
        {
          hid_t ga = H5Aopen_name(grp,"attribute_style_version");
          float versionNumber=0;
          H5Aread(ga, H5Aget_type(ga), &versionNumber);
          H5Aclose(ga);
          if (versionNumber > 0.8)
            {
              int runnumber=0;
              hid_t grn = H5Aopen_name(grp,"run_number");
              H5Aread(grn,H5Aget_type(grn),&runnumber);
              H5Aclose(grn);
              hdfFileInfoPtr->runNumber = runnumber;
            }
        }
      H5Gclose(grp);
      return hdfFileInfoPtr;
    }

    HDFFileInfoPtr reopenFile(HDFFileInfoPtr oldHdfFileInfoPtr)
    {
      H5Fclose(oldHdfFileInfoPtr->filePtr);
      oldHdfFileInfoPtr->filePtr = 0;

      HDFFileInfoPtr newHdfFileInfoPtr(new HDFFileInfo());
      newHdfFileInfoPtr->filePtr = H5Fopen(oldHdfFileInfoPtr->fileName.data(), H5F_ACC_RDONLY, H5P_DEFAULT);
      newHdfFileInfoPtr->bytesWritten = 0;
      newHdfFileInfoPtr->fileName = oldHdfFileInfoPtr->fileName;
      newHdfFileInfoPtr->runNumber = oldHdfFileInfoPtr->runNumber;
      return newHdfFileInfoPtr;
    }

    void closeFile(HDFFileInfoPtr hdfFileInfoPtr)
    {
      H5Fclose(hdfFileInfoPtr->filePtr);
      hdfFileInfoPtr->filePtr = 0;
    }

    std::list<std::string> getTopLevelGroupNames(HDFFileInfoPtr& hdfFileInfoPtr)
    {
      hid_t grp = H5Gopen(hdfFileInfoPtr->filePtr,"/", H5P_DEFAULT);
      std::list<std::string> theList = getMidLevelGroupNames(grp);
      H5Gclose(grp);
      return theList;
    }


    std::list<std::string> getMidLevelGroupNames(hid_t grp)
    {
    
      std::list<std::string> theList;
      hsize_t nobj = 0;
      H5Gget_num_objs(grp,&nobj);
      for (hsize_t idx = 0; idx < nobj; ++idx)
        {
          hsize_t len = H5Gget_objname_by_idx(grp, idx, NULL, 0 );
          char *memb_name = new char(len+1);
          H5Gget_objname_by_idx(grp, idx, memb_name, len+1 );
          theList.emplace_back(memb_name);
          delete[] memb_name;
        }
      return theList;
    }

    hid_t getGroupFromPath(HDFFileInfoPtr& hdfFileInfoPtr, const std::string &path)
    {
      hid_t grp = H5Gopen(hdfFileInfoPtr->filePtr,path.data(),H5P_DEFAULT);
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

    // ************************************************
    // *** Trigger-Record-related data and routines ***
    // ************************************************

    FragmentListsByType getFragmentsForEvent(HDFFileInfoPtr& hdfFileInfoPtr, const std::string& topLevelGroupName)
    {
      FragmentListsByType fragmentMap;
      artdaq::Fragment::fragment_id_t sampleFragmentID = 1001;

      //struct rusage blah;
      //getrusage(RUSAGE_SELF, &blah);
      //std::cout << "KAB " << __LINE__ << " rusage max resident memory " << blah.ru_maxrss << std::endl;

      hid_t requestedGroup = getGroupFromPath(hdfFileInfoPtr, topLevelGroupName);
      {
        std::list<std::string> detectorTypeNames = getMidLevelGroupNames(requestedGroup);
        for (auto& detectorTypeName : detectorTypeNames)
          {
            std::string subdetGroupPath = topLevelGroupName + "/" + detectorTypeName;
            hid_t subdetGroup = getGroupFromPath(hdfFileInfoPtr, subdetGroupPath);
            {
              fragmentMap[detectorTypeName] = std::make_unique<artdaq::Fragments>();

              std::list<std::string> subdetGeoNames = getMidLevelGroupNames(subdetGroup);
              for (auto& subdetGeoName : subdetGeoNames)
                {
                  std::string dataSetPath = subdetGroupPath + "/" + subdetGeoName;
                  hid_t datasetid = H5Dopen(subdetGroup,subdetGeoName.data(),H5P_DEFAULT);
                  hsize_t ds_size = H5Dget_storage_size(datasetid);

		  // todo -- check for zero size and move to place data directly in the fragment instead of making
		  // a copy

		  size_t narray = ds_size / sizeof(unsigned long long);
		  size_t rdr = ds_size % sizeof(unsigned long long);
		  if (rdr > 0 || narray == 0) narray++;
		  unsigned long long *ds_data = new unsigned long long [narray];
		  H5Dread(datasetid, H5T_STD_U64LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, ds_data);

                  //std::cout << "Reading DataSet " << dataSetPath << ", sizes " << theDataSet.getStorageSize()
                  //    << " " << theDataSet.getDataType().getSize() << " " << theDataSet.getElementCount()
                  //    << std::endl;

                  artdaq::FragmentPtr frag;
                  artdaq::Fragment::type_t type = artdaq::Fragment::InvalidFragmentType;
                  uint32_t payloadOffset = 0;
                  if (detectorTypeName == FELIX_GROUP_NAME)
                    {
                      frag = artdaq::Fragment::FragmentBytes(ds_size);
                      type = dune::detail::FELIX;

                      uint64_t windowStart = 0;
                      uint64_t windowEnd = 0;
                      uint64_t triggerTime = 0;

                      if (attrExists(requestedGroup,"timeslice_start"))
                        {
                          hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_start");
                          H5Aread(a1, H5Aget_type(a1), &windowStart);
                          H5Aclose(a1);
                        }
                      else
                        {
                          if (attrExists(datasetid,"timestamp_of_first_frame"))
                            {
                              hid_t a1 = H5Aopen_name(datasetid,"timestamp_of_first_frame");
                              H5Aread(a1, H5Aget_type(a1), &windowStart);
                              H5Aclose(a1);
                            }
                        }

                      if (attrExists(requestedGroup,"timeslice_end"))
                        {
                          hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_end");
                          H5Aread(a1, H5Aget_type(a1), &windowEnd);
                          H5Aclose(a1);
                        }
                      else
                        {
                          if (attrExists(datasetid,"timestamp_of_last_frame"))
                            {
                              hid_t a1 = H5Aopen_name(datasetid,"timestamp_of_last_frame");
                              H5Aread(a1, H5Aget_type(a1), &windowEnd);
                              H5Aclose(a1);
                            }
                        }

                      if (attrExists(datasetid,"trigger_timestamp"))
                        {
                          hid_t a1 = H5Aopen_name(datasetid,"trigger_timestamp");
                          H5Aread(a1, H5Aget_type(a1), &triggerTime);
                          H5Aclose(a1);
                        }
                      else
                        {
                          if (attrExists(requestedGroup,"trigger_timestamp"))
                            {
                              hid_t a1 = H5Aopen_name(requestedGroup,"trigger_timestamp");
                              H5Aread(a1, H5Aget_type(a1), &triggerTime);
                              H5Aclose(a1);
                            }
                          else
                            {
                              if (attrExists(requestedGroup,"timeslice_start"))
                                {
                                  hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_start");
                                  H5Aread(a1, H5Aget_type(a1), &triggerTime);
                                  H5Aclose(a1);
                                }

                            }
                        }

                      dune::FelixFragmentBase::Metadata felixMetadata;
                      felixMetadata.control_word = 0xabc;
                      felixMetadata.version = 1;
                      felixMetadata.reordered = 0;
                      felixMetadata.compressed = 0;
                      felixMetadata.offset_frames = (uint32_t) ((triggerTime - windowStart) / 25);
                      felixMetadata.window_frames = (uint32_t) ((windowEnd - windowStart) / 25);

                      if (attrExists(datasetid,"number_of_frames"))
                        {
                          hid_t a1 = H5Aopen_name(datasetid,"number_of_frames");
                          H5Aread(a1, H5Aget_type(a1), &felixMetadata.num_frames);
                          H5Aclose(a1);
                        }

                      frag->setMetadata(felixMetadata);
                    }

                  uint64_t pdsPayloadHeaderWord = 0;
                  if (detectorTypeName == PDS_GROUP_NAME)
                    {
                      payloadOffset = sizeof(uint64_t);
                      frag = artdaq::Fragment::FragmentBytes(ds_size + sizeof(uint64_t));
                      type = dune::detail::PHOTON;

                      // fill in the SSPFragment metadata, as much as we can
                      SSPDAQ::MillisliceHeader pdsMetadata;
                      pdsMetadata.triggerTime = 0;
                      pdsMetadata.triggerType = 0;

                      if (attrExists(requestedGroup,"trigger_timestamp"))
                        {
                          uint64_t timestamp;
                          hid_t a1 = H5Aopen_name(requestedGroup,"trigger_timestamp");
                          H5Aread(a1, H5Aget_type(a1), &timestamp);
                          H5Aclose(a1);
                          pdsMetadata.triggerTime = timestamp * 3;
                        }

                      if (attrExists(requestedGroup,"timeslice_start"))
                        {
                          uint64_t timestamp;
                          hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_start");
                          H5Aread(a1, H5Aget_type(a1), &timestamp);
                          H5Aclose(a1);
                          pdsMetadata.startTime = timestamp * 3;
                          if (pdsMetadata.triggerTime == 0) {pdsMetadata.triggerTime = timestamp * 3;}
                        }
                      else
                        {
                          if (attrExists(datasetid,"timestamp_of_first_packet"))
                            {
                              uint64_t timestamp;
                              hid_t a1 = H5Aopen_name(datasetid,"timestamp_of_first_packet");
                              H5Aread(a1, H5Aget_type(a1), &timestamp);
                              H5Aclose(a1);
                              pdsMetadata.startTime = timestamp * 3;
                            }
                        }


                      if (attrExists(requestedGroup,"timeslice_end"))
                        {
                          uint64_t timestamp;
                          hid_t a1 = H5Aopen_name(requestedGroup,"timeslice_end");
                          H5Aread(a1, H5Aget_type(a1), &timestamp);
                          H5Aclose(a1);
                          pdsMetadata.endTime = timestamp * 3;
                        }
                      else
                        {
                          if (attrExists(datasetid,"timestamp_of_last_packet"))
                            {
                              uint64_t timestamp;
                              hid_t a1 = H5Aopen_name(datasetid,"timestamp_of_last_packet");
                              H5Aread(a1, H5Aget_type(a1), &timestamp);
                              H5Aclose(a1);
                              pdsMetadata.endTime = timestamp * 3;
                            }
                        }


                      if (attrExists(datasetid,"number_of_packets"))
                        {
                          uint32_t packetCount;
                          hid_t a1 = H5Aopen_name(datasetid,"number_of_packets");
                          H5Aread(a1, H5Aget_type(a1), &packetCount);
                          H5Aclose(a1);
                          pdsMetadata.nTriggers = packetCount;
                        }

                      if (attrExists(datasetid,"size_in_bytes"))
                        {
                          uint32_t sizeBytes;
                          hid_t a1 = H5Aopen_name(datasetid,"size_in_bytes");
                          H5Aread(a1, H5Aget_type(a1), &sizeBytes);
                          H5Aclose(a1);
                          pdsMetadata.length = 9 + (sizeBytes / sizeof(uint32_t));
                          pdsPayloadHeaderWord |= ((2 + (sizeBytes / sizeof(uint32_t))) & 0xffffffff);
                        }
                      else
                        {
                          pdsMetadata.length = 9 + (ds_size / sizeof(uint32_t));
                          pdsPayloadHeaderWord |= ((2 + (ds_size / sizeof(uint32_t))) & 0xffffffff);
                        }

                      frag->setMetadata(pdsMetadata);
                      pdsPayloadHeaderWord |= 0x000003e700000000;  // run number always seems to be set to 999
                    }

                  artdaq::Fragment::sequence_id_t seqID;
                  artdaq::Fragment::fragment_id_t fragID;
                  artdaq::Fragment::timestamp_t headerTimestamp;

                  std::stringstream seqStr(topLevelGroupName);
                  seqStr >> seqID;
                  // sequence ID in artdaq::Fragment header is only 48 bits
                  seqID &= 0xffffffffffff;

                  fragID = sampleFragmentID++;

                  if (attrExists(requestedGroup,"trigger_timestamp"))
                    {
                      uint64_t timestamp;
                      hid_t a1 = H5Aopen_name(requestedGroup,"trigger_timestamp");
                      H5Aread(a1, H5Aget_type(a1), &timestamp);
                      H5Aclose(a1);
                      headerTimestamp = timestamp;
                    }
                  else
                    {
                      std::stringstream timeStr(topLevelGroupName);
                      timeStr >> headerTimestamp;
                    }

                  frag->setUserType(type);
                  frag->setSequenceID(seqID);
                  frag->setFragmentID(fragID);
                  frag->setTimestamp(headerTimestamp);

                  //std::cout << "Attempting to determine sequence ID " << topLevelGroupName << " " << seqID
                  //          << " " << ((uint64_t)frag->sequenceID()) << std::endl;

                  //TLOG(TLVL_READFRAGMENT_V) << "readFragment_: Reading payload data into Fragment BEGIN";
                  //theDataSet.read(frag->dataBeginBytes() + payloadOffset, theDataSet.getDataType());
		  memcpy(frag->dataBeginBytes() + payloadOffset, ds_data, ds_size);

                  if (detectorTypeName == PDS_GROUP_NAME)
                    {
                      memcpy(frag->dataBeginBytes(), &pdsPayloadHeaderWord, sizeof(uint64_t));
                    }

                  fragmentMap[detectorTypeName]->emplace_back(std::move(*(frag.release())));

		  H5Dclose(datasetid);
		  delete[] ds_data;  // free up memory

                }
            }
            H5Gclose(subdetGroup);
          }
      }

      H5Gclose(requestedGroup);

      //hdfFileInfoPtr->filePtr->flush(H5F_SCOPE_GLOBAL);
      return fragmentMap;
    }

  } // namespace HDFFileReader
}   // namespace dune
