/* 
 * File:   Thread_Logger.h
 * Author: sneha
 *
 * Created on July 21, 2016, 6:52 PM
 */

#include<fstream>
#include<memory.h>
#include<iostream>
#include"Thread_Logger.h"
#include "nsecm_constants.h"
#include "nsecm_exch_structs.h"
#include "nsefo_exch_structs.h"

void convertAndWriteOrdRespToFile(NSECM::MS_OE_RESPONSE_TR* orderResp)
{
    NSECM::MS_OE_RESPONSE orderRespNTF = {0};
    
    /*Header fields - S*/
    orderRespNTF.msg_hdr.TransactionCode = orderResp->TransactionCode;
    orderRespNTF.msg_hdr.LogTime = orderResp->LogTime;
    orderRespNTF.msg_hdr.TraderId = orderResp->TraderId;
    orderRespNTF.msg_hdr.ErrorCode = orderResp->ErrorCode;
    orderRespNTF.msg_hdr.Timestamp1 = orderResp->TimeStamp1;
    orderRespNTF.msg_hdr.TimeStamp2[7] = orderResp->TimeStamp2;
    orderRespNTF.msg_hdr.MessageLength = sizeof (NSECM::MS_OE_RESPONSE);;
    //orderRespNTF.msg_hdr.AlphaChar;
    //orderRespNTF.msg_hdr.Timestamp;
    /*Header fields - E*/
    
    orderRespNTF.ModifiedCancelledBy = orderResp->Modify;
    orderRespNTF.ReasonCode = orderResp->ReasonCode;
    memcpy (&orderRespNTF.sec_info, &orderResp->sec_info, sizeof(orderRespNTF.sec_info));
    orderRespNTF.OrderNumber = orderResp->OrderNumber;
    memcpy (&orderRespNTF.AccountNumber, &orderResp->AccountNumber, sizeof (orderRespNTF.AccountNumber));
    orderRespNTF.BookType = orderResp->BookType;
    orderRespNTF.BuySellIndicator = orderResp->BuySellIndicator;
    orderRespNTF.DisclosedVolume = orderResp->DisclosedVolume;
    orderRespNTF.DisclosedVolumeRemaining = orderResp->DisclosedVolumeRemain;
    orderRespNTF.TotalVolumeRemaining = orderResp->TotalVolumeRemain;
    orderRespNTF.Volume = orderResp->Volume;
    orderRespNTF.VolumeFilledToday = orderResp->VolumeFilledToday;
    orderRespNTF.Price = orderResp->Price;
    orderRespNTF.EntryDateTime = orderResp->EntryDateTime;
    orderRespNTF.LastModified = orderResp->LastModified;
    memcpy (&orderRespNTF.st_order_flags, &orderResp->OrderFlags, sizeof (orderRespNTF.st_order_flags));
    orderRespNTF.BranchId = orderResp->BranchId;
    orderRespNTF.TraderId = orderResp->TraderId;
    memcpy (&orderRespNTF.BrokerId, &orderResp->BrokerId, sizeof (orderRespNTF.BrokerId));
    orderRespNTF.Suspended = orderResp->Suspended;
    memcpy (&orderRespNTF.Settlor, &orderResp->Settlor, sizeof (orderRespNTF.Settlor));
    orderRespNTF.ProClientIndicator = orderResp->ProClient;
    orderRespNTF.SettlementPeriod = orderResp->SettlementPeriod;
    orderRespNTF.NnfField = orderResp->NnfField;
    orderRespNTF.TransactionId = orderResp->TransactionId;
    /*orderRespNTF.CompetitorPeriod = __bswap_16(orderRespNTF.CompetitorPeriod);
    orderRespNTF.SolicitorPeriod = __bswap_16(orderRespNTF.SolicitorPeriod);
    orderRespNTF.AuctionNumber = __bswap_16(orderRespNTF.AuctionNumber);;
    orderRespNTF.TriggerPrice = __bswap_32(orderRespNTF.TriggerPrice);
    orderRespNTF.GoodTillDate = __bswap_32(orderRespNTF.GoodTillDate);
    orderRespNTF.MinimumFill = __bswap_32(orderRespNTF.MinimumFill);
    orderRespNTF.ExecTimeStamp = SwapDouble(orderRespNTF.ExecTimeStamp);
    orderRespNTF.ParticipantType;
    orderRespNTF.reserved1;
    orderRespNTF.reserved2;
    orderRespNTF.rfiller;
    orderRespNTF.CounterPartyBrokerId;
    orderRespNTF.OeRemarks;*/
    
  
    
    //std::ofstream file;
    FILE* file = NULL;
    /*string filename(orderResp->TraderId + ".txt");
    cout<<"Filename = "<<filename;*/
    int32_t TraderId = __bswap_32(orderResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    //std::cout<<"Filename = "<<filename<<std::endl;
    //file.open (filename, std::ios_base::out|std::ios_base::app);
    file = fopen(filename, "a");
    
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSECM::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &orderRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(orderRespNTF.ParticipantType), (sizeof(orderRespNTF) - sizeof(orderRespNTF.msg_hdr)));
              
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        //int dataLen = strlen(buf);
        buf[msgdnldSize] = '\0';
        //file.write(buf, sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA));
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
            std::cout<<"fwrite FAILED..!!Bytes written to file "<<bytesWritten<<std::endl;
            exit(1);
        }
        
        //file.flush();
        std::cout<<"Added MsgDnld data|"<<filename<<"|"<<__bswap_16(orderRespNTF.msg_hdr.TransactionCode)<<"|"<<msgDnldData.inner_hdr.Timestamp1<<"|"<<__bswap_64(msgDnldData.inner_hdr.Timestamp1)<<std::endl;
    }
    else
    {
      std::cout<<"Could not open file "<<filename<<std::endl;    
    }
    //file.close();
    if (file != NULL)
    {
      fclose(file);
    }
}

void convertAndWriteTrdRespToFile(NSECM::TRADE_CONFIRMATION_TR* tradeResp)
{
    NSECM::MS_TRADE_CONFIRM tradeRespNTF = {0};
    size_t sizeNTF = sizeof (NSECM::MS_TRADE_CONFIRM);
    
    /*Header fields - S*/
    tradeRespNTF.msg_hdr.TransactionCode = tradeResp->TransactionCode;
    tradeRespNTF.msg_hdr.LogTime = tradeResp->LogTime;
    tradeRespNTF.msg_hdr.TraderId = tradeResp->UserId;
    memcpy (&tradeRespNTF.msg_hdr.Timestamp, &tradeResp->Timestamp, sizeof (tradeRespNTF.msg_hdr.Timestamp));
    tradeRespNTF.msg_hdr.Timestamp1 = tradeResp->Timestamp1;
    tradeRespNTF.msg_hdr.TimeStamp2[7] = tradeResp->Timestamp2;
    tradeRespNTF.msg_hdr.ErrorCode = 0;
    tradeRespNTF.msg_hdr.MessageLength = sizeNTF;
    /*tradeRespNTF.msg_hdr.AlphaChar;*/
    /*Header fields - E*/
    
    tradeRespNTF.ResponseOrderNumber = tradeResp->ResponseOrderNumber;
    memcpy (&tradeRespNTF.BrokerId, &tradeResp->BrokerId, sizeof (tradeRespNTF.BrokerId));
    tradeRespNTF.TraderNumber = tradeResp->TraderNum;
    memcpy (&tradeRespNTF.AccountNumber, &tradeResp->AccountNumber, sizeof (tradeRespNTF.AccountNumber));
    tradeRespNTF.BuySellIndicator = tradeResp->BuySellIndicator;
    tradeRespNTF.OriginalVolume = tradeResp->OriginalVolume;
    tradeRespNTF.DisclosedVolume = tradeResp->DisclosedVolume;
    tradeRespNTF.RemainingVolume = tradeResp->RemainingVolume;
    tradeRespNTF.DisclosedVolumeRemaining = tradeResp->DisclosedVolumeRemaining;
    tradeRespNTF.Price = tradeResp->Price;
    memcpy (&tradeRespNTF.OrderFlags, &tradeResp->OrderFlags, sizeof (tradeRespNTF.OrderFlags));
    tradeRespNTF.FillNumber = tradeResp->FillNumber;
    tradeRespNTF.FillQuantity = tradeResp->FillQuantity;
    tradeRespNTF.FillPrice = tradeResp->FillPrice;
    tradeRespNTF.VolumeFilledToday = tradeResp->VolumeFilledToday;
    memcpy (&tradeRespNTF.ActivityType, &tradeResp->ActivityType, sizeof (tradeRespNTF.ActivityType));
    tradeRespNTF.ActivityTime = tradeResp->ActivityTime;
    memcpy (&tradeRespNTF.sec_info, &tradeResp->sec_info, sizeof (tradeRespNTF.sec_info));
    std::cout<<"Symbol|Series = "<<tradeRespNTF.sec_info.Symbol<<"|"<<tradeRespNTF.sec_info.Series<<std::endl;
    tradeRespNTF.BookType = tradeResp->BookType;
    tradeRespNTF.ProClient = tradeResp->ProClient;
    /*tradeRespNTF.CounterBrokerId;
    tradeRespNTF.CounterTraderOrderNumber;
    tradeRespNTF.reserved1;
    tradeRespNTF.Reserved2;
    tradeRespNTF.NewVolume;
    tradeRespNTF.GoodTillDate;*/
  
    //std::ofstream file;
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(tradeResp->UserId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    //file.open (filename, std::ios_base::out|std::ios_base::app);
    file = fopen(filename, "a");
    
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSECM::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &tradeRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(tradeRespNTF.ResponseOrderNumber), (sizeof(tradeRespNTF) - sizeof(tradeRespNTF.msg_hdr)));
    
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        /*buf[msgdnldSize] = '\0';
        file.write(buf, sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA));
        file.flush();*/
        
        //int dataLen = strlen(buf);
        buf[msgdnldSize] = '\0';
        //file.write(buf, sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA));
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
            std::cout<<"fwrite FAILED..!!Bytes written to file "<<bytesWritten<<std::endl;
            exit(1);
        }
        //file.flush();

        std::cout<<"Added MsgDnld data|"<<filename<<"|"<<__bswap_16(tradeRespNTF.msg_hdr.TransactionCode)<<"|"<<msgDnldData.inner_hdr.Timestamp1<<"|"<<__bswap_64(msgDnldData.inner_hdr.Timestamp1)<<std::endl;
    }
    else
    {
      std::cout<<"Could not open file "<<filename<<std::endl;
    }
    //file.close(); 
    if (file != NULL)
    {
      fclose(file);
    }
}

void convertAndWriteOrdRespToFile(NSEFO::MS_OE_RESPONSE_TR* orderResp)
{
    NSEFO::MS_OE_RESPONSE orderRespNTF = {0};
    
    /*Header fields - S*/
    orderRespNTF.msg_hdr.TransactionCode = orderResp->TransactionCode;
    orderRespNTF.msg_hdr.LogTime = orderResp->LogTime;
    orderRespNTF.msg_hdr.TraderId = orderResp->UserId;
    orderRespNTF.msg_hdr.ErrorCode = orderResp->ErrorCode;
    orderRespNTF.msg_hdr.TimeStamp1 = orderResp->Timestamp1;
    orderRespNTF.msg_hdr.TimeStamp2[7] = orderResp->Timestamp2;
    orderRespNTF.msg_hdr.MessageLength = sizeof (NSEFO::MS_OE_RESPONSE);
    //orderRespNTF.msg_hdr.AlphaChar;
    //orderRespNTF.msg_hdr.Timestamp;
    /*Header fields - E*/
    
    orderRespNTF.ModifiedCancelledBy = orderResp->ModCanBy;
    orderRespNTF.ReasonCode = orderResp->ReasonCode;
    orderRespNTF.TokenNo = orderResp->TokenNo;
    memcpy (&orderRespNTF.contract_desc, &orderResp->contract_desc_tr, sizeof(orderResp->contract_desc_tr));
    //memcpy (&orderRespNTF.sec_info, &orderResp->sec_info, sizeof(orderRespNTF.sec_info));
    orderRespNTF.CloseoutFlag = orderResp->CloseoutFlag;
    orderRespNTF.OrderNumber = orderResp->OrderNumber;
    memcpy (&orderRespNTF.AccountNumber, &orderResp->AccountNumber, sizeof (orderRespNTF.AccountNumber));
    orderRespNTF.BookType = orderResp->BookType;
    orderRespNTF.BuySellIndicator = orderResp->BuySellIndicator;
    orderRespNTF.DisclosedVolume = orderResp->DisclosedVolume;
    orderRespNTF.DisclosedVolumeRemaining = orderResp->DisclosedVolumeRemaining;
    orderRespNTF.TotalVolumeRemaining = orderResp->TotalVolumeRemaining;
    orderRespNTF.Volume = orderResp->Volume;
    orderRespNTF.VolumeFilledToday = orderResp->VolumeFilledToday;
    orderRespNTF.Price = orderResp->Price;
    orderRespNTF.GoodTillDate = orderResp->GoodTillDate;
    orderRespNTF.EntryDateTime = orderResp->EntryDateTime;
    orderRespNTF.LastModified = orderResp->LastModified;
    memcpy (&orderRespNTF.st_order_flags, &orderResp->OrderFlags, sizeof (orderRespNTF.st_order_flags));
    orderRespNTF.BranchId = orderResp->BranchId;
    orderRespNTF.TraderId = orderResp->TraderId;
    memcpy (&orderRespNTF.BrokerId, &orderResp->BrokerId, sizeof (orderRespNTF.BrokerId));
    orderRespNTF.OpenClose = orderResp->OpenClose;
    memcpy (&orderRespNTF.Settlor, &orderResp->Settlor, sizeof (orderRespNTF.Settlor));
    orderRespNTF.ProClientIndicator = orderResp->ProClientIndicator;
    memcpy(&orderRespNTF.additional_order_flags, &orderResp->AddtnlOrderFlags1, sizeof(orderRespNTF.additional_order_flags));
    orderRespNTF.filler = orderResp->filler;
    orderRespNTF.NnfField = orderResp->NnfField;
         
    /*orderRespNTF.CompetitorPeriod = __bswap_16(orderRespNTF.CompetitorPeriod);
    orderRespNTF.SolicitorPeriod = __bswap_16(orderRespNTF.SolicitorPeriod);
    orderRespNTF.OrderType = __bswap_16(orderRespNTF.OrderType);;
    orderRespNTF.TriggerPrice = __bswap_32(orderRespNTF.TriggerPrice);
    orderRespNTF.MinimumFill = __bswap_32(orderRespNTF.MinimumFill);
    orderRespNTF.SettlementPeriod = __bswap_16(orderRespNTF.SettlementPeriod);
    orderRespNTF.MktReplay = SwapDouble(orderRespNTF.MktReplay);
    orderRespNTF.ParticipantType;
    orderRespNTF.reserved1;
    orderRespNTF.reserved2;
    orderRespNTF.reserved3;
    orderRespNTF.CounterPartyBrokerId;
    orderRespNTF.reserved4;
    orderRespNTF.reserved5;
    orderRespNTF.reserved6;
    orderRespNTF.cOrdFiller;
    orderRespNTF.GiveUpFlag;*/
  
    
    //std::ofstream file;
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(orderResp->TraderId);
    //std::cout<<"Trader ID = "<< orderResp->TraderId<<std::endl;
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    //std::cout<<"Filename = "<<filename<<std::endl;
    //file.open (filename, std::ios_base::out|std::ios_base::app);
    file = fopen (filename, "a");
    
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSEFO::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &orderRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(orderRespNTF.ParticipantType), (sizeof(orderRespNTF) - sizeof(orderRespNTF.msg_hdr)));
              
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
       /*buf[msgdnldSize] = '\0';
        file.write(buf, sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA));
        file.flush();*/
        //int dataLen = strlen(buf);
        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
            std::cout<<"fwrite FAILED..!!Bytes written to file "<<bytesWritten<<std::endl;
            exit(1);
        }
        //file.flush();
        std::cout<<"Added MsgDnld data|"<<filename<<"|"<<__bswap_16(orderRespNTF.msg_hdr.TransactionCode)<<"|"<<msgDnldData.inner_hdr.TimeStamp1<<"|"<<__bswap_64(msgDnldData.inner_hdr.TimeStamp1)<<std::endl;
    }
    else
    {
      std::cout<<"Could not open file "<<filename<<std::endl;    
    }
    //file.close();
     if (file != NULL)
    {
      fclose(file);
    }
}


void convertAndWriteTrdRespToFile(NSEFO::TRADE_CONFIRMATION_TR* tradeResp)
{
    NSEFO::MS_TRADE_CONFIRM tradeRespNTF = {0};
        
    //std::cout<<"tradeResp.TS1="<<tradeResp->Timestamp1<<std::endl;
    
    /*Header fields - S*/
    tradeRespNTF.msg_hdr.TransactionCode = tradeResp->TransactionCode;
    tradeRespNTF.msg_hdr.LogTime = tradeResp->LogTime;
    tradeRespNTF.msg_hdr.TraderId = tradeResp->TraderId;
    sprintf(tradeRespNTF.msg_hdr.Timestamp,"%f",tradeResp->Timestamp);
    tradeRespNTF.msg_hdr.TimeStamp1 = tradeResp->Timestamp1;
    sprintf(tradeRespNTF.msg_hdr.TimeStamp2,"%f",tradeResp->Timestamp2);
    tradeRespNTF.msg_hdr.ErrorCode = 0;
    tradeRespNTF.msg_hdr.MessageLength = sizeof (NSEFO::MS_TRADE_CONFIRM);;
    /*tradeRespNTF.msg_hdr.AlphaChar;*/
    /*Header fields - E*/
    
    tradeRespNTF.ResponseOrderNumber = tradeResp->ResponseOrderNumber;
    memcpy (&tradeRespNTF.BrokerId, &tradeResp->BrokerId, sizeof (tradeRespNTF.BrokerId));
    tradeRespNTF.reserved1 = tradeResp->Reserved;
    memcpy (&tradeRespNTF.AccountNumber, &tradeResp->AccountNumber, sizeof (tradeRespNTF.AccountNumber));
    tradeRespNTF.BuySellIndicator = tradeResp->BuySellIndicator;
    tradeRespNTF.OriginalVolume = tradeResp->OriginalVolume;
    tradeRespNTF.DisclosedVolume = tradeResp->DisclosedVolume;
    tradeRespNTF.RemainingVolume = tradeResp->RemainingVolume;
    tradeRespNTF.DisclosedVolumeRemaining = tradeResp->DisclosedVolumeRemaining;
    tradeRespNTF.Price = tradeResp->Price;
    memcpy (&tradeRespNTF.OrderFlags, &tradeResp->OrderFlags, sizeof (tradeRespNTF.OrderFlags));
    tradeRespNTF.GoodTillDate = tradeResp->GoodTillDate;
    tradeRespNTF.FillNumber = tradeResp->FillNumber;
    tradeRespNTF.FillQuantity = tradeResp->FillQuantity;
    tradeRespNTF.FillPrice = tradeResp->FillPrice;
    tradeRespNTF.VolumeFilledToday = tradeResp->VolumeFilledToday;
    memcpy (&tradeRespNTF.ActivityType, &tradeResp->ActivityType, sizeof (tradeRespNTF.ActivityType));
    tradeRespNTF.ActivityTime = tradeResp->ActivityTime;
    memcpy (&tradeRespNTF.contract_desc, &tradeResp->contract_desc_tr, sizeof(tradeResp->contract_desc_tr));
    tradeRespNTF.OpenClose = tradeResp->OpenClose;
    tradeRespNTF.BookType = tradeResp->BookType;
    memcpy(&tradeRespNTF.Participant, &tradeResp->Participant, sizeof(tradeResp->Participant));
    memcpy(&tradeRespNTF.AddtnlOrderFlags,&tradeResp->AddtnlOrderFlags1,sizeof(tradeRespNTF.AddtnlOrderFlags));
    
    /*tradeRespNTF.TraderNumber = __bswap_32(tradeRespNTF.TraderNumber);
    SwapDouble((char*)&tradeRespNTF.CounterTraderOrderNumber);
    tradeRespNTF.NewVolume = __bswap_32(tradeRespNTF.NewVolume);
    tradeRespNTF.CounterBrokerId;
    tradeRespNTF.OldOpenClose; 
    tradeRespNTF.OldAccountNumber;
    tradeRespNTF.OldParticipant;
    tradeRespNTF.ReservedFiller;
    tradeRespNTF.GiveUpTrade;*/
  
    //std::ofstream file;
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(tradeResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    //file.open (filename, std::ios_base::out|std::ios_base::app);
    file = fopen(filename, "a");
    
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSEFO::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &tradeRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(tradeRespNTF.ResponseOrderNumber), (sizeof(tradeRespNTF) - sizeof(tradeRespNTF.msg_hdr)));
    
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        /*buf[msgdnldSize] = '\0';
        file.write(buf, sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA));
        file.flush();*/
        
        //int dataLen = strlen(buf);
        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
            std::cout<<"fwrite FAILED..!!Bytes written to file "<<bytesWritten<<std::endl;
            exit(1);
        }
        //file.flush();

        std::cout<<"Added MsgDnld data|"<<filename<<"|"<<__bswap_16(tradeRespNTF.msg_hdr.TransactionCode)<<"|"<<msgDnldData.inner_hdr.TimeStamp1<<"|"<<__bswap_64(msgDnldData.inner_hdr.TimeStamp1)<<std::endl;
    }
    else
    {
      std::cout<<"Could not open file "<<filename<<std::endl;
    }
    //file.close(); 
     if (file != NULL)
    {
      fclose(file);
    }
}

void StartLog(ProducerConsumerQueue<DATA_RECEIVED>* qptr, int _segMode)
{
    DATA_RECEIVED dataLog;
    
    while(1)
    {
        memset (&dataLog, 0, sizeof(dataLog));
        if(qptr->dequeue(dataLog))
        {
            switch (_segMode)
            {
                case SEG_NSECM:
                {
                    if (dataLog.MyFd == 1)
                    {
                      NSECM::MS_OE_RESPONSE_TR* orderRespCM = (NSECM::MS_OE_RESPONSE_TR*)dataLog.msgBuffer;
                      convertAndWriteOrdRespToFile (orderRespCM);
                    }
                    else if (dataLog.MyFd == 2)
                    {
                      NSECM::TRADE_CONFIRMATION_TR* tradeRespCM = (NSECM::TRADE_CONFIRMATION_TR*)dataLog.msgBuffer;  
                      convertAndWriteTrdRespToFile (tradeRespCM);  
                    }
                }
                break;
                case SEG_NSEFO:
                {
                    if (dataLog.MyFd == 1)
                    {
                      NSEFO::MS_OE_RESPONSE_TR* orderRespFO = (NSEFO::MS_OE_RESPONSE_TR*)dataLog.msgBuffer;
                      convertAndWriteOrdRespToFile (orderRespFO);
                    }
                    else if (dataLog.MyFd == 2)
                    {
                      NSEFO::TRADE_CONFIRMATION_TR* tradeRespFO = (NSEFO::TRADE_CONFIRMATION_TR*)dataLog.msgBuffer;  
                      convertAndWriteTrdRespToFile (tradeRespFO);  
                    }
                }
                break;
            }
        }
    }
}
