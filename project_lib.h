#ifndef PART_1_H
#define PART_1_H

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <mutex>
using namespace std;

class DataBase {
public:
	DataBase();
	virtual ~DataBase();
	virtual void insert(vector<string> vect) = 0;
	virtual void remove(string rule_name) = 0;
	virtual bool check_exist(string drop_name) = 0;
	virtual void print_file(ostream& outfile) = 0;
};

class KnowledgeBase: public DataBase {
private:
	map<string, int> param_map;
	map<string, set<vector<string>>> fact_map;
public:
	KnowledgeBase();
	virtual ~KnowledgeBase();
	void insert(vector<string> vect);
	void remove(string rule_name);
	bool check_exist(string drop_name);
	void print_file(ostream& outfile);
	void query(set<vector<string>>& results, vector<string> vect);
};

class RuleBase: public DataBase {
private:
	multimap<string, vector<string>> rule_map;
public:
	RuleBase();
	virtual ~RuleBase();
	void insert(vector<string> vect);
	void remove(string rule_name);
	bool check_exist(string drop_name);
	void print_file(ostream& outfile);
	multimap<string, vector<string>> get(string relation);
};

class SRI {
private:
	KnowledgeBase KB;
	RuleBase RB;
	void exe_load(string filename);

	void thread_OR(set<vector<string>>& final_results_set, int op_num,
			set<vector<string>> op_results_set, vector<string> base_param_vect,
			multimap<string, pair<int,int>> coords_mmap);
	void thread_AND(vector<string> combo, vector<string> param_order_vect,
		vector<set<vector<string>>> all_results_vect,
		multimap<string, pair<int, int>> coords_mmap,
		vector<string> base_param_vect,
		set<vector<string>>& final_results_set);

	void exe_dump(string filename);
	void exe_fact(vector<string> vect);
	void exe_rule(vector<string> vect);
	void exe_inference(vector<string> vect);
	void exe_drop(string rule_name);
	void evaluate(vector<string> vect,
			set<vector<string>>& final_results_set);
public:
	SRI();
	~SRI();
	void scan(string line);
};

string parse(string& line, string delimiter);
bool invalid_command(vector<string> vect);
void make_combo(int i, vector<string> combo_vect,
		vector<set<string>> valid_results_vect,
		vector<vector<string>>& all_combo_vect);
bool valid_combo(vector<string> combo_vect, vector<string> param_order_vect,
		vector<set<vector<string>>> all_results_vect,
		multimap<string,pair<int,int>> coords_mmap);
void thread_print_constr(thread::id t_id);
void thread_print_destr(thread::id t_id);

#endif