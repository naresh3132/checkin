#include "ConfigReader.h"
#include<fstream>
#include<cctype>
#include<algorithm>
#include "CommonFunctions.h"


void ConfigReader::parseFile()
{
  std::ifstream configFile(_filename.c_str());
  std::string line;

  if(configFile.is_open())
  {
    while(getline(configFile, line))
    {
      // remove white space
      line.erase(std::remove_if(line.begin(), line.end(),(int(*)(int))isspace),line.end());
      
      //Ignoring lines that start with a #. use # for comments.
      if(line[0] == '#')
      {
        continue;
      }
      std::string::size_type pos;
      std::string property;
      std::string value;
      
      if((pos=line.find_first_of('='))!=std::string::npos)
      {
        property = line.substr(0,pos);
        value = line.substr(pos+1,line.length());
        addProperty(property, value);
      }
      //Ignore bad lines
      else
      {
        continue;
      }
    }
  }
};
  
void ConfigReader::addProperty(std::string property, std::string value)
{
  _propValMap[property] = value;
};

void ConfigReader::setProperty(std::string property, std::string value)
{
  _propValMap[property] = value;
};
  
void ConfigReader::delProperty(std::string property)
{
  _propValMap.erase(property);
};

std::string ConfigReader::getProperty(std::string property)
{
  return _propValMap[property];
};
void ConfigReader::reset()
{
  _propValMap.clear();
};

void ConfigReader::dump()
{
  char logBufCR[100] = {0};
  std::map<std::string,std::string>::iterator it;
  for(std::map<std::string,std::string>::iterator it=_propValMap.begin() ; it!=_propValMap.end(); ++it)
  {
      //std::cout<< " Parsing file " <<std::endl;
      //std::cout<<it->first << "=" <<it->second <<std::endl;

// NSEFO *************************************************      
        if(it->first == "MULTI_CAST_IP_ADDR_NSEFO")
        {
            //Exch_Connection_Params_NSEFO.sMulticastIP = it->second;
            //std::cout << "Exch_Connection_Params_NSEFO.sMulticastIP = " << Exch_Connection_Params_NSEFO.sMulticastIP << std::endl;
        }   
      
        if(it->first == "MULTI_CAST_PORT_NUMBER_NSEFO")
        {
            //Exch_Connection_Params_NSEFO.sMPort = it->second;
        }   

        if(it->first == "INTERFACE_IP_ADDR_NSEFO")
        {
            //Exch_Connection_Params_NSEFO.sLocalIP = it->second;
        }       
      
// NSEFO *************************************************              
        if(it->first == "MULTI_CAST_IP_ADDR_NSECM")
        {
            //Exch_Connection_Params_NSECM.sMulticastIP = it->second;
            //std::cout << "Exch_Connection_Params_NSECM.sMulticastIP = " << Exch_Connection_Params_NSECM.sMulticastIP << std::endl;
        }   
      
        if(it->first == "MULTI_CAST_PORT_NUMBER_NSECM")
        {
            //Exch_Connection_Params_NSECM.sMPort = it->second;
        }   

        if(it->first == "INTERFACE_IP_ADDR_NSECM")
        {
            //Exch_Connection_Params_NSECM.sLocalIP = it->second;
        }       
// NSEFO *************************************************      
        if(it->first == "MULTI_CAST_IP_ADDR_BSECM")
        {
            //Exch_Connection_Params_BSECM.sMulticastIP = it->second;
            //std::cout << "Exch_Connection_Params_BSECM.sMulticastIP = " << Exch_Connection_Params_BSECM.sMulticastIP << std::endl;
        }   
      
        if(it->first == "MULTI_CAST_PORT_NUMBER_BSECM")
        {
            //Exch_Connection_Params_BSECM.sMPort = it->second;
        }   

        if(it->first == "INTERFACE_IP_ADDR_BSECM")
        {
            //Exch_Connection_Params_BSECM.sLocalIP = it->second;
        }       
// Key File Path *************************************************              
        if(it->first == "KEY_MDP_TO_PLATFORM")
        {
            //strcpy(cKeyPathMDPToPlatform,it->second.c_str());
        }       
        if(it->first == "KEY_PLATFORM_TO_MDP")
        {
            //strcpy(cKeyPathPlatformToMDP,it->second.c_str());
        }    
        if(it->first == "FILE_IRMS")
        {
            //strcpy(ciRMSFile,it->second.c_str());
        }   
        snprintf(logBufCR,100,"%s: %s",it->first.c_str(),it->second.c_str());
        std::cout<<it->first<<":"<<it->second<<std::endl;
  }
};

ConfigReader::ConfigReader(std::string filename):_filename(filename)
{
  parseFile();  
};
