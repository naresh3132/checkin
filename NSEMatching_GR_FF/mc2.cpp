Thread TCP:

class client
{
  int status;
  char msgBuffer[3000];
  int msgBufSize;
};

typedef map<int,class client*> connectionMap;
typedef connectionMap::iterator connectionItr;

char msgBuffer[3000] ={0};
char msgBuffer[2048] ={0};

connectionMap connMap;
connectionItr connItr;

if (new connection received)
{
  class client* obj = new (class client); // or we can pre-declare some 50 client objects.
  obj.status = 1;
  obj.msgBufSize = 0;
  memset(obj.msgBuffer, 0, sizeof(obj.msgBuffer));
  connMap.insert(std::pair<int,class client*>(fd, obj));
}

 CONNINFO* pConnInfo = connItr->second;                 
 if (processingFD != fdsock && pConnInfo->status == DISCONNECTED
 {
    delete (pConnInfo);
    close(fdsock);
 } 
 do
 {
    numRead = recv(fdSocket , &(pConnInfo->msgBuffer+pConnInfo->msgBufSize), 2048, 0);
 }while(numRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
                                
 if(numRead == 0)
 {
   if (processingFD != fdsock)
   {
     connItr = connMap.find(fdsock);
     delete (connMap->second);
   }
   shutdown(fdSocket, SHUT_RDWR);
   close(fdSocket);
 
   std::cout<<"FD "<<fdSocket<<"|Disconnection recieved."<<std::endl;
   FD_CLR(fdSocket, &read_flags);
   FD_CLR(fdSocket, &write_flags);
   continue;
 }                                                                                                                                                                              }

if (numRead > 0)
{
  processData and add in queue for matching engine
}

Thread_ME

 while(1)
 {
        for(int j=0; j < 10 ; j++) 
        {
            if(Inqptr_TCPServerToMe->dequeue(RcvData))
            {
               processingFD = RcvData->MyFd;
               ProcessTranscodes(&RcvData,std::ref(Inqptr_MeToTCPServer)); 
               memset(&RcvData, 0, sizeof(RcvData));
            }
            else
            {
              processingFD = -1;
            }   
        }
 }




