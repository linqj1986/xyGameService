#ifndef C_PARSE_INI_FILE_H_
#define C_PARSE_INI_FILE_H_

#include <fstream>
#include <iostream>
#include <string>
#include <map>
using namespace std;

#define COMMENT_CHAR '#'

class CParseIniFile
{
public:
	CParseIniFile();
	~CParseIniFile();
	string GetValue(const string& filename, const string&  strSection,const string&  strKey);
	
private:
	bool ReadConfig(const string& filename, map<string, string>& mContent, const char* section);
	bool AnalyseLine(const string & line, string & key, string & val);
	void Trim(string & str);
	bool IsSpace(char c);
	bool IsCommentChar(char c);
	void PrintConfig(const map<string, string> & mContent);

};

#endif
