#include <iostream>
#include <math.h>
#include "analysis/Minkowski.h"
#include "common/Domain.h"
#include "common/SpherePack.h"

using namespace std;

/*
 *  Compare the measured and analytical curvature for a sphere
 *
 */

std::shared_ptr<Database> loadInputs( )
{
  //auto db = std::make_shared<Database>( "Domain.in" );
    auto db = std::make_shared<Database>();
    db->putScalar<int>( "BC", 0 );
    db->putVector<int>( "nproc", { 1, 1, 1 } );
    db->putVector<int>( "n", { 16, 16, 16 } );
    db->putScalar<int>( "nspheres", 1 );
    db->putVector<double>( "L", { 1, 1, 1 } );
    return db;
}

int main(int argc, char **argv)
{
	MPI_Init(&argc,&argv);
	MPI_Comm comm = MPI_COMM_WORLD;
	int rank = MPI_WORLD_RANK();
	int nprocs = MPI_WORLD_SIZE();
	int toReturn = 0;
	{
		int i,j,k;

		// Load inputs
		auto db = loadInputs( );
		int Nx = db->getVector<int>( "n" )[0];
		int Ny = db->getVector<int>( "n" )[1];
		int Nz = db->getVector<int>( "n" )[2];
		std::shared_ptr<Domain> Dm = std::shared_ptr<Domain>(new Domain(db,comm));
		
		Nx+=2; Ny+=2; Nz+=2;
		DoubleArray SDs(Nx,Ny,Nz);
		DoubleArray SDs_x(Nx,Ny,Nz);
		DoubleArray SDs_y(Nx,Ny,Nz);
		DoubleArray SDs_z(Nx,Ny,Nz);

		printf("Set distance map \n");
		for (k=0; k<Nz; k++){
			for (j=0; j<Ny; j++){
				for (i=0; i<Nx; i++){
					SDs(i,j,k) = sqrt((1.0*i-0.5*Nx)*(1.0*i-0.5*Nx)+(1.0*j-0.5*Ny)*(1.0*j-0.5*Ny)+(1.0*k-0.5*Nz)*(1.0*k-0.5*Nz))-0.3*Nx;
				}
			}
		}
		pmmc_MeshGradient(SDs,SDs_x,SDs_y,SDs_z,Nx,Ny,Nz);

		DECL object;
		Point P1,P2,P3;
		Point U,V,W;
		unsigned long int e1,e2,e3;
		double s,s1,s2,s3;
		double a1,a2,a3;
		double Vx,Vy,Vz,Wx,Wy,Wz,nx,ny,nz,norm;
		double isovalue = 0.f;
		for (int k=1; k<Nz-1; k++){
			for (int j=1; j<Ny-1; j++){
				for (int i=1; i<Nx-1; i++){
					object.LocalIsosurface(SDs,isovalue,i,j,k);
					for (unsigned long int idx=0; idx<object.TriangleCount; idx++){
						e1 = object.Face(idx); 
						U = object.TriNormal(e1);
						// normal from gradient
						nx = SDs_x(i,j,k);
						ny = SDs_y(i,j,k);
						nz = SDs_z(i,j,k);
						double dotprod=U.x*nx + U.y*ny + U.z*nz;
						if (dotprod < 0){
							printf("negative %f \n",dotprod);
						}
						else{
							printf("positive %f \n",dotprod);
						}
					}
				}
			}
		}
	}
	MPI_Barrier(comm);
	MPI_Finalize();
	return toReturn;
}