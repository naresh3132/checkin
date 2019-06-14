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


for(int fdSocket=0; fdSocket< maxfd+1 ; fdSocket++)
{
   if(FD_ISSET(fdSocket, &tempset))
   {
      connItr =  connMap.find (fdSocket);
      if (connItr == connMap.end())
      { 
        //print eror;
        continue;
      }  
      class client* pClient = connItr->second;
      do
      {
         numRead = recv(fdSocket , (pClient.msgBuffer + pClient->msgBufSize), 2048, 0);
      }while(numRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
        
      if (numRead == 0) 
      {
           Close connection/socket;
      }
      
      if (numRead > 0)
      {
        pClient->msgBufSize = pClient->msgBufSize + numRead;
       
        while(exit==0 )
        {
           Header* hdr = (Header*)&(pClient->msgBuffer);
           if(pClient->msgBufSize < hdr.length || hdr.length == 0)
           {
              exit = 1;
           }    
           else
           {
              Data* dt = (Data*)&(msgBuffer);
              //processData;
              pClient->msgBufSize = pClient->msgBufSize - hdr.length;
              memcpy(pClient->msgBuffer,(pClient->msgBugger+hdr.length),(sizeof(pClient->msgBuffer) - hdr.length));      
           }        
        }
      
      }
   }
}

