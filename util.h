#ifndef UTIL_H
#define UTIL_H

#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

using namespace std;

vector<string> split(const string str, const string regex_str);
void trim(string &s);
void trim_vector(vector<string> &vec);
void remove_ending_semicolon(string &s);
void remove_ending_semicolon_vector(vector<string> &vec);
string substr_between_indices(const string s, int l, int r);
bool is_substr(const string s1, const string s2);
bool is_array_accessor(const string s);
bool is_array_accessor_dynamic(const string s);
bool is_int(const string s);

vector<string> loadFile(string filename, int &maxlen);
void writeFile(string filename, vector<string> assembly, string f_name);

#endif
