#include <mpi.h>
#include <thread> // this_thread::sleep_for
#include <random> // dispositivos, generadores y distribuciones aleatorias
#include <chrono> // duraciones (duration), unidades de tiempo
#include <iostream>
#include <complex>
#include <vector>

using namespace std;
using namespace std::this_thread ;
using namespace std::chrono ;


const int num_esclavos = 3 ,
  num_procesos = num_esclavos+1 , 
  id_maestro   = num_esclavos ,
  num_filas    = 50 ,
  num_colum    = 50 ,
  etiq_fila    = 1  ,
  etiq_fin     = 2 ;


vector< char > recive_fila( int id )
{
  vector< char > contenedor;
  MPI_Status estado ;
  char valor;

  for ( int i = 0; i < num_colum; ++i)
    {
      MPI_Recv ( &valor, 1, MPI_CHAR, id, 0, MPI_COMM_WORLD, &estado );
      contenedor.push_back( valor );
    }

  return contenedor;
}


void envia_fila( vector< char > fila ) //Solo recive el maestro
{
  char valor;

  for ( int i = 0; i < num_colum; ++i)
    {
      valor = fila[i];
      MPI_Send ( &valor, 1, MPI_CHAR, id_maestro, 0, MPI_COMM_WORLD );
    }  
}


void funcion_maestro()
{
  int filas_enviadas = 0, filas_rellenas = 0;
  int valor = 0;
  MPI_Status estado ;
  vector< vector< char > > mandel ( num_filas, vector< char >());

  //Reparte una fila por esclavo
  for ( int i = 0; i < num_esclavos && i < num_filas; ++i )
    {
      //Envia la fila que tiene que ser leida
      MPI_Send( &i, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
      ++filas_enviadas;
    }
  
  //Reparte una fila a cada esclavo que se libere
  while( filas_enviadas < num_filas )
    {
      //Recivimos que fila vamos a leer
      MPI_Recv( &valor, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &estado );

      //Recivimos la fila
      mandel[valor] = recive_fila( estado.MPI_SOURCE ); 
      ++filas_rellenas;

      //Repartimos una nueva fila.
      MPI_Send( &filas_enviadas, 1, MPI_INT, estado.MPI_SOURCE, etiq_fila, MPI_COMM_WORLD);
      ++filas_enviadas;
    }

  //Va matando a los esclavos
  for( int i = 0; i < num_esclavos; ++i )
    {
      //Recivimos que fila vamos a leer
      MPI_Recv( &valor, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &estado );

      //Recivimos la fila
      mandel[valor] = recive_fila( estado.MPI_SOURCE ); 
      ++filas_rellenas;

      //Matamos al proceso
      MPI_Ssend( &valor , 1, MPI_INT, estado.MPI_SOURCE, etiq_fin, MPI_COMM_WORLD);
    }
  
  
  if ( filas_rellenas==num_filas )
    {
      cout << "Ejecicion correcta. Solución: " << endl;
      for (int i = 0; i < num_filas; ++i )
	{
	  for(int j = 0; j < num_colum; ++j) 
	    cout << mandel[i][j] << ' ';
	  cout << endl;
  	}
    }

  else
    cout << "Error en el reparto de filas. "  << endl;
}

complex< double > funcion_mandelbrot ( complex< double > z, complex< double > c )
{
  return ((z*z) + c); 
}

complex< double > complejo ( double fila, double col )
{
  double img =  ((fila/num_filas) *(-2.0)) + 1;
  double real = ((col/num_colum) *3) - 2;

  complex< double > c (real, img);

  return c;
}


char complejo_pertenece ( int fila, int col, int id )
{
  const static int limite = 10000;
  complex< double > z(0,0);
  const static int radio = 100000;

  complex< double > c = complejo( fila, col );

  int n_iterations = 0;
  while( abs(z) < radio && n_iterations < limite )
    {
      z = funcion_mandelbrot(z,c);
      n_iterations++;
    }

  return (n_iterations == limite) ? '*' : ' ' ;
}



vector< char > color_fila ( int fila, int id )
{
  vector< char > result;
  
  for ( int i = 0; i < num_colum; ++i )
    result.push_back( complejo_pertenece( fila, i, id) );

  return result;
}


void funcion_esclavo(int id)
{
  int fila;
  MPI_Status estado ;
  MPI_Recv ( &fila, 1, MPI_INT, id_maestro, MPI_ANY_TAG, MPI_COMM_WORLD, &estado );

  while( estado.MPI_TAG != etiq_fin )
    {
      vector< char > colores = color_fila( fila, id );

      MPI_Ssend( &fila, 1, MPI_INT, id_maestro, 0, MPI_COMM_WORLD);
      envia_fila ( colores );
      
      MPI_Recv ( &fila, 1, MPI_INT, id_maestro, MPI_ANY_TAG, MPI_COMM_WORLD, &estado );
    }
	
} 


int main (int argc, char** argv)
{
  int id_propio, num_procesos_actual ;
  MPI_Init( &argc, &argv );
  MPI_Comm_rank( MPI_COMM_WORLD, &id_propio );
  MPI_Comm_size( MPI_COMM_WORLD, &num_procesos_actual );

  if (num_procesos_actual == num_procesos)
    {
      if (id_propio == id_maestro)
	funcion_maestro();
      else
	funcion_esclavo( id_propio );
    }
  else
    {
      if ( id_propio == 0 ) // solo el primero escribe error
	{
	  cout << "el número de procesos esperados es:    "
	       << num_procesos << endl
	       << "el número de procesos en ejecución es: "
	       << num_procesos_actual << endl
	       << "(programa abortado)" << endl ;
	}
    }

  MPI_Finalize( );
  return 0;
}
   

