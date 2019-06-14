/* 
 * File:   Thread_Logger.h
 * Author: sneha
 *
 * Created on July 21, 2016, 6:52 PM
 */

#include<fstream>
#include<memory.h>
#include<iostream>
#include"Thread_MsgDnld.h"
#include "nsecm_constants.h"
#include "nsecm_exch_structs.h"
#include "nsefo_exch_structs.h"
#include "nsecd_exch_structs.h"
#include "CommonFunctions.h"


char logbufMsgdnld[300];
void convertAndWriteOrdRespToFile(NSECM::MS_OE_RESPONSE_TR* orderResp, FileStore&  DnldFileStore)
{
    NSECM::MS_OE_RESPONSE orderRespNTF = {0};
    
    /*Header fields - S*/
    orderRespNTF.msg_hdr.TransactionCode = orderResp->TransactionCode;
    orderRespNTF.msg_hdr.LogTime = orderResp->LogTime;
    orderRespNTF.msg_hdr.TraderId = orderResp->TraderId;
    orderRespNTF.msg_hdr.ErrorCode = orderResp->ErrorCode;
    orderRespNTF.msg_hdr.Timestamp1 = orderResp->TimeStamp1;
    orderRespNTF.msg_hdr.Timestamp = orderResp->Timestamp;
    orderRespNTF.msg_hdr.TimeStamp2[7] = orderResp->TimeStamp2;
    orderRespNTF.msg_hdr.MessageLength = sizeof (NSECM::MS_OE_RESPONSE);;
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
    /*Pan card changes*/
    orderRespNTF.AlgoCategory = orderResp->AlgoCategory;
    orderRespNTF.AlgoId = orderResp->AlgoId;
    memcpy (&orderRespNTF.PAN, &orderResp->PAN, sizeof (orderRespNTF.PAN));
    /*Pan card changes end*/
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
    
    //std::ofstream file;
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(orderResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
         
    FileItr itr = DnldFileStore.find(std::string(filename));
    if (itr != DnldFileStore.end())
    {
        file = itr->second;
    }
    else
    {
       file = fopen(filename, "a");
       DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
       snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }

    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSECM::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &orderRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(orderRespNTF.ParticipantType), (sizeof(orderRespNTF) - sizeof(orderRespNTF.msg_hdr)));
              
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
          snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file %d",bytesWritten);
          Logger::getLogger().log(DEBUG, logbufMsgdnld);
          exit(1);
      }
        
        fflush(file);
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld", filename,__bswap_16(orderRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.Timestamp1,__bswap_64(msgDnldData.inner_hdr.Timestamp1));
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file %s Error|%s:%d",filename,strerror(errno),errno);
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    
}

void convertAndWriteOrdRespNonTrimToFile(NSECM::MS_OE_RESPONSE_SL* orderResp, FileStore&  DnldFileStore)
{
  NSECM::MS_OE_RESPONSE orderRespNTF = {0};
    
    /*Header fields - S*/
    orderRespNTF.msg_hdr.TransactionCode = orderResp->msg_hdr.TransactionCode;
    orderRespNTF.msg_hdr.LogTime = orderResp->msg_hdr.LogTime;
    orderRespNTF.msg_hdr.TraderId = orderResp->msg_hdr.TraderId;
    orderRespNTF.msg_hdr.ErrorCode = orderResp->msg_hdr.ErrorCode;
    orderRespNTF.msg_hdr.Timestamp1 = orderResp->msg_hdr.Timestamp1;
    orderRespNTF.msg_hdr.Timestamp = orderResp->msg_hdr.Timestamp;
    orderRespNTF.msg_hdr.TimeStamp2[7] = orderResp->msg_hdr.TimeStamp2[7];
    orderRespNTF.msg_hdr.MessageLength = sizeof (NSECM::MS_OE_RESPONSE);
    /*Header fields - E*/
    
    orderRespNTF.ModifiedCancelledBy = orderResp->ModifiedCancelledBy;
    orderRespNTF.ReasonCode = orderResp->ReasonCode;
    memcpy (&orderRespNTF.sec_info, &orderResp->sec_info, sizeof(orderRespNTF.sec_info));
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
    /*Pan Card changes*/
    memcpy (&orderRespNTF.PAN, &orderResp->PAN, sizeof (orderRespNTF.PAN));
    orderRespNTF.AlgoId = orderResp->AlgoId;
    orderRespNTF.AlgoCategory = orderResp->AlgoCategory;
    /*Pan Card changes ends*/
    orderRespNTF.EntryDateTime = orderResp->EntryDateTime;
    orderRespNTF.LastModified = orderResp->LastModified;
    memcpy (&orderRespNTF.st_order_flags, &orderResp->st_order_flags, sizeof (orderRespNTF.st_order_flags));
    orderRespNTF.BranchId = orderResp->BranchId;
    orderRespNTF.TraderId = orderResp->TraderId;
    memcpy (&orderRespNTF.BrokerId, &orderResp->BrokerId, sizeof (orderRespNTF.BrokerId));
    orderRespNTF.Suspended = orderResp->Suspended;
    memcpy (&orderRespNTF.Settlor, &orderResp->Settlor, sizeof (orderRespNTF.Settlor));
    orderRespNTF.ProClientIndicator = orderResp->ProClientIndicator;
    orderRespNTF.SettlementPeriod = orderResp->SettlementPeriod;
    orderRespNTF.NnfField = orderResp->NnfField;
    orderRespNTF.TransactionId = orderResp->TransactionId;
    
    std::cout<<"CM msg dnld::"<<orderRespNTF.PAN<<"|"<<__bswap_32(orderRespNTF.AlgoId)<<"|"<<__bswap_16(orderRespNTF.AlgoCategory)<<std::endl;
  
    
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(orderResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
         
    FileItr itr = DnldFileStore.find(std::string(filename));
    if (itr != DnldFileStore.end())
    {
        file = itr->second;
    }
    else
    {
       file = fopen(filename, "a");
       DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
       snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }

    if (file != NULL)
    {
      size_t msgdnldSize = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA);
      char buf[1000];
      memset(buf, 0, sizeof(buf));
      NSECM::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};

      memcpy (&msgDnldData.inner_hdr, &orderRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
      memcpy (&msgDnldData.Data, (void*)&(orderRespNTF.ParticipantType), (sizeof(orderRespNTF) - sizeof(orderRespNTF.msg_hdr)));

      memcpy (buf, (void*)&msgDnldData, msgdnldSize);
      buf[msgdnldSize] = '\0';
      int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
      if (bytesWritten != msgdnldSize)
      {
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file %d",bytesWritten);
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
        exit(1);
      }

      fflush(file);
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld", filename,__bswap_16(orderRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.Timestamp1,__bswap_64(msgDnldData.inner_hdr.Timestamp1));
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file %s Error|%s:%d",filename,strerror(errno),errno);
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
}
void convertAndWriteSLRespToFile (NSECM::MS_SL_TRIGGER* SLTrigger,FileStore&  DnldFileStore)
{
    NSECM::MS_SL_TRIGGER SLTrigRespNTF = {0};
    size_t sizeNTF = sizeof (NSECM::MS_SL_TRIGGER);
    
    /*Header fields - S*/
    SLTrigRespNTF.msg_hdr.TransactionCode = SLTrigger->msg_hdr.TransactionCode;
    SLTrigRespNTF.msg_hdr.LogTime = SLTrigger->msg_hdr.LogTime;
    SLTrigRespNTF.msg_hdr.TraderId = SLTrigger->msg_hdr.TraderId;
    memcpy (&SLTrigRespNTF.msg_hdr.Timestamp, &SLTrigger->msg_hdr.Timestamp, sizeof (SLTrigRespNTF.msg_hdr.Timestamp));
    SLTrigRespNTF.msg_hdr.Timestamp1 = SLTrigger->msg_hdr.Timestamp1;
    SLTrigRespNTF.msg_hdr.Timestamp = SLTrigger->msg_hdr.Timestamp;
    SLTrigRespNTF.msg_hdr.TimeStamp2[7] = SLTrigger->msg_hdr.TimeStamp2[7];
    SLTrigRespNTF.msg_hdr.ErrorCode = 0;
    SLTrigRespNTF.msg_hdr.MessageLength = sizeNTF;
    /*Header fields - E*/
    
    SLTrigRespNTF.ResponseOrderNumber = SLTrigger->ResponseOrderNumber;
    memcpy (&SLTrigRespNTF.BrokerId, &SLTrigger->BrokerId, sizeof (SLTrigRespNTF.BrokerId));
    SLTrigRespNTF.TraderNumber = SLTrigger->TraderNumber;
    memcpy (&SLTrigRespNTF.AccountNumber, &SLTrigger->AccountNumber, sizeof (SLTrigRespNTF.AccountNumber));
    SLTrigRespNTF.BuySellIndicator = SLTrigger->BuySellIndicator;
    SLTrigRespNTF.OriginalVolume = SLTrigger->OriginalVolume;
    SLTrigRespNTF.DisclosedVolume = SLTrigger->DisclosedVolume;
    SLTrigRespNTF.RemainingVolume = SLTrigger->RemainingVolume;
    SLTrigRespNTF.DisclosedVolumeRemaining = SLTrigger->DisclosedVolumeRemaining;
    SLTrigRespNTF.Price = SLTrigger->Price;
    
    /*Pan card changes*/
    SLTrigRespNTF.AlgoId = SLTrigger->AlgoId;
    SLTrigRespNTF.AlgoCategory= SLTrigger->AlgoCategory;
    memcpy (&SLTrigRespNTF.PAN, &SLTrigger->PAN, sizeof (SLTrigRespNTF.PAN));    
    /*Pan card changes end*/
    
    memcpy (&SLTrigRespNTF.OrderFlags, &SLTrigger->OrderFlags, sizeof (SLTrigRespNTF.OrderFlags));
    SLTrigRespNTF.FillNumber = SLTrigger->FillNumber;
    SLTrigRespNTF.FillQuantity = SLTrigger->FillQuantity;
    SLTrigRespNTF.FillPrice = SLTrigger->FillPrice;
    SLTrigRespNTF.VolumeFilledToday = SLTrigger->VolumeFilledToday;
    memcpy (&SLTrigRespNTF.ActivityType, &SLTrigger->ActivityType, sizeof (SLTrigRespNTF.ActivityType));
    SLTrigRespNTF.ActivityTime = SLTrigger->ActivityTime;
    memcpy (&SLTrigRespNTF.sec_info, &SLTrigger->sec_info, sizeof (SLTrigRespNTF.sec_info));
    SLTrigRespNTF.BookType = SLTrigger->BookType;
    SLTrigRespNTF.ProClient = SLTrigger->ProClient;
   
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(SLTrigger->msg_hdr.TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    FileItr itr = DnldFileStore.find(std::string(filename));
    if (itr != DnldFileStore.end())
    {
      file = itr->second;
    }
    else
    {
      file = fopen(filename, "a");
      DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore:%s",filename);
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    
    
    if (file != NULL)
    {
      size_t msgdnldSize = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA);
      char buf[1000];
      memset(buf, 0, sizeof(buf));
      NSECM::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};

      memcpy (&msgDnldData.inner_hdr, &SLTrigRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
      memcpy (&msgDnldData.Data, (void*)&(SLTrigRespNTF.ResponseOrderNumber), (sizeof(SLTrigRespNTF) - sizeof(SLTrigRespNTF.msg_hdr)-sizeof(SLTrigRespNTF.tap_hdr)));

      memcpy (buf, (void*)&msgDnldData, msgdnldSize);
      buf[msgdnldSize] = '\0';
      int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
      if (bytesWritten != msgdnldSize)
      {
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file %d",bytesWritten);
         Logger::getLogger().log(DEBUG, logbufMsgdnld);
        exit(1);
      }
      fflush(file);
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(SLTrigRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.Timestamp1,__bswap_64(msgDnldData.inner_hdr.Timestamp1));
      Logger::getLogger().log(DEBUG, logbufMsgdnld);

    }
    else
    {
       snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file %s",filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
}


void convertAndWriteTrdRespToFile(NSECM::TRADE_CONFIRMATION_TR* tradeResp, FileStore&  DnldFileStore)
{
    NSECM::MS_TRADE_CONFIRM tradeRespNTF = {0};
    size_t sizeNTF = sizeof (NSECM::MS_TRADE_CONFIRM);
    
    /*Header fields - S*/
    tradeRespNTF.msg_hdr.TransactionCode = tradeResp->TransactionCode;
    tradeRespNTF.msg_hdr.LogTime = tradeResp->LogTime;
    tradeRespNTF.msg_hdr.TraderId = tradeResp->UserId;
    memcpy (&tradeRespNTF.msg_hdr.Timestamp, &tradeResp->Timestamp, sizeof (tradeRespNTF.msg_hdr.Timestamp));
    tradeRespNTF.msg_hdr.Timestamp1 = tradeResp->Timestamp1;
    tradeRespNTF.msg_hdr.Timestamp = tradeResp->Timestamp;
    tradeRespNTF.msg_hdr.TimeStamp2[7] = 1;
    tradeRespNTF.msg_hdr.ErrorCode = 0;
    tradeRespNTF.msg_hdr.MessageLength = sizeNTF;
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
    /*Pan card changes*/
    tradeRespNTF.AlgoId = tradeResp->AlgoId;
    tradeRespNTF.AlgoCategory= tradeResp->AlgoCategory;
    memcpy (&tradeRespNTF.PAN, &tradeResp->PAN, sizeof (tradeRespNTF.PAN));    
    /*Pan card changes end*/
    memcpy (&tradeRespNTF.OrderFlags, &tradeResp->OrderFlags, sizeof (tradeRespNTF.OrderFlags));
    tradeRespNTF.FillNumber = tradeResp->FillNumber;
    tradeRespNTF.FillQuantity = tradeResp->FillQuantity;
    tradeRespNTF.FillPrice = tradeResp->FillPrice;
    tradeRespNTF.VolumeFilledToday = tradeResp->VolumeFilledToday;
    memcpy (&tradeRespNTF.ActivityType, &tradeResp->ActivityType, sizeof (tradeRespNTF.ActivityType));
    tradeRespNTF.ActivityTime = tradeResp->ActivityTime;
    memcpy (&tradeRespNTF.sec_info, &tradeResp->sec_info, sizeof (tradeRespNTF.sec_info));
    tradeRespNTF.BookType = tradeResp->BookType;
    tradeRespNTF.ProClient = tradeResp->ProClient;


    FILE* file = NULL;
    int32_t TraderId = __bswap_32(tradeResp->UserId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    FileItr itr = DnldFileStore.find(std::string(filename));
     if (itr != DnldFileStore.end())
     {
         file = itr->second;
     }
     else
     {
        file = fopen(filename, "a");
        DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore:%s",filename);
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSECM::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSECM::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &tradeRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(tradeRespNTF.ResponseOrderNumber), (sizeof(tradeRespNTF) - sizeof(tradeRespNTF.msg_hdr)));
    
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);

        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
          snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file %d",bytesWritten);
          Logger::getLogger().log(DEBUG, logbufMsgdnld);
            exit(1);
        }
        fflush(file);
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(tradeRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.Timestamp1,__bswap_64(msgDnldData.inner_hdr.Timestamp1));
         Logger::getLogger().log(DEBUG, logbufMsgdnld);
        //std::cout<<"Added MsgDnld data|"<<filename<<"|"<<__bswap_16(tradeRespNTF.msg_hdr.TransactionCode)<<"|"<<msgDnldData.inner_hdr.Timestamp1<<"|"<<__bswap_64(msgDnldData.inner_hdr.Timestamp1)<<std::endl;
    }
    else
    {
       snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file %s",filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
      //std::cout<<"Could not open file "<<filename<<std::endl;
    }
   
}

void convertAndWriteOrdRespToFile(NSEFO::MS_OE_RESPONSE_TR* orderResp, FileStore&  DnldFileStore)
{
    NSEFO::MS_OE_RESPONSE orderRespNTF = {0};
    
    /*Header fields - S*/
    orderRespNTF.msg_hdr.TransactionCode = orderResp->TransactionCode;
    orderRespNTF.msg_hdr.LogTime = orderResp->LogTime;
    orderRespNTF.msg_hdr.TraderId = orderResp->UserId;
    orderRespNTF.msg_hdr.ErrorCode = orderResp->ErrorCode;
    orderRespNTF.msg_hdr.TimeStamp1 = orderResp->Timestamp1;
    orderRespNTF.msg_hdr.Timestamp = orderResp->Timestamp;
    orderRespNTF.msg_hdr.TimeStamp2[7] = orderResp->Timestamp2;
    orderRespNTF.msg_hdr.MessageLength = sizeof (NSEFO::MS_OE_RESPONSE);
    /*Header fields - E*/
    
    orderRespNTF.ModifiedCancelledBy = orderResp->ModCanBy;
    orderRespNTF.ReasonCode = orderResp->ReasonCode;
    orderRespNTF.TokenNo = orderResp->TokenNo;
    memcpy (&orderRespNTF.contract_desc, &orderResp->contract_desc_tr, sizeof(orderResp->contract_desc_tr));
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
    
    /*Pan card changes*/
    orderRespNTF.AlgoId = orderResp->AlgoId;
    orderRespNTF.AlgoCategory= orderResp->AlgoCategory;
    memcpy (&orderRespNTF.PAN, &orderResp->PAN, sizeof (orderRespNTF.PAN));    
    /*Pan card changes end*/ 
    
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
         
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(orderResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
      
    FileItr itr = DnldFileStore.find(std::string(filename));
    if (itr != DnldFileStore.end())
    {
        file = itr->second;
    }
    else
    {
       file = fopen(filename, "a");
       DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
       snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }

    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSEFO::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &orderRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(orderRespNTF.ParticipantType), (sizeof(orderRespNTF) - sizeof(orderRespNTF.msg_hdr)));
              
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
          snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file  %d", bytesWritten);
          Logger::getLogger().log(DEBUG, logbufMsgdnld);
          exit(1);
        }
        fflush(file);
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(orderRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.TimeStamp1,__bswap_64(msgDnldData.inner_hdr.TimeStamp1));
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file: %s",filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
  
}

void convertAndWriteOrdRespToFile(NSECD::MS_OE_RESPONSE_TR* orderResp, FileStore&  DnldFileStore)
{
    NSECD::MS_OE_RESPONSE orderRespNTF = {0};
    
    /*Header fields - S*/
    orderRespNTF.msg_hdr.TransactionCode = orderResp->TransactionCode;
    orderRespNTF.msg_hdr.LogTime = orderResp->LogTime;
    orderRespNTF.msg_hdr.TraderId = orderResp->UserId;
    orderRespNTF.msg_hdr.ErrorCode = orderResp->ErrorCode;
    orderRespNTF.msg_hdr.TimeStamp1 = orderResp->Timestamp1;
    orderRespNTF.msg_hdr.TimeStamp2[7] = orderResp->Timestamp2;
    orderRespNTF.msg_hdr.MessageLength = sizeof (NSECD::MS_OE_RESPONSE);
    /*Header fields - E*/
    
    orderRespNTF.ModifiedCancelledBy = orderResp->ModCanBy;
    orderRespNTF.ReasonCode = orderResp->ReasonCode;
    orderRespNTF.TokenNo = orderResp->TokenNo;
    memcpy (&orderRespNTF.contract_desc, &orderResp->contract_desc_tr, sizeof(orderResp->contract_desc_tr));
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
    
    /*Pan card changes*/
    orderRespNTF.AlgoId = orderResp->AlgoId;
    orderRespNTF.AlgoCategory= orderResp->AlgoCategory;
    memcpy (&orderRespNTF.PAN, &orderResp->PAN, sizeof (orderRespNTF.PAN));    
    /*Pan card changes end*/ 
    
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
         
   
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(orderResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
      
     FileItr itr = DnldFileStore.find(std::string(filename));
     if (itr != DnldFileStore.end())
     {
         file = itr->second;
     }
     else
     {
        file = fopen(filename, "a");
        DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
    
    
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSECD::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSECD::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &orderRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(orderRespNTF.ParticipantType), (sizeof(orderRespNTF) - sizeof(orderRespNTF.msg_hdr)));
              
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
           snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file  %d", bytesWritten);
            Logger::getLogger().log(DEBUG, logbufMsgdnld);
            exit(1);
        }
        fflush(file);
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(orderRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.TimeStamp1,__bswap_64(msgDnldData.inner_hdr.TimeStamp1));
         Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file: %s",filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
  
}


void convertAndWriteOrdRespNonTrimToFile(NSEFO::MS_OE_RESPONSE_SL* orderResp, FileStore&  DnldFileStore)
{
    NSEFO::MS_OE_RESPONSE orderRespNTF = {0};
    
    /*Header fields - S*/
    orderRespNTF.msg_hdr.TransactionCode = orderResp->msg_hdr.TransactionCode;
    orderRespNTF.msg_hdr.LogTime = orderResp->msg_hdr.LogTime;
    orderRespNTF.msg_hdr.TraderId = orderResp->msg_hdr.TraderId;
    orderRespNTF.msg_hdr.ErrorCode = orderResp->msg_hdr.ErrorCode;
    orderRespNTF.msg_hdr.Timestamp = orderResp->msg_hdr.Timestamp;
    orderRespNTF.msg_hdr.TimeStamp1 = orderResp->msg_hdr.TimeStamp1;
    orderRespNTF.msg_hdr.TimeStamp2[7] = orderResp->msg_hdr.TimeStamp2[7];
    orderRespNTF.msg_hdr.MessageLength = sizeof (NSEFO::MS_OE_RESPONSE);
    /*Header fields - E*/
    
    orderRespNTF.ModifiedCancelledBy = orderResp->ModifiedCancelledBy;
    orderRespNTF.ReasonCode = orderResp->ReasonCode;
    orderRespNTF.TokenNo = orderResp->TokenNo;
    memcpy (&orderRespNTF.contract_desc, &orderResp->contract_desc, sizeof(orderResp->contract_desc));
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
    
    /*Pan card changes*/
    orderRespNTF.AlgoId = orderResp->AlgoId;
    orderRespNTF.AlgoCategory= orderResp->AlgoCategory;
    memcpy (&orderRespNTF.PAN, &orderResp->PAN, sizeof (orderRespNTF.PAN));    
    /*Pan card changes end*/ 
    
    orderRespNTF.GoodTillDate = orderResp->GoodTillDate;
    orderRespNTF.EntryDateTime = orderResp->EntryDateTime;
    orderRespNTF.LastModified = orderResp->LastModified;
    memcpy (&orderRespNTF.st_order_flags, &orderResp->st_order_flags, sizeof (orderRespNTF.st_order_flags));
    orderRespNTF.BranchId = orderResp->BranchId;
    orderRespNTF.TraderId = orderResp->TraderId;
    memcpy (&orderRespNTF.BrokerId, &orderResp->BrokerId, sizeof (orderRespNTF.BrokerId));
    orderRespNTF.OpenClose = orderResp->OpenClose;
    memcpy (&orderRespNTF.Settlor, &orderResp->Settlor, sizeof (orderRespNTF.Settlor));
    orderRespNTF.ProClientIndicator = orderResp->ProClientIndicator;
    memcpy(&orderRespNTF.additional_order_flags, &orderResp->additional_order_flags, sizeof(orderRespNTF.additional_order_flags));
    orderRespNTF.filler = orderResp->filler;
    orderRespNTF.NnfField = orderResp->NnfField;
   
  
    
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(orderResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
      
    FileItr itr = DnldFileStore.find(std::string(filename));
    if (itr != DnldFileStore.end())
    {
        file = itr->second;
    }
    else
    {
      file = fopen(filename, "a");
      DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    
    
    if (file != NULL)
    {
      size_t msgdnldSize = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA);
      char buf[1000];
      memset(buf, 0, sizeof(buf));
      NSEFO::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};

      memcpy (&msgDnldData.inner_hdr, &orderRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
      memcpy (&msgDnldData.Data, (void*)&(orderRespNTF.ParticipantType), (sizeof(orderRespNTF) - sizeof(orderRespNTF.msg_hdr)));

      memcpy (buf, (void*)&msgDnldData, msgdnldSize);
      buf[msgdnldSize] = '\0';
      int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
      if (bytesWritten != msgdnldSize)
      {
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file  %d", bytesWritten);
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
        exit(1);
      }
      fflush(file);
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(orderRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.TimeStamp1,__bswap_64(msgDnldData.inner_hdr.TimeStamp1));
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file: %s",filename);
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
  
}


void convertAndWriteSLRespToFile(NSEFO::MS_SL_TRIGGER* SLTrigger, FileStore&  DnldFileStore)
{
    NSEFO::MS_SL_TRIGGER SLTrigRespNTF = {0};
        
    
    /*Header fields - S*/
    SLTrigRespNTF.msg_hdr.TransactionCode = SLTrigger->msg_hdr.TransactionCode;
    SLTrigRespNTF.msg_hdr.LogTime = SLTrigger->msg_hdr.LogTime;
    SLTrigRespNTF.msg_hdr.TraderId = SLTrigger->msg_hdr.TraderId;
    SLTrigRespNTF.msg_hdr.Timestamp = SLTrigger->msg_hdr.Timestamp;
    SLTrigRespNTF.msg_hdr.TimeStamp1 = SLTrigger->msg_hdr.TimeStamp1;
    sprintf(SLTrigRespNTF.msg_hdr.TimeStamp2,"%f",SLTrigger->msg_hdr.TimeStamp2);
    SLTrigRespNTF.msg_hdr.ErrorCode = 0;
    SLTrigRespNTF.msg_hdr.MessageLength = sizeof (NSEFO::MS_SL_TRIGGER);
    /*Header fields - E*/
    
    SLTrigRespNTF.ResponseOrderNumber = SLTrigger->ResponseOrderNumber;
    memcpy (&SLTrigRespNTF.BrokerId, &SLTrigger->BrokerId, sizeof (SLTrigRespNTF.BrokerId));
    SLTrigRespNTF.reserved1 = SLTrigger->reserved1;
    memcpy (&SLTrigRespNTF.AccountNumber, &SLTrigger->AccountNumber, sizeof (SLTrigRespNTF.AccountNumber));
    SLTrigRespNTF.BuySellIndicator = SLTrigger->BuySellIndicator;
    SLTrigRespNTF.OriginalVolume = SLTrigger->OriginalVolume;
    SLTrigRespNTF.DisclosedVolume = SLTrigger->DisclosedVolume;
    SLTrigRespNTF.RemainingVolume = SLTrigger->RemainingVolume;
    SLTrigRespNTF.DisclosedVolumeRemaining = SLTrigger->DisclosedVolumeRemaining;
    SLTrigRespNTF.Price = SLTrigger->Price;
    
    /*Pan card changes*/
    SLTrigRespNTF.AlgoId = SLTrigger->AlgoId;
    SLTrigRespNTF.AlgoCategory= SLTrigger->AlgoCategory;
    memcpy (&SLTrigRespNTF.PAN, &SLTrigger->PAN, sizeof (SLTrigRespNTF.PAN));    
    /*Pan card changes end*/ 
    
    memcpy (&SLTrigRespNTF.OrderFlags, &SLTrigger->OrderFlags, sizeof (SLTrigRespNTF.OrderFlags));
    SLTrigRespNTF.GoodTillDate  = SLTrigger->GoodTillDate;
    SLTrigRespNTF.FillNumber = SLTrigger->FillNumber;
    SLTrigRespNTF.FillQuantity = SLTrigger->FillQuantity;
    SLTrigRespNTF.FillPrice = SLTrigger->FillPrice;
    SLTrigRespNTF.VolumeFilledToday = SLTrigger->VolumeFilledToday;
    memcpy (&SLTrigRespNTF.ActivityType, &SLTrigger->ActivityType, sizeof (SLTrigRespNTF.ActivityType));
    SLTrigRespNTF.ActivityTime = SLTrigger->ActivityTime;
    memcpy (&SLTrigRespNTF.contract_desc, &SLTrigger->contract_desc, sizeof(SLTrigger->contract_desc));
    SLTrigRespNTF.OpenClose = SLTrigger->OpenClose;
    SLTrigRespNTF.BookType = SLTrigger->BookType;
    memcpy(&SLTrigRespNTF.Participant, &SLTrigger->Participant, sizeof(SLTrigger->Participant));
    memcpy(&SLTrigRespNTF.AddtnlOrderFlags,&SLTrigger->AddtnlOrderFlags,sizeof(SLTrigRespNTF.AddtnlOrderFlags));
    SLTrigRespNTF.Token = SLTrigger->Token;
        
    
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(SLTrigger->msg_hdr.TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    
     FileItr itr = DnldFileStore.find(std::string(filename));
     if (itr != DnldFileStore.end())
     {
         file = itr->second;
     }
     else
     {
        file = fopen(filename, "a");
        DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
         Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
    
    
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSEFO::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &SLTrigRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(SLTrigRespNTF.ResponseOrderNumber), (sizeof(SLTrigRespNTF) - sizeof(SLTrigRespNTF.msg_hdr) - sizeof(SLTrigRespNTF.tap_hdr)));
    
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
          snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file %d",bytesWritten);
          Logger::getLogger().log(DEBUG, logbufMsgdnld); 
          exit(1);
        }
        fflush(file);
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(SLTrigRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.TimeStamp1,__bswap_64(msgDnldData.inner_hdr.TimeStamp1));
         Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file %s",filename);
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
 }

void convertAndWriteTrdRespToFile(NSEFO::TRADE_CONFIRMATION_TR* tradeResp, FileStore&  DnldFileStore)
{
    NSEFO::MS_TRADE_CONFIRM tradeRespNTF = {0};
        
    
    /*Header fields - S*/
    tradeRespNTF.msg_hdr.TransactionCode = tradeResp->TransactionCode;
    tradeRespNTF.msg_hdr.LogTime = tradeResp->LogTime;
    tradeRespNTF.msg_hdr.TraderId = tradeResp->TraderId;
    tradeRespNTF.msg_hdr.Timestamp = tradeResp->Timestamp;
    tradeRespNTF.msg_hdr.TimeStamp1 = tradeResp->Timestamp1;
    tradeRespNTF.msg_hdr.TimeStamp2[7]=1;
    tradeRespNTF.msg_hdr.ErrorCode = 0;
    tradeRespNTF.msg_hdr.MessageLength = sizeof (NSEFO::MS_TRADE_CONFIRM);
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
    
    /*Pan card changes*/
    tradeRespNTF.AlgoId = tradeResp->AlgoId;
    tradeRespNTF.AlgoCategory= tradeResp->AlgoCategory;
    memcpy (&tradeRespNTF.PAN, &tradeResp->PAN, sizeof (tradeRespNTF.PAN));    
    /*Pan card changes end*/
    
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
    tradeRespNTF.Token = tradeResp->Token;
        
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(tradeResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    
     FileItr itr = DnldFileStore.find(std::string(filename));
     if (itr != DnldFileStore.end())
     {
         file = itr->second;
     }
     else
     {
        file = fopen(filename, "a");
        DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
    
    
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSEFO::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &tradeRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(tradeRespNTF.ResponseOrderNumber), (sizeof(tradeRespNTF) - sizeof(tradeRespNTF.msg_hdr)));
    
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
          snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file %d",bytesWritten);
          Logger::getLogger().log(DEBUG, logbufMsgdnld); 
          exit(1);
        }
        fflush(file);
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(tradeRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.TimeStamp1,__bswap_64(msgDnldData.inner_hdr.TimeStamp1));
        Logger::getLogger().log(DEBUG, logbufMsgdnld); 
    }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file %s",filename);
      Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
 }

void convertAndWriteTrdRespToFile(NSECD::TRADE_CONFIRMATION_TR* tradeResp, FileStore&  DnldFileStore)
{
    NSECD::MS_TRADE_CONFIRM tradeRespNTF = {0};
        
    
    /*Header fields - S*/
    tradeRespNTF.msg_hdr.TransactionCode = tradeResp->TransactionCode;
    tradeRespNTF.msg_hdr.LogTime = tradeResp->LogTime;
    tradeRespNTF.msg_hdr.TraderId = tradeResp->TraderId;
    tradeRespNTF.msg_hdr.TimeStamp1 = tradeResp->Timestamp1;
    tradeRespNTF.msg_hdr.TimeStamp2[7]=1;
    tradeRespNTF.msg_hdr.ErrorCode = 0;
    tradeRespNTF.msg_hdr.MessageLength = sizeof (NSECD::MS_TRADE_CONFIRM);
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
    
    /*Pan card changes*/
    tradeRespNTF.AlgoId = tradeResp->AlgoId;
    tradeRespNTF.AlgoCategory= tradeResp->AlgoCategory;
    memcpy (&tradeRespNTF.PAN, &tradeResp->PAN, sizeof (tradeRespNTF.PAN));    
    /*Pan card changes end*/
    
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
    tradeRespNTF.Token = tradeResp->Token;
        
    
  
    FILE* file = NULL;
    int32_t TraderId = __bswap_32(tradeResp->TraderId);
    char filename[20] = {"\0"};
    sprintf(filename, "%d", TraderId);
    strcat(filename, "Dnld");
    
     FileItr itr = DnldFileStore.find(std::string(filename));
     if (itr != DnldFileStore.end())
     {
         file = itr->second;
     }
     else
     {
        file = fopen(filename, "a");
        DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
         Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
    
    
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSECD::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSECD::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        memcpy (&msgDnldData.inner_hdr, &tradeRespNTF.msg_hdr, sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, (void*)&(tradeRespNTF.ResponseOrderNumber), (sizeof(tradeRespNTF) - sizeof(tradeRespNTF.msg_hdr)));
    
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        buf[msgdnldSize] = '\0';
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
          snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file %d",bytesWritten);
           Logger::getLogger().log(DEBUG, logbufMsgdnld); 
            exit(1);
        }
        fflush(file);
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(tradeRespNTF.msg_hdr.TransactionCode),msgDnldData.inner_hdr.TimeStamp1,__bswap_64(msgDnldData.inner_hdr.TimeStamp1));
         Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file %s",filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
 }

void convertAndWriteMLRespToFile(NSECD::MS_SPD_OE_REQUEST* MLResp, FileStore&  DnldFileStore)
{
     NSECD::MS_SPD_OE_REQUEST MLOrder = {0};
     //memcpy (&MLOrder, MLResp, sizeof (NSEFO::MS_SPD_OE_REQUEST));
     
     FILE* file = NULL;
     int32_t TraderId = __bswap_32(MLResp->TraderId1);
     char filename[20] = {"\0"};
     sprintf(filename, "%d", TraderId);
     strcat(filename, "Dnld");
      
     FileItr itr = DnldFileStore.find(std::string(filename));
     if (itr != DnldFileStore.end())
     {
         file = itr->second;
     }
     else
     {
        file = fopen(filename, "a");
        DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
      
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSECD::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSECD::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        size_t dataSize = sizeof(NSECD::MS_SPD_OE_REQUEST) - (sizeof(NSECD::TAP_HEADER) + sizeof(NSECD::MESSAGE_HEADER));
        //std::cout<<"Size of data = "<<dataSize<<std::endl;
        memcpy (&msgDnldData.inner_hdr, &(MLResp->msg_hdr), sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, &(MLResp->ParticipantType1), dataSize);
              
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        buf[msgdnldSize] = '\0';
        
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
           snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file  %d", bytesWritten);
           Logger::getLogger().log(DEBUG, logbufMsgdnld);
          exit(1);
        }
        fflush(file);
    
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(MLResp->msg_hdr.TransactionCode),msgDnldData.inner_hdr.TimeStamp1,__bswap_64(msgDnldData.inner_hdr.TimeStamp1));
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file: %s",filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
}


void convertAndWriteMLRespToFile(NSEFO::MS_SPD_OE_REQUEST* MLResp, FileStore&  DnldFileStore)
{
     NSEFO::MS_SPD_OE_REQUEST MLOrder = {0};
     //memcpy (&MLOrder, MLResp, sizeof (NSEFO::MS_SPD_OE_REQUEST));
     
     FILE* file = NULL;
     int32_t TraderId = __bswap_32(MLResp->TraderId1);
     char filename[20] = {"\0"};
     sprintf(filename, "%d", TraderId);
     strcat(filename, "Dnld");
      
     FileItr itr = DnldFileStore.find(std::string(filename));
     if (itr != DnldFileStore.end())
     {
         file = itr->second;
     }
     else
     {
        file = fopen(filename, "a");
        DnldFileStore.insert( std::pair<std::string,FILE*>(std::string(filename), file));
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Inserted in DnlFileStore: %s", filename);
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
      
    if (file != NULL)
    {
        size_t msgdnldSize = sizeof(NSEFO::MS_MESSAGE_DOWNLOAD_DATA);
        char buf[1000];
        memset(buf, 0, sizeof(buf));
        NSEFO::MS_MESSAGE_DOWNLOAD_DATA msgDnldData = {0};
        
        size_t dataSize = sizeof(NSEFO::MS_SPD_OE_REQUEST) - (sizeof(NSEFO::TAP_HEADER) + sizeof(NSEFO::MESSAGE_HEADER));
        //std::cout<<"Size of data = "<<dataSize<<std::endl;
        memcpy (&msgDnldData.inner_hdr, &(MLResp->msg_hdr), sizeof (msgDnldData.inner_hdr));
        memcpy (&msgDnldData.Data, &(MLResp->ParticipantType1), dataSize);
              
        memcpy (buf, (void*)&msgDnldData, msgdnldSize);
        buf[msgdnldSize] = '\0';
        
        int bytesWritten = fwrite(buf, 1, msgdnldSize, file);
        if (bytesWritten != msgdnldSize)
        {
           snprintf(logbufMsgdnld,300,"Thread_Msgdnld|fwrite FAILED..!!Bytes written to file  %d", bytesWritten);
           Logger::getLogger().log(DEBUG, logbufMsgdnld);
          exit(1);
        }
        fflush(file);
    
        snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Added MsgDnld data|%s|%d|%ld|%ld",filename,__bswap_16(MLResp->msg_hdr.TransactionCode),msgDnldData.inner_hdr.TimeStamp1,__bswap_64(msgDnldData.inner_hdr.TimeStamp1));
        Logger::getLogger().log(DEBUG, logbufMsgdnld);
     }
    else
    {
      snprintf(logbufMsgdnld,300,"Thread_Msgdnld|Could not open file: %s",filename);
       Logger::getLogger().log(DEBUG, logbufMsgdnld);
    }
}

void StartLog(ProducerConsumerQueue<DATA_RECEIVED>* qptr, int _segMode, int iMsgDnldCore)
{
    DATA_RECEIVED dataLog;
    FileStore DnldFiles;
        
    TaskSetCPU(iMsgDnldCore);
    Logger::getLogger().log(DEBUG, "Thread_Msgdnld| Message Download started..!!");
    
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
                      convertAndWriteOrdRespToFile (orderRespCM, DnldFiles);
                    }
                    else if (dataLog.MyFd == 2)
                    {
                      NSECM::TRADE_CONFIRMATION_TR* tradeRespCM = (NSECM::TRADE_CONFIRMATION_TR*)dataLog.msgBuffer;  
                      convertAndWriteTrdRespToFile (tradeRespCM, DnldFiles);  
                    }
                    else if (dataLog.MyFd == 3)
                    {
                      NSECM::MS_SL_TRIGGER* SLTrigger = (NSECM::MS_SL_TRIGGER*)dataLog.msgBuffer;  
                      convertAndWriteSLRespToFile (SLTrigger, DnldFiles);  
                    }
                    else if (dataLog.MyFd == 4)
                    {
                      NSECM::MS_OE_RESPONSE_SL* orderRespCM = (NSECM::MS_OE_RESPONSE_SL*)dataLog.msgBuffer;  
                      convertAndWriteOrdRespNonTrimToFile (orderRespCM, DnldFiles);  
                    }
                }
                break;
                case SEG_NSEFO:
                {
                    if (dataLog.MyFd == 1)
                    {
                      NSEFO::MS_OE_RESPONSE_TR* orderRespFO = (NSEFO::MS_OE_RESPONSE_TR*)dataLog.msgBuffer;
                      convertAndWriteOrdRespToFile (orderRespFO, DnldFiles);
                    }
                    else if (dataLog.MyFd == 2)
                    {
                      NSEFO::TRADE_CONFIRMATION_TR* tradeRespFO = (NSEFO::TRADE_CONFIRMATION_TR*)dataLog.msgBuffer;  
                      convertAndWriteTrdRespToFile (tradeRespFO, DnldFiles);  
                    }
                    else if (dataLog.MyFd == 3)
                    {
                      NSEFO::MS_SPD_OE_REQUEST*  MLResp = (NSEFO::MS_SPD_OE_REQUEST*)dataLog.msgBuffer; 
                      convertAndWriteMLRespToFile(MLResp, DnldFiles);  
                    }
                    else if (dataLog.MyFd == 4)
                    {
                      NSEFO::MS_SL_TRIGGER* SLTrigger = (NSEFO::MS_SL_TRIGGER*)dataLog.msgBuffer;  
                      convertAndWriteSLRespToFile (SLTrigger, DnldFiles);  
                    }
                    else if (dataLog.MyFd == 5)
                    {
                      NSEFO::MS_OE_RESPONSE_SL* orderRespFO = (NSEFO::MS_OE_RESPONSE_SL*)dataLog.msgBuffer;  
                      convertAndWriteOrdRespNonTrimToFile (orderRespFO, DnldFiles);  
                    }
                }
                break;
              case SEG_NSECD:
              {
                if (dataLog.MyFd == 1)
                {
                  NSECD::MS_OE_RESPONSE_TR* orderRespFO = (NSECD::MS_OE_RESPONSE_TR*)dataLog.msgBuffer;
                  convertAndWriteOrdRespToFile (orderRespFO, DnldFiles);
                }
                else if (dataLog.MyFd == 2)
                {
                  NSECD::TRADE_CONFIRMATION_TR* tradeRespFO = (NSECD::TRADE_CONFIRMATION_TR*)dataLog.msgBuffer;  
                  convertAndWriteTrdRespToFile (tradeRespFO, DnldFiles);  
                }
                else if (dataLog.MyFd == 3)
                {
                  NSECD::MS_SPD_OE_REQUEST*  MLResp = (NSECD::MS_SPD_OE_REQUEST*)dataLog.msgBuffer; 
                  convertAndWriteMLRespToFile(MLResp, DnldFiles);  
                }
              }
            }
        }
    }
}
