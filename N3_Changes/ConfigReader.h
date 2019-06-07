/* 
 * File:   ConfigReader.h
 * Author: NareshRK
 *
 * Created on August 29, 2018, 5:21 PM
 */

#ifndef CONFIGREADER_H
#define CONFIGREADER_H

#include<string>
#include<iostream>
#include<map>
#include "log.h"

class ConfigReader
{
  std::map<std::string,std::string> _propValMap;

  std::string _filename;
  char _delimiter;
  void parseFile();
  public:
  
  ConfigReader(std::string,const char delimiter);
  ~ConfigReader(){}

  void addProperty(std::string property, std::string value);
  void setProperty(std::string property, std::string value);
  void delProperty(std::string property);
  
  std::string getProperty(std::string property);
  void reset();
  void dump();
};

#endif

