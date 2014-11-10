#include <czmq.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <map>
#include <queue>
#include <utility>

using namespace std;
unordered_map< string , vector < pair< string,string > > > listas; // nombreLista, cancion-direccion
unordered_map<zframe_t*,vector<string>> ssongs; //server-songs
unordered_map<string, queue< pair< zframe_t*, string > > > duplicados; // canciones, servidores y direcciones


int contConsultas=0;

void handleClientMessage(zmsg_t* msg,void* clients){
  cout<<"Entre al handleClientMessage\n";
  zmsg_t* outmsg = zmsg_new();

  zframe_t* idC=zmsg_pop(msg); // id del cliente
  string code(zmsg_popstr(msg));
  string name(zmsg_popstr(msg));
  int cont=0; //cantidad de canciones encontradas
  if(code.compare("query")==0){
      
    for ( auto it = duplicados.begin(); it != duplicados.end(); ++it ){
      string cancion=it->first;
      queue< pair< zframe_t*, string > > idAddres=it->second;

      if(cancion.find(name)!=string::npos){
        zmsg_addstr(outmsg,cancion.c_str());
        pair<zframe_t*, string > idAdd=idAddres.front(); // <idS, direccion>

        zmsg_addstr(outmsg,idAdd.second.c_str());
        cont++; //Contador saber el numero de canciones encontradas

        //BALANCEO DE CARGA ROUND-ROBIN
        duplicados[cancion].pop(); // elimino el frente, idAdd tiene el frente que elimine
        duplicados[cancion].push(idAdd); //Lo pongo en la cola
        //TERMINA EL BALANCEO ROUND-ROBIN

      }
    }

      zmsg_pushstr(outmsg,to_string(cont).c_str());
      zmsg_pushstr(outmsg,"respuesta"); //codigo de cancion
      int nlistas=listas.size();
      if(nlistas>0){
        zmsg_addstr(outmsg,"SI"); //NO hay listas por ahora
        zmsg_addstr(outmsg,to_string(nlistas).c_str());
        for ( auto it = listas.begin(); it != listas.end(); ++it ){
          string namelist=it->first;
          zmsg_addstr(outmsg,namelist.c_str());
        }
      }else{
        zmsg_addstr(outmsg,"NO"); //NO hay listas por ahora
      }

      zframe_t* dup=zframe_dup(idC);
      zmsg_prepend(outmsg,&dup); //Id del cliente
      

      cout<<"Mensaje que sale del handleClientMessage"<<endl;
      zmsg_print(outmsg);
      zmsg_send(&outmsg,clients);
      
}else if(code.compare("crear")==0){

    if(listas.count(name)>0){
      //LA LISTA YA ESTA, NO SE PUEDE CREAR
    }else{
      //LA LISTA NO ESTA, SE PUEDE CREAR
    }

    int cont=duplicados.size();
    for ( auto it = duplicados.begin(); it != duplicados.end(); ++it ){
      string cancion=it->first;
      queue< pair< zframe_t*, string > > idAddres=it->second;

        zmsg_addstr(outmsg,cancion.c_str());
        pair<zframe_t*, string > idAdd=idAddres.front(); // <idS, direccion>
        zmsg_addstr(outmsg,idAdd.second.c_str());

        cout<<"Cancion: "<<cancion.c_str()<<endl;
        cout<<"direccion: "<<idAdd.second.c_str()<<endl;
 

        //BALANCEO DE CARGA ROUND-ROBIN
        duplicados[cancion].pop(); // elimino el frente, idAdd tiene el frente que elimine
        duplicados[cancion].push(idAdd); //Lo pongo en la cola
        //TERMINA EL BALANCEO ROUND-ROBIN  
        
    }

    zframe_t* idcc=zframe_dup(idC);
    zmsg_pushstr(outmsg,to_string(cont).c_str()); // total canciones
    zmsg_prepend(outmsg,&idcc);
    cout<<"Mensaje para enviar al cliente\n",
    zmsg_print(outmsg);
    zmsg_send(&outmsg,clients);

    zmsg_t* canciones=zmsg_recv(clients);
    zframe_t* idCliente=zmsg_pop(canciones);
    int n=atoi(zmsg_popstr(canciones));
    for (int i = 0; i < n; i++){
       string s(zmsg_popstr(canciones));
       string d(zmsg_popstr(canciones));
       listas[name].push_back(make_pair(s,d));
    }
    zframe_t* dup=zframe_dup(idCliente);
    zmsg_t*confir=zmsg_new();
    zmsg_prepend(confir,&dup);
    zmsg_addstr(confir,"Lista creada");
    zmsg_send(&confir,clients);
}else if(code.compare("queryList")==0){

    // Lo que se eligio es una lista de reproduccion
    if(listas.count(name)>0){
      int ns=listas[name].size();
      zframe_t* copy=zframe_dup(idC);
      zmsg_addstr(outmsg,"lista");  //codigo de lista
      zmsg_addstr(outmsg,to_string(ns).c_str()); //numero de canciones de la lista
     
      vector < pair< string,string > > l=listas[name];
      int nl=l.size();
      for ( int i=0; i<nl;i++){ 
        zmsg_addstr(outmsg,l[i].first.c_str()); //nombre;
        zmsg_addstr(outmsg,l[i].second.c_str()); //direccion;
      }
      zmsg_prepend(outmsg,&copy); //Id del cliente
      cout<<"Mensaje respuesta de lista"<<endl;
      zmsg_print(outmsg);
      zmsg_send(&outmsg,clients);
    }else{
      zframe_t* copy=zframe_dup(idC);
      zmsg_addstr(outmsg,"NoList");  //codigo de lista
      zmsg_send(&outmsg,clients);
    }
}


}


void handleServerMessage(zmsg_t* msg,void* workers){
  zframe_t* idS=zmsg_pop(msg); // id del server
  string code(zmsg_popstr(msg)); // code registrar o actualizar

  

  if(code.compare("register")==0){
    cout<<"Entre al register"<<endl;

    int n=2*atoi(zmsg_popstr(msg));
    zframe_t* id=zframe_dup(idS);
    for (int i=0;i<n;i+=2){
      string cancion(zmsg_popstr(msg));
      string direc(zmsg_popstr(msg)); //direccion

      //Registro para hacer el balanceo de la cancion, el server y la direccion 
        duplicados[cancion].push(make_pair(id,direc));


      ssongs[id].push_back(cancion);
      ssongs[id].push_back(direc);

    }

    cout<<"Lo uque contiene songs\n";
    for ( auto it = ssongs.begin(); it != ssongs.end(); ++it ){
      zframe_t* idS= it->first;
      vector<string> songs=it->second;
      int tamVec=songs.size();
      for (int i = 0; i <tamVec;i++){
          cout<<songs[i]<<endl;
      }
    }

       
  }

}

void handleRepServerMessage(void* repServer){
  zmsg_t* msgAsk=zmsg_new(); //pregunta
  zmsg_addstr(msgAsk,"query");
  cout<<"El mensaje que envio al repserver:\n";
  zmsg_print(msgAsk);
  zmsg_send(&msgAsk,repServer);
  cout<<"Si fue enviado\n";


  zmsg_t* msgAns=zmsg_recv(repServer); //respuesta
  cout<<"Desde repServer"<<endl;
  zmsg_print(msgAns);
  string codetr(zmsg_popstr(msgAns));
  if(codetr.compare("update")==0){
    string code(zmsg_popstr(msgAns));
    if(code.compare("SI")==0){
      string song(zmsg_popstr(msgAns));
      zframe_t* idS=zmsg_pop(msgAns);
      string direccion(zmsg_popstr(msgAns));

      //Actualizo las dos estructuras
      ssongs[idS].push_back(song); //Ingreso la nueva cancion
      ssongs[idS].push_back(direccion); //Ingreso la direccion de la nueva cancion

      duplicados[song].push(make_pair(idS,direccion));
      cerr << "OK, update completed: "<<song<<endl;

    }else if(code.compare("NO")==0){
      string msgError(zmsg_popstr(msgAns));
      cout<<msgError<<endl;
      cerr << "No se realizo la replica: "<<endl;
    }
  } 
}



int main(int argc, char** argv) {
  zctx_t* context = zctx_new();
  // Socket to talk to the clients
  void* clients = zsocket_new(context, ZMQ_ROUTER);
  int clientPort = zsocket_bind(clients, "tcp://*:5555");
  cout << "Listen to clients at: "<< "localhost:" << clientPort << endl;

  // Socket to talk to the servers
  void* servers = zsocket_new(context, ZMQ_ROUTER);
  int serverPort = zsocket_bind(servers, "tcp://*:5556");
  cout << "Listen to servers at: "<< "localhost:" << serverPort << endl;

  // Socket to talk to the repServer
  void* repServer = zsocket_new(context,ZMQ_REQ);
  //zsocket_connect(server, "tcp://localhost:5558"); //Deber ir a una direccion  "tcp://localhost:4444
  int c=zsocket_connect(repServer,"tcp://localhost:9998");
  cout << "Listen to repServer at: "<< "localhost:" << c << endl;

  zmsg_t* msgAsk=zmsg_new(); //pregunta
  zmsg_addstr(msgAsk,"prueba conectividad");
  zmsg_print(msgAsk);
  zmsg_send(&msgAsk,repServer);
  zmsg_t* msgAsn=zmsg_new(); //pregunta
  msgAsn=zmsg_recv(repServer);
  zmsg_print(msgAsn);





  zmq_pollitem_t items[] = {{clients, 0, ZMQ_POLLIN, 0},
                            {servers, 0, ZMQ_POLLIN, 0}};
  cout << "Listening!" << endl;

  while (true) {
    zmq_poll(items, 2, 10 * ZMQ_POLL_MSEC);
    if (items[0].revents & ZMQ_POLLIN) {
      cerr << "From clients\n";
      zmsg_t* msg = zmsg_recv(clients);
      zmsg_print(msg);
      zmsg_t* outmsg = zmsg_new();
      handleClientMessage(msg,clients);

      contConsultas=contConsultas+1; //Contador para hacer las replicas teniendo en cuenta el numero de consultas

      if(contConsultas==3){
        cerr << "Asking to repServer\n";
        handleRepServerMessage(repServer);
        contConsultas=0;
      }
    }
    if (items[1].revents & ZMQ_POLLIN) {
      cerr << "From servers\n";
      zmsg_t* msg = zmsg_recv(servers);
      cout<<"Mensaje desde el server\n";
      zmsg_print(msg);
      handleServerMessage(msg,servers);
    }      
  }

  zctx_destroy(&context);
  return 0;
}
