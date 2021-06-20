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



#include "Recorder.hpp"
#include <string>     // std::string, std::stoi

using namespace std;

Recorder::Recorder(int task_num){
	this->task_num = task_num;
	this->localFields = new std::map<std::string, std::string>[task_num];
}
void Recorder::addGlobalField(std::string field){
	if(globalFields.count(field)==0){
		globalFields[field]="";
	}
}
void Recorder::addThreadField(std::string field, std::string (*summarizeFunction)(std::list<std::string>)){
	if(globalFields.count(field)==0){
		globalFields[field]="";
		for(int i = 0; i<task_num; i++){
			localFields[i][field]="";
		}
		summaryFunctions[field]=(void*)summarizeFunction;
	}
}

void Recorder::reportGlobalInfo(std::string field, double value){
	globalFields[field]=ftoa(value);
}
void Recorder::reportGlobalInfo(std::string field, int value){
	globalFields[field]=itoa(value);
}
void Recorder::reportGlobalInfo(std::string field, long value){
	//globalFields[field]=itoa(value);
	globalFields[field]=std::to_string(value);
}
void Recorder::reportGlobalInfo(std::string field, unsigned long value){
	globalFields[field]=itoa(value);
}
void Recorder::reportGlobalInfo(std::string field, std::string value){
	globalFields[field]=value;
}

std::string Recorder::getColumnHeader(){
	string out = "";
	for (auto& x: globalFields) {
		out += x.first + ",";
  }
	out.pop_back();
	return out;
}

void Recorder::summarize(){
	for (auto& x: localFields[0]) {
		std::string field = x.first;
		std::list<std::string> list;
		for(int i = 0; i<task_num; i++){
			list.push_back(localFields[i][field]);
		}
		
		std::string (*summarizeFunction)(std::list<std::string>) =
		  (std::string (*)(std::list<std::string>))summaryFunctions.at(field);
		
		globalFields[field]=summarizeFunction(list);
	}
}

std::string Recorder::getData(){
	summarize();

	string out = "";
	for (auto& x: globalFields) {
		out += x.second + ",";
  	}
	out.pop_back();
	return out;
}

std::string Recorder::getCSV(){
	return getColumnHeader()+'\n'+getData();
}

bool Recorder::verifyFile(std::string file){
	std::ifstream f(file.c_str());
	std::string line;
	if(f.bad()){
		   errexit("Unable to open csv output file.");
	}
	std::getline(f, line);
	std::string hdr = getColumnHeader();
	bool ret = (line == hdr);
	if(!ret){
		cerr<<"Header mismatch\n";
		cerr<<"Old header: "<<line<<endl;
		cerr<<"New header: "<<hdr<<endl;
	}
	f.close();
	return ret;
}

void Recorder::outputToFile(std::string outFile){
	// check if file exists
	ofstream f;
	string toAdd;
	if(( access( outFile.c_str(), F_OK ) == -1 )){
		// new file
		f.open (outFile.c_str(),ios::app);
		toAdd = getCSV()+'\n';
	}
	else if(!verifyFile(outFile)){
		// file exists, but problems
		errexit("Output file's header does not match recorded information.");
	}
	else{
		// file exists, no problems
		f.open (outFile.c_str(),ios::app);
		toAdd = getData()+'\n';
	}

	// verify good file handle
	if(f.bad()){
		errexit("Unable to open csv output file.");
	}
	f << toAdd;
	f.close();
}


// may be called concurrent with other logLocalEntry calls
void Recorder::reportThreadInfo(std::string field, double value, int tid){
	localFields[tid].at(field)=ftoa(value);
}
void Recorder::reportThreadInfo(std::string field, int value, int tid){
	localFields[tid].at(field)=itoa(value);
}
void Recorder::reportThreadInfo(std::string field, long value, int tid){
	//localFields[tid].at(field)=itoa(value);
	localFields[tid].at(field)=std::to_string(value);
}
void Recorder::reportThreadInfo(std::string field, uint64_t value, int tid){
	localFields[tid].at(field)=std::to_string(value);
}
void Recorder::reportThreadInfo(std::string field, std::string value, int tid){
	localFields[tid].at(field)=value;
}



// summary functions

