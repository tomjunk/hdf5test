#ifndef dune_raw_data_HDFUtils_HDFFileReader_hh
#define dune_raw_data_HDFUtils_HDFFileReader_hh

#include "artdaq-core/Data/Fragment.hh"
#include <hdf5.h>
#include <memory>
#include <string>

namespace dune {
  namespace HDFFileReader {

    // *****************************************
    // *** General-purpose data and routines ***
    // *****************************************

    struct HDFFileInfo {
      hid_t filePtr;
      size_t bytesWritten;
      std::string fileName;
      int runNumber;
    };
    typedef std::unique_ptr<HDFFileInfo> HDFFileInfoPtr;

    typedef std::map<std::string, std::unique_ptr<artdaq::Fragments> > FragmentListsByType;

    /**
     * Opens an HDF file (for reading) with the specified filename (including path information).
     */
    HDFFileInfoPtr openFile(const std::string& fileName);

    /**
     * Closes and re-opens the specified file.
     */
    HDFFileInfoPtr reopenFile(HDFFileInfoPtr oldHdfFileInfoPtr);

    /**
     * Closes the specified HDF file.  To call this method, user code should use
     * closeFile(std::move(fileInfoPtr));
     * The intention behind this is to make it clear that the user code is 
     * relinquishing control.
     */
    void closeFile(HDFFileInfoPtr hdfFileInfoPtr);

    std::list<std::string> getTopLevelGroupNames(HDFFileInfoPtr& hdfFileInfoPtr);

    std::list<std::string> getMidLevelGroupNames(hid_t gid);

    /**
     * Gets the requested Group ID from the specified file and group path.
     */
    hid_t getGroupFromPath(HDFFileInfoPtr& hdfFileInfoPtr, const std::string& groupPath);

    bool attrExists(hid_t object, const std::string& attrname);

    // ***************************************
    // *** FELIX-related data and routines ***
    // ***************************************

    typedef uint64_t FELIX_DATASET_DATATYPE;
    const std::string FELIX_GROUP_NAME = "FELIX";


    // ***************************************
    // *** PDS-related data and routines ***
    // ***************************************

    typedef uint64_t PDS_DATASET_DATATYPE;
    const std::string PDS_GROUP_NAME = "PHOTON";


    // ************************************************
    // *** Trigger-Record-related data and routines ***
    // ************************************************

    FragmentListsByType getFragmentsForEvent(HDFFileInfoPtr& hdfFileInfoPtr, const std::string& topLevelGroupName);


  } // namespace HDFFileReader
}   // namespace dune

#endif /* dune_raw_data_HDFUtils_HDFFileReader_hh */
