#include <czmq.h>
#include <iostream>
#include <utility>
#include <vector>
#include <unordered_map>
#include <set>
#include <list>

using namespace std;

list< pair< zframe_t*,int > > ss; // Servers reproduciones de un server, para ordenar. Complejidad nlogn
unordered_map<zframe_t*,string> registroServers; // Server-direccion
unordered_map< zframe_t*,set<string > > songServer; // servidor: canciones
unordered_map<string,pair< int, zframe_t* > > contCanciones; //cancion y el numero de reproducciones, el ultimo server que envio sus datos
float nReproduciones=0;

//Para ordenar el servidor por el numero de reproducciones
bool compare (pair< zframe_t* ,int > first, pair< zframe_t* ,int > second){
  return first.second < second.second;
}


//Guardo las canciones en una tabla hash y actualizo el numero de reproduccion es total
void procesarReproducciones(zmsg_t* repSongs){

  zframe_t* idS=zmsg_pop(repSongs);
  int nS=atoi(zmsg_popstr(repSongs));  //Numero de reproduciones en un servidor
  int nC=atoi(zmsg_popstr(repSongs));  //Cantidad canciones reproducidas en servidor


    zframe_t* dup=zframe_dup(idS);
    //Guardo el numero de reproducciones de un servidor
    ss.push_back(make_pair(idS,nS));

    //contador de canciones y servidor
    for (int i = 0; i < nC; i++){
        string song(zmsg_popstr(repSongs));
        int n=atoi(zmsg_popstr(repSongs));
        //voy sumando reproducciones a una cancion


        //Cada que se ejecuta un consulta del broker, comienzo a llenar con todas
        //las respuestas de los servers
        if(contCanciones.count(song)>0){
          int temp=contCanciones[song].first;
          contCanciones[song].first=temp+n;
          contCanciones[song].second=idS;   
        }else{
          contCanciones[song].first=n;
          contCanciones[song].second=idS;
        }
          
        zframe_print(idS,"id server");
        cout<<"song: "<<song<<"Size songserver "<<songServer.size()<<endl;
    }
    cout<<endl;
}

void handleBrokerMessage(zmsg_t* msg,void* brokers,void* servers){
  zmsg_print(msg);
  
  string code(zmsg_popstr(msg));

  if(code.compare("query")==0){
    ss.clear(); //Borro para que el conteo de canciones de un server comience
    contCanciones.clear(); //Borro lo que tengo para que comience el conteo con los nuevos datos
    
    cout<<"Numero de server: "<<songServer.size()<<" "<<ss.size()<<endl;
    //for ( auto it = songServer.begin(); it != songServer.end(); it++ ){
    for ( auto it = registroServers.begin(); it != registroServers.end(); it++ ){
      zframe_t* id= zframe_dup(it->first);
      zmsg_t* q=zmsg_new();
      zmsg_prepend (q, &id);
      string codee=code;
      zmsg_addstr(q,codee.c_str());
      cout<<"Mensaje a enviar al servidor\n";
      zmsg_print(q);
      zmsg_send(&q,servers);
      zmsg_t* repSongs=zmsg_recv(servers); //Recibo listas de canciones
      cout<<"Respuesta del servidor\n";
      zmsg_print(repSongs);

      procesarReproducciones(repSongs);
    }



    //Calculo cual es la cancion mas reproducida
    float nMayor=-1.0;
    string sMayor;
    zframe_t* idSS;
    float totalreproducciones=0;
    //Ya tengo almacenadas las canciones, ahora calculo cual es la mas reproducida

    int cantidadCanciones=contCanciones.size();
    cout<<"Cantidad de canciones\n";
    for ( auto it = contCanciones.begin(); it != contCanciones.end(); it++ ){
      string ssong=it->first;
      pair< int, zframe_t* > nss=it->second;
      float nsong=(float)nss.first;
      zframe_t* sdup=zframe_dup(nss.second);
      totalreproducciones+=nsong;
      if(nsong>nMayor){
        sMayor=ssong;
        nMayor=nsong;
        idSS=sdup;
      }
    }

    cout<<"totalreproducciones:"<<totalreproducciones<<endl;
    nReproduciones=totalreproducciones;
    //Ahora tengo la cancion mas reproducida , debo crear el mensaje a ese id del
    //servidor diciendole que lo replique en otro servidor

    zmsg_t* replica=zmsg_new();
    cout<<"nMayor:"<<nMayor<<" nReproduciones:"<<nReproduciones<<endl;
    float porcentaje=nMayor/nReproduciones;
    cout<<"Porcentaje: "<<porcentaje<<endl;
    if(porcentaje>=0.6){

        zmsg_append(replica,&idSS); //El que tiene la cancion
        zmsg_addstr(replica,"replicar"); //Code para el server
        zmsg_addstr(replica,"SI");  //Si replique
        string songRepli=sMayor;
        zmsg_addstr(replica,songRepli.c_str()); //Cancion que se tiene que replicar
        zmsg_send(&replica,servers);


        //Ordeno la lista complejidad nlogn
        ss.sort(compare);
        
     
        //Calculo cual es el servidor que tiene menos reproducciones y no tiene la cancion
       // list<pair< zframe_t* ,int > >::iterator it;
        cout<<"Tamaño del ss: "<<ss.size()<<endl;
        cout<<"Cantidad de servidores, con los que estoy haciendo el calculo: "<<ss.size()<<endl;
        int f=0;
        zframe_t* serverMenos;

        for (auto it=ss.begin(); it!=ss.end(); ++it){
          //pair< zframe_t* ,int > jjj=it->first;
          zframe_t* serverr=it->first;
          int reproducciones=it->second;
          //zframe_print(serverr,"Id del server: ");
          cout<<"Reproducciones: "<<reproducciones<<endl;
          if(songServer[serverr].count(sMayor)==0){
            serverMenos=serverr;
            f=0;
            //zframe_print(serverMenos,"Id del server con menos reproducciones");
            break;
          }else{
            cout<<"La cancion ya esta en un servidor"<<endl;
          }
          f=1;
        }

        cout<<"VALOR DE F LA BANDERA, 0 REPLICO, 1 NO REPLICO: "<<f<<endl;

        //SI ALGUNO NO TIENE LA CANCION HAGO AL OPERACION
        if(f==0){

              //Recibo el archivo. Solo es necesari decirle a que servidor enviarlo
              zmsg_t* file=zmsg_recv(servers);

              zframe_t* nointeresa=zmsg_pop(file); //Saco el id del server que transfiere el archivo
              string confirmacion(zmsg_popstr(file)); 

              //Adiciono el id del server en el que voy a replicar y envio
              if(confirmacion.compare("OK")==0 ){

                zmsg_pushstrf(file,"transferencia");
                zframe_t* idServerReplica=zframe_dup(serverMenos);
                zmsg_prepend(file,&idServerReplica); //Agrego el identidicador del server a replicar
                zmsg_print(file);
                zmsg_send(&file,servers);

                zmsg_t* msgConfir=zmsg_recv(servers);
                zframe_t* idST=zmsg_pop(msgConfir);
                string c(zmsg_popstr(msgConfir));
                if(c.compare("OK")==0){
                  zmsg_t* m=zmsg_new();
                  zmsg_addstr(m,"update"); //Cancion a actualizar
                  zmsg_addstr(m,"SI"); //  Como el servidor respondio, ya sabemos que la replica se hizo
                  string cancionRepli=sMayor;
                  zmsg_addstr(m,cancionRepli.c_str()); //Cancion a actualizar
                  zframe_t* idstt=zframe_dup(idST);
                  zmsg_append(m,&idstt); //Id del servidor actualizado   

                  zframe_t* paraAddress=zframe_dup(idST);
                  string isAddress;

                  //Se hace de esta forma porque tuve problemas al acceder a registerServers[]
                  cout<<"Contenido de registroServers"<<endl;
                  for (auto it=registroServers.begin();it!=registroServers.end();++it){
                    if(zframe_eq(it->first,paraAddress))
                      isAddress=it->second;
                    zframe_print(it->first,"IdS");
                    cout<<it->second<<endl;
                  }
                  
                
                  cout<<"Direccion del servidor: "<<isAddress<<endl;
                  zmsg_addstr(m,isAddress.c_str()); //direccion servidor
                  zmsg_send(&m,brokers);

                  //Registro esa cancion en el servidor en el que se hizo la replica
                  zframe_t* ServerReplica=zframe_dup(idST);
                  //zframe_print(ServerReplica,"Estoy agregando otro servidor_____________________________________________");
                  songServer[ServerReplica].insert(sMayor);
                  cout<<"sMayor: "<<sMayor<<endl;
                }else{
                  zmsg_t* m=zmsg_new(); 
                  zmsg_addstr(m,"update"); //Cancion a actualizar
                  zmsg_addstr(m,"NO"); //  Como el servidor respondio, ya sabemos que la replica NO se hizo
                  zmsg_addstr(m,"Error al replicar la cancion en el server");
                  zmsg_send(&m,brokers);
                  cout<<"Error al replicar la cancion en el server\n";
                }
                //Debo recibir una confirmacion del server 
                //despues avisarle al broker de la actizacion y decirle cual es el nombre de la cancion, el id del server y la direccion
              }else if(confirmacion.compare("ERROR")){ //si ocurrio un error o todos los server tienen la cancion
                //Enviar mensaje al broker diciendo que no hubo actualizacion
                zmsg_t* m=zmsg_new();
                zmsg_addstr(m,"update"); //Cancion a actualizar
                zmsg_addstr(m,"NO"); //  Como el servidor respondio, ya sabemos que la replica NO se hizo
                zmsg_addstr(m,"Error en el server que tenia que transferir la cancion");
                zmsg_send(&m,brokers);
                cout<<"Error en el server que tenia que transferir la cancion\n";
                
              }
        }else{
          //SI TODOS LO TIENEN ESCRIBO QUE NON PARA QUE EL BROKER IDENTIFIQUE QUE PASO 
          zmsg_t* m=zmsg_new();
          zmsg_addstr(m,"update"); //Cancion a actualizar
          zmsg_addstr(m,"NO"); //  Como el servidor respondio, ya sabemos que la replica NO se hizo
          zmsg_addstr(m,"Todos los servers tienen la cancion, no se puede replicar");
          zmsg_send(&m,brokers);
          cout<<"Todos los servers tienen la cancion, no se puede replicar\n";
        }

        
    }else{
      //EL PORCENTAJE NO SE CUMPLE, POR LO TANTO NO HAY REPLICA
      zmsg_t* m=zmsg_new();
      zmsg_addstr(m,"update"); //Cancion a actualizar
      zmsg_addstr(m,"NO"); //  Como el servidor respondio, ya sabemos que la replica NO se hizo
      cout<<"Mensaje a el broker con no update"<<endl;
      zmsg_addstr(m,"NO hay una cancion que tenga el 0.6 porciento de reproducciones");
      zmsg_print(m);
      zmsg_send(&m,brokers);
      cout<<"NO hay una que tenga el 0.6 reproducciones\n";
    }
  }
}


//Registro del servidor en el servidor de replicas
void handleServerMessage(zmsg_t* msg,void* servers){
  zmsg_print(msg);
  zframe_t* idS=zmsg_pop(msg); // id del server
  string code(zmsg_popstr(msg)); // code registrar o actualizar

  if(code.compare("register")==0){

      string addresnointeresa(zmsg_popstr(msg)); //no lo estoy usando
      //numero de canciones a registrar
      registroServers[idS]=addresnointeresa;
      zframe_print(idS,"Ids registroServers");
      int n=atoi(zmsg_popstr(msg)); 

      //Registro las canciones, para poder saber si el servidor ya tiene la cancion o no.
      for (int i = 0; i < n; ++i){
        string song(zmsg_popstr(msg));
        songServer[idS].insert(song);

      }
      zframe_print(idS,"Ids  songServer");
      cout<<"Numero de servers"<<songServer.size()<<endl;

  }
}




int main(int argc, char** argv) {
  zctx_t* context = zctx_new();

  // Socket to talk to the server
  void* servers = zsocket_new(context, ZMQ_ROUTER);
  int serverPort = zsocket_bind(servers, "tcp://*:5557");
  cout << "Listen to servers at: "<< "localhost:" << serverPort << endl;

  //socker repserver en zmq
  void* brokers = zsocket_new(context,ZMQ_REP);
  int ccc=zsocket_bind(brokers,"tcp://*:9998");
  cout << "Listen to repservers at: "<< "localhost:" << ccc << endl;

  zmsg_t* msgAsn=zmsg_new(); //pregunta
  msgAsn=zmsg_recv(brokers);
  zmsg_print(msgAsn); 

  zmsg_t* msgAsk=zmsg_new(); //pregunta
  zmsg_addstr(msgAsk,"prueba conectividad confirmada mensaje desde el repservers");
  zmsg_print(msgAsk);
  zmsg_send(&msgAsk,brokers);

  zmq_pollitem_t items[] = {{servers, 0, ZMQ_POLLIN, 0},
                            {brokers, 0, ZMQ_POLLIN, 0}};
  cout << "Listening!" << endl;

  while (true) {
    zmq_poll(items, 2, 10 * ZMQ_POLL_MSEC);
    if (items[0].revents & ZMQ_POLLIN) {
      cerr << "From servers\n";
      zmsg_t* msg = zmsg_recv(servers);
      
       handleServerMessage(msg,servers);
      cout<<"Sali del handleServerMessage\n";
    }
    if (items[1].revents & ZMQ_POLLIN) {
      cerr << "From brokers\n";
      zmsg_t* msg = zmsg_recv(brokers);
      handleBrokerMessage(msg, brokers,servers); 
    }
  }
  zctx_destroy(&context);
  return 0;
}