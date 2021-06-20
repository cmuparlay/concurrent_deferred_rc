/*

Copyright 2015 University of Rochester

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 

*/



#ifndef RECORDER_HPP
#define RECORDER_HPP

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif


#include <map>
#include <list>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include "HarnessUtils.hpp"

class Recorder{

private:
	int task_num;

public:
	// member vars
	std::map<std::string, std::string> globalFields;
	std::map<std::string, std::string>* localFields;

	std::map<std::string, void*> summaryFunctions;

	// these methods must ensure no other threads are accessing 
	// the Recorder concurrently
	Recorder(int task_num);
	void addGlobalField(std::string field);
	void addThreadField(std::string s, std::string (*summarizeFunction)(std::list<std::string>));

	void reportGlobalInfo(std::string field, double value);
	void reportGlobalInfo(std::string field, int value);
	void reportGlobalInfo(std::string field, long value);
	void reportGlobalInfo(std::string field, unsigned long value);
	void reportGlobalInfo(std::string field, std::string value);

	std::string getColumnHeader();
	std::string getData();
	std::string getCSV();
	void outputToFile(std::string outFile);
  void summarize();

	// may be called concurrent with other logLocalEntry calls
	void reportThreadInfo(std::string field, double value, int tid);
	void reportThreadInfo(std::string field, int value, int tid);
	void reportThreadInfo(std::string field, long value, int tid);
	void reportThreadInfo(std::string field, uint64_t value, int tid);
	void reportThreadInfo(std::string field, std::string value, int tid);


private:
	bool verifyFile(std::string file);

	static double computeSum(std::list<std::string> list){
		double sum = 0;
		for(std::string s : list){
			sum+=atof(s.c_str());
		}
		return sum;
	}

	static double computeMean(std::list<std::string> list){
		double sum = computeSum(list);
		return sum/list.size();
	}

	static double computeVar(std::list<std::string> list){
		double mean = computeMean(list);
		double temp = 0;
		for(std::string s : list){
			double a = atof(s.c_str());
			temp += (mean-a)*(mean-a);
		}
		return temp/list.size();
	}

	static double computeStdDev(std::list<std::string> list){
		double var = computeVar(list);
		return sqrt(var);
	}

	static std::string ftoa(double f){
		char buff[20];
		sprintf(buff, "%f", f);
		return std::string(buff);
	}

	static std::string itoa(int i){
		char buff[20];
		sprintf(buff, "%d", i);
		return std::string(buff);
	}

	// summary functions
public:
	static std::string sumDoubles(std::list<std::string> list){
		double ans = computeSum(list);
		return std::string(ftoa(ans));
	}
	static std::string sumInts(std::list<std::string> list){
		int ans = (int)computeSum(list);
		return std::string(itoa(ans));
	}
	static std::string sumLongs(std::list<std::string> list){
		long ans = (long)computeSum(list);
		//return std::string(ftoa(ans));
		return std::to_string(ans);
	}
	static std::string sumInt64s(std::list<std::string> list){
		uint64_t ans = (uint64_t)computeSum(list);
		return std::to_string(ans);
	}
	static std::string avgInts(std::list<std::string> list){
		int ans = (int)computeMean(list);
		return std::string(itoa(ans));
	}
	static std::string avgDoubles(std::list<std::string> list){
		double ans = (double)computeMean(list);
		return std::string(itoa(ans));
	}
	static std::string varInts(std::list<std::string> list){
		int ans = (int)computeVar(list);
		return std::string(itoa(ans));
	}
	static std::string varDoubles(std::list<std::string> list){
		double ans = (double)computeVar(list);
		return std::string(itoa(ans));
	}
	static std::string stdDevInts(std::list<std::string> list){
		int ans = (int)computeStdDev(list);
		return std::string(itoa(ans));
	}
	static std::string stdDevDoubles(std::list<std::string> list){
		double ans = (double)computeStdDev(list);
		return std::string(itoa(ans));
	}

	static std::string concat(std::list<std::string> list){
		std::string sum = "";
		for(std::string s : list){
			sum+=s+":";
		}
		return sum;
	}


	static std::string dateTimeString(){
		time_t rawTime;
		struct tm* fancyTime;
		char buff[100];

		time(&rawTime);
		fancyTime = localtime(&rawTime);
		strftime(buff,100,"%d-%m-%Y %I:%M:%S", fancyTime);
		std::string str(buff);
		return str;
	}
	
};

#endif
