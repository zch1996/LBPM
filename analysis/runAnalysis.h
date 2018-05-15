#ifndef RunAnalysis_H_INC
#define RunAnalysis_H_INC

#include "analysis/analysis.h"
#include "analysis/TwoPhase.h"
#include "common/Communication.h"
#include "common/ScaLBL.h"
#include "threadpool/thread_pool.h"


typedef std::shared_ptr<std::pair<int,IntArray>> BlobIDstruct;
typedef std::shared_ptr<std::vector<BlobIDType>> BlobIDList;


// Types of analysis
enum class AnalysisType : uint64_t { AnalyzeNone=0, IdentifyBlobs=0x01, CopyPhaseIndicator=0x02, 
    CopySimState=0x04, ComputeAverages=0x08, CreateRestart=0x10, WriteVis=0x20 };


//! Class to run the analysis in multiple threads
class runAnalysis
{
public:

    //! Constructor
    runAnalysis( std::shared_ptr<Database> db, const RankInfoStruct& rank_info,
        const ScaLBL_Communicator &ScaLBL_Comm, const Domain& dm, int Np, bool pBC, double beta, IntArray Map );

    //! Destructor
    ~runAnalysis();

    //! Run the next analysis
    void run( int timestep, TwoPhase& Averages, const double *Phi,
        double *Pressure, double *Velocity, double *fq, double *Den );

    //! Finish all active analysis
    void finish();

    /*!
     *  \brief    Set the affinities
     *  \details  This function will create the analysis threads and set the affinity
     *      of this thread and all analysis threads.  If MPI_THREAD_MULTIPLE is not
     *      enabled, the analysis threads will be disabled and the analysis will run in the current thread.
     * @param[in] method    Method used to control the affinities:
     *                      none - Don't use threads (runs all analysis in the current thread)
     *                      default - Create the specified number of threads, but don't load balance
     *                      independent - Create the necessary number of threads to fill all cpus,
     *                                and set the affinities based on the current process such
     *                                that all threads run on independent cores
     * @param[in] N_threads Number of threads, only used by some of the methods
     */
    void createThreads( const std::string& method = "default", int N_threads = 4 );


private:

    runAnalysis();

    // Determine the analysis to perform
    AnalysisType computeAnalysisType( int timestep );

public:

    class commWrapper
    {
      public:
        MPI_Comm comm;
        int tag;
        runAnalysis *analysis;
        commWrapper( int tag, MPI_Comm comm, runAnalysis *analysis );
        commWrapper( ) = delete;
        commWrapper( const commWrapper &rhs ) = delete;
        commWrapper& operator=( const commWrapper &rhs ) = delete;
        commWrapper( commWrapper &&rhs );
        ~commWrapper();
    };

    // Get a comm (not thread safe)
    commWrapper getComm( );

private:

    int d_N[3];
    int d_Np;
    int d_rank;
    int d_restart_interval, d_analysis_interval, d_blobid_interval, d_visualization_interval;
    double d_beta;
    ThreadPool d_tpool;
    ScaLBL_Communicator d_ScaLBL_Comm;
    RankInfoStruct d_rank_info;
    IntArray d_Map;
    BlobIDstruct d_last_ids;
    BlobIDstruct d_last_index;
    BlobIDList d_last_id_map;
    std::vector<IO::MeshDataStruct> d_meshData;
    fillHalo<double> d_fillData;
    std::string d_restartFile;
    MPI_Comm d_comm;
    MPI_Comm d_comms[1024];
    volatile bool d_comm_used[1024];

    // Ids of work items to use for dependencies
    ThreadPool::thread_id_t d_wait_blobID;
    ThreadPool::thread_id_t d_wait_analysis;
    ThreadPool::thread_id_t d_wait_vis;
    ThreadPool::thread_id_t d_wait_restart;

    // Friends
    friend commWrapper::~commWrapper();

};

#endif
