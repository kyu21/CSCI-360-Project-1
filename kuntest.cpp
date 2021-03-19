#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>
#include <algorithm> 
#include <cctype>
#include <locale>

#include "Function.h"
#include "Variable.h"

using namespace std;

void common_instruction_handler_dispatcher(vector<string> source, int &loc, int max_len, Function &f1, int &addr_offset);
void variable_offset_allocation(vector<string> &source, int &loc, Function &f1, int &addr_offset) ;
void IF_statement_handler(vector<string> &source, int &loc, int max_len, Function &f1, int &addr_offset);

vector<string> split(const string str, const string regex_str);
void trim(string &s);
void trim_vector(vector<string> &vec);
void remove_ending_semicolon(string &s);
void remove_ending_semicolon_vector(vector<string> &vec);
string substr_between_indices(const string s, int l, int r);
bool is_substr(const string s1, const string s2);
bool is_array_accessor(const string s);
bool is_int(const string s);

void get_arr_val_into_register(const string s, const string reg, Function &f1);

string register_for_argument_32[6] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
string register_for_argument_64[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

int label_num = 2;

/*
    According to the instruction type, call the corresponding handler.
*/
void common_instruction_handler_dispatcher(vector<string> source, int &loc, int max_len, Function &f1, int &addr_offset) {
    /*
        code line starts with variable declaration keyword "int" and ends with semicolon
    */
    if (source[loc].find("int") == 0 && source[loc].find(";") == source[loc].length() - 1) {
        variable_offset_allocation(source, loc, f1, addr_offset);
        loc++;
    }
    /*
        code line starts with "if"
    */
    else if (source[loc].find("if") == 0) {
        f1.assembly_instructions.push_back("# " + source[loc]);
        IF_statement_handler(source, loc, max_len, f1, addr_offset);
    }
    else {
        loc++;
    }
}

/*
    Helper function which given a string and a delimiter, creates a iterator of the split tokens
    Sourced from: https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
*/
vector<string> split(const string str, const string regex_str) {
    return {sregex_token_iterator(str.begin(), str.end(), regex(regex_str), -1), sregex_token_iterator()};
}

/*
    Helper function to remove whitespace from string on both ends in place
    Sourced from: https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
*/
void trim(string &s) {
    // trim from start
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !isspace(ch);
    }));

    // trim from end
    s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !isspace(ch);
    }).base(), s.end());
}

/*
    Helper function to run trim on all elements of a vector
*/
void trim_vector(vector<string> &vec) {
    for (auto &v: vec) {
        trim(v);
    }
}

/*
    Helper function to remove ending semicolon
*/
void remove_ending_semicolon(string &s) {
    if (s.find(";") == s.length() - 1) {
        s.pop_back();
    }
}

/*
    Helper function to remove ending semicolon from vector
*/
void remove_ending_semicolon_vector(vector<string> &vec) {
    for (auto &v: vec) {
        remove_ending_semicolon(v);
    }
}

/*
    Helper function to get s[l:r] substring where l is inclusive and r is exclusive
*/
string substr_between_indices(const string s, int l, int r) {
    return s.substr(0, r).substr(l);
}

/*
    Helper function to determine if a string is a substring of another
*/
bool is_substr(const string s1, const string s2) {
    return s1.find(s2) != string::npos;
}

/*
    Helper function to determine if a line of code is accessing an array
*/
bool is_array_accessor(const string s) {
    return is_substr(s, "[") && is_substr(s, "]");
}

/*
    Helper function to determine if string is int
    Sourced from: https://stackoverflow.com/questions/2844817/how-do-i-check-if-a-c-string-is-an-int
*/
bool is_int(const string s) {
  return s.find_first_not_of( "0123456789" ) == string::npos;
}

/*
    Handle variable declaration statements.
    If "[" and "]" in line, then it's an array declaration,
    otherwise it is a primative variable declaration.
*/
void variable_offset_allocation(vector<string> &source, int &loc, Function &f1, int &addr_offset) {
    if (is_array_accessor(source[loc])) {
        /*
            Split line by = and then by the [] to parse the name, size and values
        */
        string var_type = "int";
        auto tokens = split(source[loc].substr(source[loc].find(" ")+1), " = "); // only work with part after "int "
        trim_vector(tokens);
        remove_ending_semicolon_vector(tokens);

        auto array_name = tokens[0];
        auto temp = split(array_name, "\\["); // [ needs to be escaped with \ and then that \ needs to be escaped for C++ complier
        array_name = temp[0];
        
        int array_size = stoi(temp[1].substr(0, temp[1].size()-1));
        
        auto array_values_str = tokens[1];
        auto array_values = split(array_values_str.substr(1, array_values_str.size()-2), ", "); // removes { and } and splits into int values

        for (int i = array_size-1; i >= 0; --i) {
            int val = stoi(array_values[i]);
            string name = array_name + "[" + to_string(i) + "]";
            int arr_addr_offset = addr_offset - (i * 4);

            Variable var (name, var_type, val, arr_addr_offset);
            f1.variables.insert(pair<string, Variable> (name, var));
            f1.assembly_instructions.push_back("movl $" + array_values[i] + ", " + to_string(arr_addr_offset) + "(%rbp)");
        }
        addr_offset -= (array_size * 4);
    } else {
        
        /*
            Split variables by , and then by space to parse the name and value
        */
        string var_type = "int";
        auto tokens = split(source[loc].substr(source[loc].find(" ")+1), ","); // only work with part after "int "
        trim_vector(tokens);
        remove_ending_semicolon_vector(tokens);

        for (auto& item: tokens) {
            auto var_tokens = split(item, " ");
            string var_name = var_tokens[0];
            int var_value = stoi(var_tokens[2]);

            Variable var (var_name, var_type, var_value, addr_offset);
            f1.variables.insert(pair<string, Variable> (var_name, var));
            f1.assembly_instructions.push_back("movl $" + var_tokens[2] + ", " + to_string(addr_offset) + "(%rbp)");
            addr_offset -= 4;
        }
    }
}

/*
    Helper function to handle code like a[0] and a[f] which accesses array elements
    Pushes required assembly instructions to get that value into specified register
*/
void get_arr_val_into_register(const string s, const string reg, Function &f1) {
    string arr_name = s.substr(0, s.find("["));
    string arr_index = substr_between_indices(s, s.find("[")+1, s.find("]"));
    
    if (is_int(arr_index)) {
        /*
            a[immediate]
            meaning this should be in f1.variables
        */
       f1.assembly_instructions.push_back("movl " + to_string(f1.variables.at(s).addr_offset) + "(%rbp), " + reg);
    } else {
        /*
            a[var]
        */
        // move arr_index into eax
        f1.assembly_instructions.push_back("movl " + to_string(f1.variables.at(arr_index).addr_offset) + "(%rbp), %eax");
        f1.assembly_instructions.push_back("cltq");
        // get where arr_name[0] is
        string arr_start_offset = to_string(f1.variables.at(arr_name + "[0]").addr_offset);
        f1.assembly_instructions.push_back("movl " + arr_start_offset + "(%rbp, %rax, 4), " + reg);
    }
}

/*
    Helper function to handle code like thing1 comparator thing2
    Pushes required assembly instructions for comparison
*/
void comparison_handler(string &s, Function &f1, int &loc) {
    // mapping of all comparators and their associated assembly command
    map<string, string> comparators;
    comparators.insert(pair<string, string>("<", "jge"));
    comparators.insert(pair<string, string>(">", "jle"));
    comparators.insert(pair<string, string>("<=", "jg"));
    comparators.insert(pair<string, string>(">=", "jl"));
    comparators.insert(pair<string, string>("==", "jne"));
    comparators.insert(pair<string, string>("!=", "je"));

    auto tokens = split(s, " ");
    trim_vector(tokens);
    string l_comp = tokens[0];
    string r_comp = tokens[2];
    string comp = tokens[1];

    for (auto const& c: comparators) {
        string comparator = c.first;
        string command = c.second;

        if (is_substr(s, comparator)) {
            string label_instruction = ".L" + to_string(label_num);

            if (is_array_accessor(l_comp) || is_array_accessor(r_comp)) {
                /*
                    Handles where one of the comparands is accessing an array
                    - array array
                    - array var
                    - array immediate
                    - var array
                    - immediate array
                */
                if (is_array_accessor(l_comp) && is_array_accessor(r_comp)) {
                    // array array
                    get_arr_val_into_register(l_comp, "%edx", f1);
                    get_arr_val_into_register(r_comp, "%eax", f1);

                    f1.assembly_instructions.push_back("cmpl %eax, %edx");
                } else if(is_array_accessor(l_comp)) {
                    get_arr_val_into_register(l_comp, "%eax", f1);
                    if (is_int(r_comp)) {
                        // array immediate
                        r_comp = "$" + r_comp;
                    } else {
                        // array var
                        r_comp = to_string(f1.variables.at(r_comp).addr_offset) + "(%rbp)";
                    }
                    /*
                        reverse the comparands here to ensure that the map holds
                        godbolt shows c[f] < a as
                            cmpl %eax, -4(%rbp)
                            jle .L3
                        but we want it as:
                            cmpl -4(%rbp), %eax
                            jge .L3
                    */ 
                    f1.assembly_instructions.push_back("cmpl " + r_comp + ", %eax");
                } else if(is_array_accessor(r_comp)) {
                    get_arr_val_into_register(r_comp, "%eax", f1);
                    if (is_int(l_comp)) {
                        // immediate array
                        l_comp = "$" + l_comp;
                    } else {
                        // var array
                        l_comp = to_string(f1.variables.at(l_comp).addr_offset) + "(%rbp)";
                    }

                    f1.assembly_instructions.push_back("cmpl %eax, " + l_comp);
                }
            } else if (is_int(l_comp) || is_int(r_comp)){
                /*
                    Handles where one of the comparands is an immediate and none are arrays
                    - immediate immediate
                    - immediate var
                    - var immedaite
                */
                if (is_int(l_comp) && is_int(r_comp)) {
                    /*
                        Doesn't handle case of when both are immediates
                    */
                } else if (is_int(l_comp)) {
                    r_comp = to_string(f1.variables.at(r_comp).addr_offset) + "(%rbp)";

                    f1.assembly_instructions.push_back("cmpl $" + l_comp + ", " + r_comp);
                } else {
                    l_comp = to_string(f1.variables.at(l_comp).addr_offset) + "(%rbp)";

                    f1.assembly_instructions.push_back("cmpl $" + r_comp + ", " + l_comp);
                }
            } else {
                /*
                    Else case is if both comparands are variables(non-array)
                    - var var
                */
                l_comp = to_string(f1.variables.at(l_comp).addr_offset) + "(%rbp)";
                r_comp = to_string(f1.variables.at(r_comp).addr_offset) + "(%rbp)";

                f1.assembly_instructions.push_back("movl " + l_comp + ", %eax");
                f1.assembly_instructions.push_back("cmpl " + r_comp + ", %eax");
            }

            f1.assembly_instructions.push_back(command + " " + label_instruction);

        }
    }
}

/*
    Handle if statements
*/
void IF_statement_handler(vector<string> &source, int &loc, int max_len, Function &f1, int &addr_offset) {
    // cout << source[loc] << endl;
    string comparison = substr_between_indices(source[loc], source[loc].find("(")+1, source[loc].find(")"));
    comparison_handler(comparison, f1, loc);
    loc++;

    while (source[loc] != "}") {
        common_instruction_handler_dispatcher(source, loc, max_len, f1, addr_offset);
    }

    f1.assembly_instructions.push_back("# }");
    f1.assembly_instructions.push_back(".L" + to_string(label_num) + ":");
    label_num++;
    loc++;
}

void intialize_function(Function &f1) {
    f1.return_type = "int";
    f1.function_name = "test";
}

void view_function(Function &f1) {
    for (auto x: f1.assembly_instructions) {
        cout << x << endl;
    }

    // for (auto const& x: f1.variables) {
    //     cout << x.second.type << " " << x.first << " " << x.second.value << " " << x.second.addr_offset << endl;
    // }
}

vector<string> loadFile(string filename, int &maxlen) {
    ifstream inputFile;
    inputFile.open(filename);
    string inputLine;

    if (inputFile.fail()) {
        cout << "Failed to open file." << endl;
    } else {
        cout << "Opening file successful" << endl;
    }

    vector<string> sourceCode;
    while (!inputFile.eof()) {
        getline(inputFile, inputLine);
        trim(inputLine);
        sourceCode.push_back(inputLine);
        maxlen++;
    }

    inputFile.close();

    return sourceCode;
}

int main() {
    int max_len = 0;
    vector<string> sources = loadFile("kuntest1.cpp", max_len);;
    int loc = 0;
    Function f1;
    intialize_function(f1);
    int addr_offset = -4;

    while (loc < max_len){
        common_instruction_handler_dispatcher(sources, loc, max_len, f1, addr_offset);
    }
    
    view_function(f1);
}