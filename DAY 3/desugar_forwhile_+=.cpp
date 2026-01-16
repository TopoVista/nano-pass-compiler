#include <bits/stdc++.h>               // includes all standard libraries
using namespace std;

#define fastio() ios_base::sync_with_stdio(false);cin.tie(NULL)
#define int long long
#define all(a) a.begin(),a.end()

//---------------------------------------------
// trim(): removes leading and trailing spaces/tabs/newlines
//---------------------------------------------
string trim(const string &s){
    string t=s;
    t.erase(0,t.find_first_not_of(" \n\t"));      // erase chars before first non-space
    t.erase(t.find_last_not_of(" \n\t")+1);       // erase chars after last non-space
    return t;                                     // return cleaned string
}

//---------------------------------------------
// desugar_plusassign(): replaces all "x += y;" with "x = x + y;"
//---------------------------------------------
string desugar_plusassign(string code){
    regex r("([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\+=\\s*([^;]+);"); // regex captures <var> and <expr> in "x += y;"
    return regex_replace(code,r,"$1 = $1 + $2;");             // "$1" and "$2" refer to captured groups
}

//---------------------------------------------
// desugar_for(): replaces "for(init; cond; step){body}" with "init; while(cond){ body; step; }"
//---------------------------------------------
string desugar_for(string code){
    regex r("for\\s*\\(([^;]*);([^;]*);([^\\)]*)\\)\\s*\\{([^}]*)\\}"); // regex captures init, cond, step, and body
    smatch m;                       // match object to store results
    string res=code;                // make a copy of code to transform
    while(regex_search(res,m,r)){   // repeatedly find for-loops in code
        string init=trim(m[1].str());   // group 1: initialization part
        string cond=trim(m[2].str());   // group 2: loop condition
        string step=trim(m[3].str());   // group 3: step expression
        string body=trim(m[4].str());   // group 4: loop body
        string replacement=init+";\nwhile("+cond+"){\n"+body+"\n"+step+";\n}"; // build equivalent while loop string
        res=regex_replace(res,r,replacement);  // replace current "for" with constructed "while"
    }
    return res;                      // return transformed code
}

//---------------------------------------------
// main(): reads code, applies both desugar steps, prints result
//---------------------------------------------
signed main(){
    fastio();                        // fast I/O setup
    string input,line;               // will store full code as a string
    while(getline(cin,line))input+=line+"\n";  // read all lines until EOF and combine
    string out=desugar_plusassign(input);      // first desugar all "+=" into "= +"
    out=desugar_for(out);                      // then desugar all "for" loops into "while"
    cout<<out;                                 // print transformed code
    return 0;
}
