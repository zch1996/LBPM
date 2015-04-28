#ifndef COMMUNICATION_HPP_INC
#define COMMUNICATION_HPP_INC

#include "common/Communication.h"
#include "common/MPI_Helpers.h"
#include "ProfilerApp.h"


/********************************************************
*  Structure to store the rank info                     *
********************************************************/
template<class TYPE>
fillHalo<TYPE>::fillHalo( const RankInfoStruct& info0, int nx0, int ny0, int nz0, 
    int ngx0, int ngy0, int ngz0, int tag0, int depth0,
    bool fill_face, bool fill_edge, bool fill_corner ):
    info(info0), nx(nx0), ny(ny0), nz(nz0), ngx(ngx0), ngy(ngy0), ngz(ngz0), depth(depth0)
{
    comm = MPI_COMM_WORLD;
    datatype = getMPItype<TYPE>();
    // Set the fill pattern
    memset(fill_pattern,0,sizeof(fill_pattern));
    if ( fill_face ) {
        fill_pattern[0][1][1] = true;
        fill_pattern[2][1][1] = true;
        fill_pattern[1][0][1] = true;
        fill_pattern[1][2][1] = true;
        fill_pattern[1][1][0] = true;
        fill_pattern[1][1][2] = true;
    }
    if ( fill_edge ) {
        fill_pattern[0][0][1] = true;
        fill_pattern[0][2][1] = true;
        fill_pattern[2][0][1] = true;
        fill_pattern[2][2][1] = true;
        fill_pattern[0][1][0] = true;
        fill_pattern[0][1][2] = true;
        fill_pattern[2][1][0] = true;
        fill_pattern[2][1][2] = true;
        fill_pattern[1][0][0] = true;
        fill_pattern[1][0][2] = true;
        fill_pattern[1][2][0] = true;
        fill_pattern[1][2][2] = true;
    }
    if ( fill_corner ) {
        fill_pattern[0][0][0] = true;
        fill_pattern[0][0][2] = true;
        fill_pattern[0][2][0] = true;
        fill_pattern[0][2][2] = true;
        fill_pattern[2][0][0] = true;
        fill_pattern[2][0][2] = true;
        fill_pattern[2][2][0] = true;
        fill_pattern[2][2][2] = true;
    }
    // Determine the number of elements for each send/recv
    for (int i=0; i<3; i++) {
        int ni = (i-1)==0 ? nx:ngx;
        for (int j=0; j<3; j++) {
            int nj = (j-1)==0 ? ny:ngy;
            for (int k=0; k<3; k++) {
                int nk = (k-1)==0 ? nz:ngz;
                if ( fill_pattern[i][j][k] )
                    N_send_recv[i][j][k] = ni*nj*nk;
                else
                    N_send_recv[i][j][k] = 0;
            }
        }
    }
    // Create send/recv buffers
    size_t N_mem=0;
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++)
                N_mem += N_send_recv[i][j][k];
        }
    }
    mem = new TYPE[2*depth*N_mem];
    size_t index = 0;
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++) {
                send[i][j][k] = &mem[index];
                index += depth*N_send_recv[i][j][k];
                recv[i][j][k] = &mem[index];
                index += depth*N_send_recv[i][j][k];
            }
        }
    }
    // Create the tags
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++) {
                tag[i][j][k] = tag0 + i + j*3 + k*9;
            }
        }
    }

}
template<class TYPE>
fillHalo<TYPE>::~fillHalo( )
{
    delete [] mem;
}
template<class TYPE>
void fillHalo<TYPE>::fill( Array<TYPE>& data )
{
    PROFILE_START("fillHalo::fill",1);
    int depth2 = data.size(3);
    ASSERT((int)data.size(0)==nx+2*ngx);
    ASSERT((int)data.size(1)==ny+2*ngy);
    ASSERT((int)data.size(2)==nz+2*ngz);
    ASSERT(depth2<=depth);
    ASSERT(data.ndim()==3||data.ndim()==4);
    // Start the recieves
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++) {
                if ( !fill_pattern[i][j][k] )
                    continue;
                MPI_Irecv( recv[i][j][k], depth2*N_send_recv[i][j][k], datatype, 
                    info.rank[i][j][k], tag[2-i][2-j][2-k], comm, &recv_req[i][j][k] );
            }
        }
    }
    // Pack the src data and start the sends
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++) {
                if ( !fill_pattern[i][j][k] )
                    continue;
                pack( data, i-1, j-1, k-1, send[i][j][k] );
                MPI_Isend( send[i][j][k], depth2*N_send_recv[i][j][k], datatype, 
                    info.rank[i][j][k], tag[i][j][k], comm, &send_req[i][j][k] );
            }
        }
    }
    // Recv the dst data and unpack
    MPI_Status status;
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++) {
                if ( !fill_pattern[i][j][k] )
                    continue;
                MPI_Wait(&recv_req[i][j][k],&status);
                unpack( data, i-1, j-1, k-1, recv[i][j][k] );
            }
        }
    }
    // Wait until all sends have completed
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++) {
                if ( !fill_pattern[i][j][k] )
                    continue;
                MPI_Wait(&send_req[i][j][k],&status);
            }
        }
    }
    PROFILE_STOP("fillHalo::fill",1);
}
template<class TYPE>
void fillHalo<TYPE>::pack( const Array<TYPE>& data, int i0, int j0, int k0, TYPE *buffer )
{
    int depth2 = data.size(3);
    int ni = i0==0 ? nx:ngx;
    int nj = j0==0 ? ny:ngy;
    int nk = k0==0 ? nz:ngz;
    int is = i0==0 ? ngx:((i0==-1)?ngx:nx);
    int js = j0==0 ? ngy:((j0==-1)?ngy:ny);
    int ks = k0==0 ? ngz:((k0==-1)?ngz:nz);
    for (int d=0; d<depth2; d++) {
        for (int k=0; k<nk; k++) {
            for (int j=0; j<nj; j++) {
                for (int i=0; i<ni; i++) {
                    buffer[i+j*ni+k*ni*nj+d*ni*nj*nk] = data(i+is,j+js,k+ks,d);
                }
            }
        }
    }
}
template<class TYPE>
void fillHalo<TYPE>::unpack( Array<TYPE>& data, int i0, int j0, int k0, const TYPE *buffer )
{
    int depth2 = data.size(3);
    int ni = i0==0 ? nx:ngx;
    int nj = j0==0 ? ny:ngy;
    int nk = k0==0 ? nz:ngz;
    int is = i0==0 ? ngx:((i0==-1)?0:nx+ngx);
    int js = j0==0 ? ngy:((j0==-1)?0:ny+ngy);
    int ks = k0==0 ? ngz:((k0==-1)?0:nz+ngz);
    for (int d=0; d<depth2; d++) {
        for (int k=0; k<nk; k++) {
            for (int j=0; j<nj; j++) {
                for (int i=0; i<ni; i++) {
                    data(i+is,j+js,k+ks,d) = buffer[i+j*ni+k*ni*nj+d*ni*nj*nk];
                }
            }
        }
    }
}


#endif