#include <iostream>
#include <czmq.h>
#include <string.h>
#include <vector>
#include <utility>  
#include <unordered_map>
#include <thread>
#include <chrono>
#include <ao/ao.h>
#include <mpg123.h>
#include <thread> 
#include <set>


using namespace std;


#define BITS 8

class SimplePlayer {
private:
  int driver;
  mpg123_handle *mh;
  ao_device* dev;
  //unsigned char *buffer;
  size_t buffer_size;
  bool stop_;
  
public: 
  SimplePlayer(void) {
  ao_initialize();
  driver = ao_default_driver_id(); 
  mpg123_init(); 
  int err;
    mh = mpg123_new(NULL, &err);
    buffer_size = mpg123_outblock(mh);
    stop_ = false;
  }
  
  ~SimplePlayer(void) {
  //free(buffer);
  if (dev)
    ao_close(dev);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();
  }
  
  std::thread play(const char* fname) {
  /* open the file and get the decoding format */
  mpg123_open(mh, fname);
    int channels, encoding;
    long rate;
    mpg123_getformat(mh, &rate, &channels, &encoding);
    
    /* set the output format and open the output device */
    ao_sample_format format;
    format.bits = mpg123_encsize(encoding) * BITS;
    format.rate = rate;
    format.channels = channels;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;
    dev = ao_open_live(driver, &format, NULL);
    
    return std::thread(actualPlay, mh,dev,buffer_size,stop_);
    //actualPlay(mh,dev,buffer_size);
  }
  
  void stop(void) {
    stop_ = true;
  }
  
  static void actualPlay(mpg123_handle *mh, ao_device* dev, size_t size, const bool& stop) {
    /* decode and play */
  size_t done;
    unsigned char *buffer = (unsigned char*) malloc(size * sizeof(unsigned char));
    while (mpg123_read(mh, buffer, size, &done) == MPG123_OK && !stop) {
    char * b = (char*)buffer;
        ao_play(dev, b, done);
  }
  free(buffer);
  ao_close(dev);
  mpg123_close(mh);
  }
};



int main(int argc, char** argv) {
  zctx_t* context = zctx_new();
  void* broker = zsocket_new(context,ZMQ_DEALER);
  zsocket_connect(broker, "tcp://localhost:5555");

  vector< pair< string ,string > > songaddress;
  unordered_map<string,string > sa;
  set<string> playlist;

  zmsg_t* query=zmsg_new();

  string code(argv[1]);
  if(code.compare("buscar")==0){
    zmsg_addstr(query,"query"); //code
    zmsg_addstr(query,argv[2]); //name
    cout<<"Mensaje desde el cliente\n";
    zmsg_print(query);
    zmsg_send(&query,broker);

    /*Respuesta del broker*/
    zmsg_t* answer=zmsg_recv(broker);
    cout<<"Mensaje de el broker\n";
    zmsg_print(answer);
    string op(zmsg_popstr(answer));
    
    if(op.compare("respuesta")==0){
      int n=atoi(zmsg_popstr(answer)); //Cantidad de canciones encontradas
      cout<<"Canciones encontradas: "<<endl;
      for (int i = 0; i < n; i++){
        //Pop de la cancion y la direccion del servidor
        string song(zmsg_popstr(answer));
        cout<<song<<endl;
        string address(zmsg_popstr(answer));
        sa[song]=address; 
      }

      string codeList(zmsg_popstr(answer)); //Para saber si se encontraron listas o no.
      if(codeList.compare("SI")==0){
        cout<<"Listas de reproduccion encontradas: "<<endl;
        int m=atoi(zmsg_popstr(answer)); // Cantidad de listas de reproduccion encontradas
        for (int i = 0; i <m;i++){
           string pl(zmsg_popstr(answer));
           playlist.insert(pl);
           cout<<pl<<endl;
        }

      }

      //selecciono una cancion
      cout<<"Escriba el nombre(con extencion) de su eleccion"<<endl;
      string selected;
      cin>>selected;

      //La eleccion es una lista
      if(playlist.count(selected)>0){
        zmsg_t* select=zmsg_new();
        zmsg_addstr(select,"queryList");
        zmsg_addstr(select,selected.c_str());
        zmsg_print(select);
        zmsg_send(&select,broker); //Envio de seleccion

        zmsg_t* answer=zmsg_recv(broker);
        cout<<"Lo que recibo del broker\n";
        string code(zmsg_popstr(answer));
        zmsg_print(answer);

        if(code.compare("lista")==0){
          int nLista=atoi(zmsg_popstr(answer));


          //Guardo las canciones de la lista de reproduccion en un vector para poder hacer siguiente, prev, stop
          for (int i = 0; i < nLista; i++){
            //Transferir cada cancion al cliente
            string songList(zmsg_popstr(answer));
            string addressList(zmsg_popstr(answer));
            songaddress.push_back(make_pair(songList,addressList));

          }
          pair< string ,string > song_address;
          for (int i = 0; i < nLista; i++){
            song_address=songaddress[i];
            string songList=song_address.first;
            string addressList=song_address.second;

            if(addressList[6]=='*'){
              addressList.replace(6,1,"localhost");
            }

            select=zmsg_new();
          //cout<<"Direccion a enviar el mensaje ";
            zmsg_addstr(select,songList.c_str());
            void* server = zsocket_new(context,ZMQ_REQ);
            zsocket_connect(server,addressList.c_str());
            cout<<"Mensaje para enviar al server. Direccion: "<<addressList<<" cancion: "<<songList<<endl;
            zmsg_print(select);
            zmsg_send(&select,server); //Envio de seleccion

            zfile_t *download = zfile_new("./received",songList.c_str());
            zfile_output(download);

            zmsg_t* respselect=zmsg_recv(server);
            cout<<"Respuesta del server\n";
            zmsg_print(respselect);
            zframe_t *filePart =zmsg_pop(respselect);
            zchunk_t *chunk = zchunk_new(zframe_data(filePart), zframe_size(filePart)); 
            zfile_write(download, chunk, 0);
            zfile_close(download);
            cout << "Complete " << zfile_digest(download) << endl;
            string linea="mpg123 ./received/"+songList;
            system(linea.c_str());
            cout<<"Desea continuar: [prev/next/stop]\n";
            string option;
            cin>>option;
            if(option.compare("prev")==0){
              cout<<"Lei prev\n";
              if(i<2)
                i=-1;
              else
                i=i-2;

              cout<<"Valor de i:"<<i<<endl;
            }else if(option.compare("stop")==0){
              //Elimino las canciones de la lista
              for (int i = 0; i < nLista; ++i){

                remove(songaddress[i].first.c_str());
              }
              break;
            }
            /*SimplePlayer player;
            auto t = player.play(path.c_str());
            t.join();
            */
          }
        }

        
      }else{
        void* server = zsocket_new(context,ZMQ_REQ);
        zmsg_t* select=zmsg_new();
        zmsg_addstr(select,selected.c_str());
        //zsocket_connect(server, "tcp://localhost:5558"); //Deber ir a una direccion  "tcp://localhost:4444"
        string serverAddress=sa[selected];
        if(serverAddress[6]=='*'){
          serverAddress.replace(6,1,"localhost");
        }
        cout<<"Direccion a enviar el mensaje "<<serverAddress<<endl;
        zsocket_connect(server,serverAddress.c_str());
        cout<<"Mensaje a el server\n";
        zmsg_print(select);
        zmsg_send(&select,server); //Envio de seleccion

        zfile_t *download = zfile_new("./received",selected.c_str());
        zfile_output(download);

        select=zmsg_recv(server);
        cout<<"Respuesta del server\n";
        zmsg_print(select);
        zframe_t *filePart =zmsg_pop(select);
        zchunk_t *chunk = zchunk_new(zframe_data(filePart), zframe_size(filePart)); 
        zfile_write(download, chunk, 0);
        zfile_close(download);
        cout << "Complete " << zfile_digest(download) << endl;
        string linea="mpg123 ./received/"+selected;
        
        string codePauta(zmsg_popstr(select));
        if(codePauta.compare("SI")==0){
            zfile_t *downloadPauta = zfile_new("./received","pauta.mp3");
            zfile_output(downloadPauta);
            zframe_t *filePartPauta =zmsg_pop(select);
            zchunk_t *chunkPauta = zchunk_new(zframe_data(filePartPauta), zframe_size(filePartPauta)); 
            zfile_write(downloadPauta, chunkPauta, 0);
            zfile_close(downloadPauta);
            cout << "Complete " << zfile_digest(downloadPauta) << endl;
            string lineaPauta="mpg123 ./received/pauta.mp3";
            system(lineaPauta.c_str());
            string rePauta="./received/pauta.mp3";
            remove(rePauta.c_str());

            system(linea.c_str());
            string re="./received/"+selected;
            remove(re.c_str());
            
        }else{
          system(linea.c_str());
          string re="./received/pauta.mp3";
          remove(re.c_str());
        }
    }
  }
      

    
  }else if(code.compare("crear")==0){
    zmsg_addstr(query,"crear");
    zmsg_addstr(query,argv[2]);
    zmsg_send(&query,broker);

    zmsg_t* canciones=zmsg_recv(broker);
    cout<<"Canciones disponibles: "<<endl;
    int n=atoi(zmsg_popstr(canciones));
    for (int i = 0; i < n; i++){
      string song(zmsg_popstr(canciones));
      cout<<song<<endl;
      string address(zmsg_popstr(canciones));
      
      sa[song]=address;
    }
    cout<<"Por favor seleccione las canciones de la lista, termine con 'end'"<<endl;
    zmsg_t* songsList=zmsg_new();
    int cont=0;
    while(true){

      string s;
      cin>>s;
      if(s.compare("end")==0){
        zmsg_pushstr(songsList,to_string(cont).c_str());
        zmsg_send(&songsList,broker);
        zmsg_t* confir=zmsg_recv(broker);
        cout<<zmsg_popstr(confir)<<endl;
        break;
      }else{
        zmsg_addstr(songsList,s.c_str());
        zmsg_addstr(songsList,sa[s].c_str());
        cont++;
      }
    }
  }
  
  zctx_destroy(&context);
  return 0;
}


