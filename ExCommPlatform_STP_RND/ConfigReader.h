/* 
 * File:   ConfigReader.h
 * Author: NareshRK
 *
 * Created on March 14, 2018, 12:27 PM
 */
#ifndef CONFIGREADER_H
#define CONFIGREADER_H

#include<string>
#include<iostream>
#include<map>

class ConfigReader
{
  std::map<std::string,std::string> _propValMap;

  std::string _filename;
  void parseFile();
  public:
  
  ConfigReader(std::string);
  ~ConfigReader(){}

  void addProperty(std::string property, std::string value);
  void setProperty(std::string property, std::string value);
  void delProperty(std::string property);
  
  std::string getProperty(std::string property);
  void reset();
  void dump();
};

#endif


