#include<bits/stdc++.h>
using namespace std;
int main(){
	string s = "01/*45*/89";
	auto start = s.find("/*");
	cout << start << endl << s.substr(0, start) << endl;
	return 0;
}