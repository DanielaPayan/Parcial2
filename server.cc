#include <iostream>
#include <czmq.h>
#include <string.h>
#include <vector>
#include <utility>
#include <unordered_map>
#include <dirent.h>

using namespace std;
vector<string> canciones;
vector<string> pautas;
int reproTotal=0; //Reproducciones totales
int contPautas=0;
unordered_map<string,int> cancionCantidad; //Reproducciones por canciones
string directotioMusica;


void listar(){
  cout<<"Lista de archivos"<<endl;
  DIR *dir;
  struct dirent *ent;
  dir = opendir (directotioMusica.c_str());
  if (dir == NULL) 
   cout<<"No puedo abrir el directorio"<<endl;
  while ((ent = readdir (dir)) != NULL) {
      /* Nos devolverá el directorio actual (.) y el anterior (..), como hace ls */
      if ( (strcmp(ent->d_name, ".")!=0) && (strcmp(ent->d_name, "..")!=0) ){
      /* Una vez tenemos el archivo, lo pasamos a una función para procesarlo. */
        cout<<ent->d_name<<endl;
        canciones.push_back(ent->d_name);
      }
  }
  closedir (dir);
}

void adicionarCanciones(zmsg_t* msg,string carperta,string ddir,zmsg_t*msg2){
  cout<<"Lista de archivos"<<endl;
  DIR *dir;
  struct dirent *ent;
  //dir = opendir ("./music");
  dir = opendir (carperta.c_str());
  if (dir == NULL) 
   cout<<"No puedo abrir el directorio"<<endl;
  while ((ent = readdir (dir)) != NULL) {
      /* Nos devolverá el directorio actual (.) y el anterior (..), como hace ls */
      if ( (strcmp(ent->d_name, ".")!=0) && (strcmp(ent->d_name, "..")!=0) ){
      /* Una vez tenemos el archivo, lo pasamos a una función para procesarlo. */
        cout<<ent->d_name<<endl;
        canciones.push_back(ent->d_name);
      }
  }
  closedir (dir);

  //Creo el mensaje
  int tam=canciones.size();
  zmsg_addstr(msg,to_string(tam).c_str());
  zmsg_addstr(msg2,ddir.c_str()); //Para repserver direccion
  zmsg_addstr(msg2,to_string(tam).c_str()); //Para repserver
  for (int i = 0; i < tam; i++){
  	zmsg_addstr(msg,canciones[i].c_str());
    zmsg_addstr(msg2,canciones[i].c_str()); //Para repserver
    zmsg_addstr(msg,ddir.c_str());
  }

  cout<<"Mensaje de las canciones del server\n";
  zmsg_print(msg);


  cout<<"Lista de pautas"<<endl;
  DIR *dirp;
  struct dirent *entp;
  //string dirPauta=carperta+"pautas/";
  dirp = opendir ("./pautas/");
  if (dirp == NULL) 
   cout<<"No puedo abrir el directorio"<<endl;
  while ((entp = readdir (dirp)) != NULL) {
      /* Nos devolverá el directorio actual (.) y el anterior (..), como hace ls */
      if ( (strcmp(entp->d_name, ".")!=0) && (strcmp(entp->d_name, "..")!=0) ){
      /* Una vez tenemos el archivo, lo pasamos a una función para procesarlo. */
        cout<<entp->d_name<<endl;
        pautas.push_back(entp->d_name);
      }
  }
  closedir (dirp);
}

void handleClientMessage(zmsg_t*msg,void* client,string carpeta){
  cout<<"Recibo del cliente\n";
  zmsg_print(msg);
	string fname(zmsg_popstr (msg));

  zfile_t *file = zfile_new(carpeta.c_str(),fname.c_str());

  cout << "File is readable? " << zfile_digest(file) << endl;  // checkSum
  zfile_close(file);

  string path=carpeta + fname; 
  directotioMusica=carpeta;
  zchunk_t *chunk = zchunk_slurp(path.c_str(),0);

  if(!chunk) {
    cout << "Cannot read file!" << endl;
  }else{
    cout << "Chunk size: " << zchunk_size(chunk) << endl;

    cout << "Chunk size: " << zchunk_size(chunk) << endl;
    zframe_t *frame = zframe_new(zchunk_data(chunk), zchunk_size(chunk));
    zmsg_t* mp3=zmsg_new();
    zmsg_prepend (mp3, &frame);

    //Debo poner una pauta
    if(contPautas==2){
      int np=pautas.size();
      int ra=rand() % np;

      zmsg_addstr(mp3,"SI");
      string dirPauta="./pautas/";
      zfile_t *filep = zfile_new(dirPauta.c_str(),pautas[ra].c_str());
      cout << "File is readable? " << zfile_digest(filep) << endl;  // checkSum
      zfile_close(filep);

      string pathp=dirPauta +pautas[ra]; 
      zchunk_t *chunkp = zchunk_slurp(pathp.c_str(),0);
      zframe_t *framep = zframe_new(zchunk_data(chunkp), zchunk_size(chunkp));
      zmsg_append (mp3, &framep);
      zmsg_send(&mp3,client);
      contPautas=0;
    }else{
      zmsg_addstr(mp3,"NO");
      zmsg_send(&mp3,client);
      contPautas=contPautas+1;
    }

    
  
    cout << "will end!" << endl;
    //Garantizo que solo me queden guardadas las que fueron reproducidas no todas las que se encuentran en este servidor
    if(cancionCantidad.count(fname)>0){ //Si ya esta sumo uno.
      int temp=cancionCantidad[fname];  
      cancionCantidad[fname]=temp+1;
    }else{
      cancionCantidad[fname]=1; //Si no creo esa entrada en la tabla hash con una reproduccion
    }
    
    reproTotal=reproTotal+1;
    cout<<"reproTotal"<<reproTotal<<endl;
  }
}

void handleRepServerMessage(zmsg_t* msg, void* repServer){
  cout<<"Desde el repServer\n";
  zmsg_print(msg);

  string code(zmsg_popstr(msg));
  if(code.compare("query")==0){
    zmsg_t* toRepServer=zmsg_new();
    zmsg_addstr(toRepServer,to_string(reproTotal).c_str()); //Total de reproducciones en este servidor
    int nC=cancionCantidad.size(); 
    zmsg_addstr(toRepServer,to_string(nC).c_str()); //Cantidad de canciones reproducidas

    //Solo las canciones que han sido reproducidas
    for ( auto it = cancionCantidad.begin(); it != cancionCantidad.end(); ++it ){
      string song= it->first;
      int nroReproducciones=it->second;

      zmsg_addstr(toRepServer,song.c_str()); //cancion
      zmsg_addstr(toRepServer,to_string(nroReproducciones).c_str()); //numero de reproducciones de esa cancion    

    }

    cout<<"Mensaje a enviar el repServer\n";
    zmsg_print(toRepServer);
    zmsg_send(&toRepServer,repServer);
    
  }else if(code.compare("replicar")==0){
    string code2(zmsg_popstr(msg));
    if(code2.compare("SI")==0){
      //Transfiero el archivo

      string fname(zmsg_popstr (msg));
      zfile_t *file = zfile_new(directotioMusica.c_str(),fname.c_str());
      zfile_close(file);
      string path=directotioMusica+fname;
      zchunk_t *chunk = zchunk_slurp(path.c_str(),0);
      zmsg_t* transferir=zmsg_new();
      if(!chunk) {
        cout << "Cannot read file!" << endl;
        zmsg_addstr(transferir,"ERROR");
        zmsg_send(&transferir,repServer);
      }else{   
        zmsg_addstr(transferir,"OK");
        zmsg_addstr(transferir,fname.c_str());
        zframe_t *frame = zframe_new(zchunk_data(chunk), zchunk_size(chunk));
        zmsg_append(transferir, &frame);
        zmsg_send(&transferir,repServer);
        }
    }
  }else if(code.compare("transferencia")==0){
    string fname(zmsg_popstr(msg));
    cout<<"Directorio en el que se va a replicar la cancion: "<<directotioMusica<<"Cancion: "<<fname<<endl;
    zfile_t *download = zfile_new(directotioMusica.c_str(),fname.c_str());
    zfile_output(download);

    zframe_t *filePart =zmsg_pop(msg);
    zchunk_t *chunk = zchunk_new(zframe_data(filePart), zframe_size(filePart)); 
    zfile_write(download, chunk, 0);
    zfile_close(download);

    zmsg_t* msgtransfer=zmsg_new();
    zmsg_addstr(msgtransfer,"OK");
    zmsg_send(&msgtransfer,repServer);
    cout<<"Se hizo una transferencia, verifico que el nuevo archivo este\n";
    listar();

    //Agrego la cancion transferida a los registros de cancion que tengo 
    canciones.push_back(fname);
    cancionCantidad[fname]=0;
  }


}

int main(int argc, char** argv){
  cout << "Server agent starting..." << endl;
  if (argc != 3) {
    cerr << "Wrong call\n";
    return 1;
  }
  string carperta(argv[1]);
  directotioMusica=argv[1];
  zctx_t* context = zctx_new();

  void* client = zsocket_new(context,ZMQ_REP);
  char *jjj=argv[2]; //"tcp://*:4444";
  int ccc=zsocket_bind(client, jjj);
  //cout << "connecting to client: " << (ccc == 0 ? "OK" : "ERROR") << endl;

  void* broker = zsocket_new(context, ZMQ_DEALER);
  int c = zsocket_connect(broker, "tcp://localhost:5556"); 
  cout << "connecting to broker: " << (c == 0 ? "OK" : "ERROR") << endl;

  void* repServer = zsocket_new(context, ZMQ_DEALER);
  int cc = zsocket_connect(repServer, "tcp://localhost:5557"); 
  cout << "connecting to repServer: " << (cc == 0 ? "OK" : "ERROR") << endl;




  string aa(argv[2]);

  zmsg_t* msg2broker = zmsg_new();
  zmsg_addstr(msg2broker, "register");
  string ddir(argv[2]);

  zmsg_t* msg2repServer=zmsg_new();
  zmsg_addstr(msg2repServer, "register");
  adicionarCanciones(msg2broker,carperta,ddir,msg2repServer); 

  zmsg_send(&msg2broker, broker); ///Registro de canciones en el broker
  zmsg_send(&msg2repServer,repServer);  //Registro de canciones en el repServer

  cout<<"Se envio el mensaje al broker\n";


  zmq_pollitem_t items[] = {{client, 0, ZMQ_POLLIN, 0},	{repServer, 0, ZMQ_POLLIN, 0}};

  while (true) {
    zmq_poll(items, 2, 10 * ZMQ_POLL_MSEC);
    if (items[0].revents & ZMQ_POLLIN) {
      zmsg_t* msg = zmsg_recv(client);
      handleClientMessage(msg, client, carperta);
      cout<<"Reproduciones totales"<<reproTotal<<endl;

    }
    if (items[1].revents & ZMQ_POLLIN) {
      zmsg_t* msg = zmsg_recv(repServer);
      handleRepServerMessage(msg,repServer);
    }
    
  }
  zctx_destroy(&context);
  return 0;
}