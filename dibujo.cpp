#include <mpi.h>
#include <thread> // this_thread::sleep_for
#include <chrono> // duraciones (duration), unidades de tiempo
#include <iostream>
#include <complex>
#include <vector>
#include "png++/png.hpp"



using namespace png;
using namespace std;
using namespace std::this_thread ;
using namespace std::chrono ;


const int num_esclavos = 3 ,
  num_procesos = num_esclavos+1 , 
  id_maestro   = num_esclavos  ,
  limite       = 1000          ,
  escala       = 1             ,
  num_filas    = 682  * escala ,
  num_colum    = 1024 * escala ,
  etiq_fila    = 1             ,
  etiq_fin     = 2 ;

//*******************************************************************
//******** Funciones para la recepción y envío de filas *************

//-------------
//Recibe una fila en formato int y la transforma en pixel
vector< rgb_pixel > recibe_fila( int id )
{
  vector< rgb_pixel > contenedor;
  MPI_Status estado ;
  int valor;

  for ( int i = 0; i < num_colum; ++i)
    {
      //recibe el intervalor
      MPI_Recv ( &valor, 1, MPI_INT, id, 0, MPI_COMM_WORLD, &estado );

      //Si pertenece lo deja negro, si no, lo colorea
      if( valor == limite )
	contenedor.push_back( rgb_pixel( 0, 0, 0 ) );
      else
	contenedor.push_back( rgb_pixel( 0, valor*23 , valor*23 ) );
    }

  return contenedor;
}

//-------------
//Envía un vector de int
void envia_fila( vector< int > fila ) //Solo recibe el maestro
{
  int valor;

  for ( int i = 0; i < num_colum; ++i)
    {
      //coge el valor y lo envía al maestro
      valor = fila[i];
      MPI_Send ( &valor, 1, MPI_INT, id_maestro, 0, MPI_COMM_WORLD );
    }  
}


//*******************************************************************
//******** Funciones para el funcionamiento del maestro *************

//-------------
//Función que gestiona el funcionamiento del proceso maestro
void funcion_maestro()
{
  int filas_enviadas = 0,  filas_rellenas = 0;
  int valor = 0;
  MPI_Status estado ;

  png::image< png::rgb_pixel > mandel(num_colum, num_filas);

  
  //Reparte una fila por esclavo
  for ( int i = 0; i < num_esclavos && i < num_filas; ++i )
    {
      MPI_Send( &i, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
      ++filas_enviadas;

    }

  //Reparte una fila a cada esclavo que se libere
  while( filas_enviadas < num_filas )
    {
      //Recibimos que fila vamos a leer
      MPI_Recv( &valor, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &estado );

      //Recibimos la fila
       mandel[valor] = recibe_fila( estado.MPI_SOURCE ); 
      ++filas_rellenas;

      //enviamos nueva fila
      MPI_Send( &filas_enviadas, 1, MPI_INT, estado.MPI_SOURCE, etiq_fila, MPI_COMM_WORLD);
      ++filas_enviadas;
    }
 
  for( int i = 0; i < num_esclavos; ++i )
    {
      //Recibimos que fila vamos a leer
      MPI_Recv( &valor, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &estado );

      //Recibimos la fila
      mandel[valor] = recibe_fila( estado.MPI_SOURCE ); 
      ++filas_rellenas;

      //Matamos al proceso
      MPI_Ssend( &valor , 1, MPI_INT, estado.MPI_SOURCE, etiq_fin, MPI_COMM_WORLD);
      cout << "Al esclavo " << estado.MPI_SOURCE
	   << " se le dió un clacetin." << endl;
    }
  

  if ( filas_rellenas==num_filas )
    {
      cout << "Ejecicion correcta. " << endl;
       mandel.write( "Sol.png" );
    }

  else
    cout << "Error en el reparto de filas. "  << endl;
}

//*******************************************************************
//******** Funciones para el funcionamiento cliente *****************

//-------------
//Función sobre la que iterar los complejos
complex< double > funcion_mandelbrot ( complex< double > z, complex< double > c )
{
  return ((z*z) + c); 
}

//-------------
//Función que transforma una posición en la imagen a un complejo
complex< double > complejo ( double fila, double col )
{
  double img =  ((fila/num_filas) *(-2.0)) + 1;
  double real = ((col/num_colum) *3) - 2;

  complex< double > c (real, img);
  return c;
}

//-------------
//Función para ver si un complejo pertenece al conjunto
int complejo_pertenece ( int fila, int col )
{
  complex< double > z(0,0);
  const static int radio = 1000;

  complex< double > c = complejo( fila, col );

  int n_iterations = 0;
  while( abs(z) < radio && n_iterations < limite )
    {
      z = funcion_mandelbrot(z,c);
      n_iterations++;
    }
   
  return n_iterations;
}

//-------------
//Devuelve una fila por las iteraciones realizadas sobre ella
vector< int > color_fila ( int fila, int id )
{
  vector< int > result;
  
  for ( int i = 0; i < num_colum; ++i )
    result.push_back( complejo_pertenece( fila, i ) );

  return result;

}

//-------------
//Función que gestiona el funcionamiento del proceso esclavo
void funcion_esclavo(int id)
{
  int fila;
  MPI_Status estado ;

  //Recibe una fila (suponemos que nunca habrá más esclavos que filas)
  MPI_Recv ( &fila, 1, MPI_INT, id_maestro, MPI_ANY_TAG, MPI_COMM_WORLD, &estado );

  while( estado.MPI_TAG != etiq_fin )
    {
      vector< int > colores = color_fila( fila, id );

      //Envía que fila ha computado y, después, la fila 
      MPI_Ssend( &fila, 1, MPI_INT, id_maestro, 0, MPI_COMM_WORLD);
      envia_fila ( colores );

      //Recibe que hacer a continuación
      MPI_Recv ( &fila, 1, MPI_INT, id_maestro, MPI_ANY_TAG, MPI_COMM_WORLD, &estado );
    }
	
} 

//-----------------------------------------------------------------------

//-------------
//Main
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
   

