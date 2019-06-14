#ifndef _CALLBACK_MESSAGES_H_
#define _CALLBACK_MESSAGES_H_

typedef int64_t						CommsKeyID;
typedef short int					CommsMsgCode;
typedef unsigned int			CommsTimeStamp; 
typedef short int					CommsPort;	
typedef int               CommsCounter;


#define CLIENT_ADDR_LEN   32

typedef struct tagCommsMsgHeader
{
  CommsMsgCode        nMessageCode;
	CommsTimeStamp			nTimeStamp;
	CommsCounter        nMessageLength;

	tagCommsMsgHeader()
	{
		nMessageCode		= 0;
		nTimeStamp			= 0;
		nMessageLength	= 0;
	}

	tagCommsMsgHeader(const tagCommsMsgHeader& stCommsMsgHeader)
	{
		Copy(stCommsMsgHeader);
	}

	void operator = (const tagCommsMsgHeader& stCommsMsgHeader)
	{
		if(this == &stCommsMsgHeader)
		{
			return;
		}
		else
		{
			Copy(stCommsMsgHeader);
		}
	}

	private:
		void Copy(const tagCommsMsgHeader& stCommsMsgHeader)
		{
			nMessageCode		= stCommsMsgHeader.nMessageCode;
			nTimeStamp			= stCommsMsgHeader.nTimeStamp;
			nMessageLength		= stCommsMsgHeader.nMessageLength;
		}
} tagCommsMsgHeader;

typedef struct MessageConnectEventNotification
{
	tagCommsMsgHeader					stCommsMessageHeader;
	CommsKeyID                nKeyId;
        char                      cClientAddress[CLIENT_ADDR_LEN + 1];
	CommsPort                 nPortNumber;

	MessageConnectEventNotification()
	{
		nKeyId	= 0;
	}

	MessageConnectEventNotification(const MessageConnectEventNotification& msgConnectEventNotification)
	{
		Copy(msgConnectEventNotification);
	}

	void operator = (const MessageConnectEventNotification& msgConnectEventNotification)
	{
    Copy(msgConnectEventNotification);
	}

	private:
		void Copy(const MessageConnectEventNotification& msgConnectEventNotification)
		{
      if(this == &msgConnectEventNotification)
        return;      
      
			stCommsMessageHeader			= msgConnectEventNotification.stCommsMessageHeader;
			nKeyId                    = msgConnectEventNotification.nKeyId;
      nPortNumber               = msgConnectEventNotification.nPortNumber;
      strncpy(cClientAddress, msgConnectEventNotification.cClientAddress, CLIENT_ADDR_LEN);
		}
} MessageConnectEventNotification;

typedef struct MessageDisconnectEventNotification
{
  tagCommsMsgHeader             stCommsMsgHeader;
	CommsKeyID                    nKeyId;
	int                           nEventInitiatorReference;
  
  MessageDisconnectEventNotification()
  {
    nKeyId                    = 0;
    nEventInitiatorReference  = 0;
  }
  
  MessageDisconnectEventNotification(const MessageDisconnectEventNotification& msgDisconnectEventNotification)
  {
    Copy(msgDisconnectEventNotification);  
  }

  void operator = (const MessageDisconnectEventNotification& msgDisconnectEventNotification)
	{
    Copy(msgDisconnectEventNotification);
	}

	private:
		void Copy(const MessageDisconnectEventNotification& msgDisconnectEventNotification)
		{
      if(this == &msgDisconnectEventNotification)
        return;      

			stCommsMsgHeader          = msgDisconnectEventNotification.stCommsMsgHeader;
			nKeyId                    = msgDisconnectEventNotification.nKeyId;
      nEventInitiatorReference  = msgDisconnectEventNotification.nEventInitiatorReference;
		}
}MessageDisconnectEventNotification;

#endif
