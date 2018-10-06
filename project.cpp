#include "project_lib.h"

mutex mtx;
int t_num = 0;

//----------------------------------SRI Class----------------------------------
//---------------------------------SRI:Private---------------------------------

//opens a file and scans each line
void SRI::exe_load(string filename) {

	//opens file to read and check for error
	fstream infile;
	infile.open(filename, ios::in);
	if( !infile.is_open() ) {
		string err = "unable to open file\n";
		throw err;
	}

	//read the file line by line and call scan() on each line
	string line;
	while( getline(infile, line) ) {

		//remove comments
		size_t position = line.find(" #");
		if(position != string::npos) {
			line = line.substr(0, position);
		}
		position = line.find("#");
		if(position != string::npos) {
			line = line.substr(0, position);
		}

		//use try catch block to catch errors
		try {
			scan(line);
		}
		catch(string err) {
			cerr << err;
		}
	}
	infile.close();
}

//prints the contents of the KB and RB into a specified file
void SRI::exe_dump(string filename) {
	streambuf* buffer;

	//if given a filename, open file to write to and pass it into both the RB's
	//	and KB's print function
	ofstream outfile;
	outfile.open(filename);

	//prints to cout if filename is empty
	if( !filename.empty() ) {
	    buffer = outfile.rdbuf();
	}
	else {
	    buffer = cout.rdbuf();
	}

	ostream out(buffer);
	KB.print_file(out);
	RB.print_file(out);
	outfile.close();
}

//insert a fact into the KB
void SRI::exe_fact(vector<string> vect) {
	KB.insert(vect);
}

//insert a rule into the RB
void SRI::exe_rule(vector<string> vect) {
	RB.insert(vect);
}

//for each result of the query of an operand (e.g. results of Father($$,$$)),
//	find the location of each base parameter (e.g. GrandFather($a,$b)) inside
//	the results and push it into a temp vector. Then insert that vector into
//	final_results set.
void SRI::thread_OR(set<vector<string>>& final_results_set, int op_num,
		set<vector<string>> op_results_set, vector<string> base_param_vect,
		multimap<string,pair<int,int>> coords_mmap) {

	//print thread id
	thread_print_constr( this_thread::get_id() );

	//a = iterator of op_results_set
	//(e.g. { [John,Gabriel],[Robert,Allen],... } )
	//*a = vector<string> (e.g. [John,Gabriel] )
	for(auto a=op_results_set.begin(); a!=op_results_set.end(); a++) {

		//this holds the corresponding value of the given base parameters
		vector<string> temp_vect;

		//b = iterator of base_param_vect (e.g. [$a,$b,$c,...] )
		//*b = string (e.g. "$a")
		for(auto b=base_param_vect.begin(); b!=base_param_vect.end(); b++) {

			//check if the base parameter exists in the
			//	location map
			//c = iterator of mmap for only (*b) keys
			//(e.g. $b [ <0,2>,<1,2>,... ] )
			auto c = coords_mmap.equal_range(*b);

			//d = iterator of mmap
			//(e.g. d->first = "$b", d->second = <1,2>)
			for(auto d=c.first; d!=c.second; d++) {
				if(d->second.first == op_num) {
					temp_vect.push_back( (*a)[d->second.second-1] );
				}
			}
		}
		
		//only insert temp into final_results_set if size is the
		//	same
		if( temp_vect.size()==base_param_vect.size() ) {
			final_results_set.insert( temp_vect );
		}
	}

	thread_print_destr( this_thread::get_id() );
}

//creates a thread for each possible combination
void SRI::thread_AND(vector<string> combo, vector<string> param_order_vect,
		vector<set<vector<string>>> all_results_vect,
		multimap<string, pair<int, int>> coords_mmap,
		vector<string> base_param_vect,
		set<vector<string>>& final_results_set) {

	thread_print_constr( this_thread::get_id() );

	if( valid_combo(combo, param_order_vect, all_results_vect, 
			coords_mmap) ) {
		combo.resize(base_param_vect.size());
		final_results_set.insert(combo);
	}

	thread_print_destr( this_thread::get_id() );
}

//continues to be called recursively until it is able to return FACTs found in
//	the KB
void SRI::evaluate(vector<string> vect,
		set<vector<string>>& final_results_set) {

	//print thread id
	thread_print_constr( this_thread::get_id() );

	//parse original parameters into vector
	string orig_param_str = vect[1];

	//this will hold parsed original parameters
	vector<string> orig_param_vect;
	while( !orig_param_str.empty() ) {
		orig_param_vect.push_back( parse(orig_param_str, ",") );
	}

	//check if relation exists in KB
	if( KB.check_exist(vect[0]) ) {
		KB.query(final_results_set, vect);
	}

	else { //since relation does not exist in KB

		//print an error if relation does not exist in RB
		if( !RB.check_exist(vect[0]) ) {
			string err = "ERROR: INFERENCE failed\n" + vect[0] + " relation "
							"does not exist in KnowledgeBase nor RuleBase\n";
			throw err;
		}

		//this map holds the rules for the INFERENCE
		auto rule_mmap = RB.get(vect[0]);

		//for all versions of this rule
		auto value_iter = rule_mmap.equal_range(vect[0]);
		for(auto i=value_iter.first; i!=value_iter.second; i++) {

			//holds each set of results of an operand
			vector<set<vector<string>>> all_results_vect;

			//holds the parameter location information of each operand
			multimap<string,pair<int,int>> coords_mmap;

			vector<thread> thread_vect; //holds all the threads in order

			set<string> param_set;

			auto rule_vect = i->second; //look at the rule one at a time

			//parse base parameters
			string base_param_str = rule_vect[1];
			vector<string> base_param_vect;
			while( !base_param_str.empty() ) {
				base_param_vect.push_back( parse(base_param_str, ",") );
			}

			//enter base parameter locations
			for(int x=0; x<base_param_vect.size(); x++) {
				auto coord = make_pair(0,x+1);
				coords_mmap.insert( make_pair(base_param_vect[x],
						coord) );
				param_set.insert(base_param_vect[x]);
			}

			//check if incorrect number of parameters
			if(orig_param_vect.size() != base_param_vect.size()) {
				string err = "ERROR: INFERENCE failed\ntrying to INFERENCE "
								"with invalid amount of parameters\n";
				throw err;
			}

			//parse the string of operands into a vector
			string all_operands = rule_vect[3];
			vector<string> all_op_vect;
			while ( !all_operands.empty() ) {
				all_op_vect.push_back( parse(all_operands, " ") );
			}

			//holds all the updated op_vect's
			vector<vector<string>> updated_op_vect;

			//look at each operand individually
			for(int x=0; x<all_op_vect.size(); x++) {

				//parse the current operand into relation name and parameters
				string operand = all_op_vect[x];
				vector<string> op_vect;
				op_vect.push_back( parse(operand, "(") );
				op_vect.push_back( parse(operand, ")") );

				//parse operand parameters
				string op_param_str = op_vect[1];
				vector<string> op_param_vect;
				while( !op_param_str.empty() ) {
					op_param_vect.push_back( parse(op_param_str, ",") );
				}

				//substitute original parameters into operand parameters and
				//	update coords_mmap and param_set
				for(int y=0; y<op_param_vect.size(); y++) {
					string RHS = op_param_vect[y];

					//update coords_mmap and param_set
					coords_mmap.insert( make_pair( RHS, make_pair(x+1,y+1)));
					param_set.insert(RHS);
					for(int z=0; z<base_param_vect.size(); z++) {
						string LHS = base_param_vect[z];
						if(LHS.compare(RHS)==0) {
							op_param_vect[y] = orig_param_vect[z];
						}
					}
				}

				//update op_vect[1] with new parameters
				string t1 = op_param_vect[0];
				for(int y=1; y<op_param_vect.size(); y++) {
					string t2 = "," + op_param_vect[y];
					t1 += t2;
				}
				op_vect[1] = t1;

				//push the updated op_vect into the outer updated_op_vect
				updated_op_vect.push_back(op_vect);
			}

			//holds outputs of evaluate on each operand
			vector<set<vector<string>>> op_result_set_vect;

			//sets up op_result_set_vect by placing an empty set into each
			//	element
			for(int x=0; x<updated_op_vect.size(); x++) {
				set<vector<string>> op_result_set;
				op_result_set_vect.push_back(op_result_set);
			}

			//creates a thread(evaluate) for each operand stores the thread in
			//	a vector
			for(int x=0; x<updated_op_vect.size(); x++) {
				thread_vect.push_back( thread(&SRI::evaluate, this,
						updated_op_vect[x], ref(op_result_set_vect[x])) );
			}

			//join all the threads
			for(int x=0; x<thread_vect.size(); x++) {
				thread_vect[x].join();
			}

			//place the results of each evaluate into all_results_vect
			for(auto x=op_result_set_vect.begin();
					x!=op_result_set_vect.end(); x++) {
				all_results_vect.push_back(*x);
			}

			//thread the OR section
			if( rule_vect[2].compare("OR")==0 ) {

				//create a thread for each operand to execute OR in parallel
				vector<thread> thread_vect2;
				for(int a=0; a<all_results_vect.size(); a++) {
					thread_vect2.push_back( thread(&SRI::thread_OR, this,
							ref(final_results_set), a, all_results_vect[a],
							base_param_vect, coords_mmap) );
				}
				for(auto a=thread_vect2.begin(); a!=thread_vect2.end(); a++) {
					(*a).join();
				}
			}

			//operator is AND
			else {
				//determine which parameters are couplers
				set<string> coupler_set = param_set;
				for(auto a=coupler_set.begin(); a!=coupler_set.end();) {
					int z = 0;
					auto b = coords_mmap.equal_range(*a);
					for(auto c=b.first; c!=b.second; c++) {
						if(c->second.first != 0) {
							z++;
						}
					}
					if(z<2) {
						auto c = a;
						a++;
						coupler_set.erase(c);
					}
					else {
						a++;
					}
				}

				//remove irrelevant results
				for(auto a=coupler_set.begin(); a!=coupler_set.end(); a++) {
					auto b = coords_mmap.equal_range(*a);
					for(auto c=b.first; c!=b.second; c++) {
						c++;
						if(c==b.second) {
							break;
						}
						c--;
						if(c->second.first==0) {
							c++;
						}
						set<vector<string>> temp_lhs;
						set<vector<string>> temp_rhs;
						for(auto d=all_results_vect[c->second.first-1].begin();
								d!=all_results_vect[c->second.first-1].end();
								d++) {
							string e = (*d)[c->second.second-1];
							c++;
							for(auto f=all_results_vect[c->second.first-1].begin();
									f!=all_results_vect[c->second.first-1].end();
									f++) {
								if( (*f)[c->second.second-1].compare(e)==0 ) {
									temp_lhs.insert(*d);
									temp_rhs.insert(*f);
								}
							}
							c--;
						}
						all_results_vect[c->second.first-1] = temp_lhs;
						c++;
						all_results_vect[c->second.first-1] = temp_rhs;
						c--;
					}
				}

				//set up a vector that holds base parameters + other parameters
				//	in order
				vector<string> param_order_vect = base_param_vect;
				for(auto a=param_set.begin(); a!=param_set.end(); a++) {
					bool not_exist = true;
					for(int b=0; b<param_order_vect.size(); b++) {
						if((*a).compare(param_order_vect[b])==0) {
							not_exist = false;
							break;
						}
					}
					if(not_exist) {
						param_order_vect.push_back(*a);
					}
				}

				//insert all possible, valid values for each parameter into set
				//note: vector is ordered by base parameters first
				vector<set<string>> valid_results_vect;
				for(auto a=param_order_vect.begin();
						a!=param_order_vect.end(); a++) {
					set<string> results_set;
					auto b=coords_mmap.equal_range(*a);
					for(auto c=b.first; c!=b.second; c++) {
						if(c->second.first > 0) {
							for(auto d=all_results_vect[c->second.first-1].begin();
									d!=all_results_vect[c->second.first-1].end();
									d++) {
								results_set.insert((*d)[c->second.second-1]);
							}
						}
					}
					valid_results_vect.push_back(results_set);
				}

				//create all possible combinations out of parameters
				vector<vector<string>> all_combo_vect;
				vector<string> combo_vect;
				make_combo(0, combo_vect, valid_results_vect, all_combo_vect);

				//insert only valid combinations
				//thread the AND
				vector<thread> thread_vect2;
				for(int a=0; a<all_combo_vect.size(); a++) {
					thread_vect2.push_back( thread(&SRI::thread_AND, this,
							all_combo_vect[a], param_order_vect,
							all_results_vect, coords_mmap, base_param_vect,
							ref(final_results_set)) );
				}
				for(int a=0; a<thread_vect2.size(); a++) {
					thread_vect2[a].join();
				}
			}
		}
	}
	thread_print_destr( this_thread::get_id() );
}

//make an inference
void SRI::exe_inference(vector<string> vect) {

	set<vector<string>> final_results_set; //will hold final results
	evaluate(vect, final_results_set);

	//check if we should store the results as FACTS
	if( vect.size()==3 ) {

		//i=set of vectors iterator
		for(auto i=final_results_set.begin(); i!=final_results_set.end();
				i++) {

			vector<string> new_fact_vect; //will be passed to exe_fact()

			//concatenate parameters into a single string for new fact
			auto j=(*i).begin();
			string new_fact_param_str = *j;
			j++;
			for(; j!=(*i).end(); j++) {
				string temp = "," + *j;
				new_fact_param_str += temp;
			}

			//insert the relation and the parameters into a vector	
			new_fact_vect.push_back(vect[2]); //name of relation
			new_fact_vect.push_back(new_fact_param_str); //parameters
			exe_fact(new_fact_vect); //insert new fact
		}
	}

	else { //else print out results on terminal
		for(auto i=final_results_set.begin(); i!=final_results_set.end();
				i++) {
			auto j=(*i).begin();
			cout << "[ " << *j;
			j++;
			for(; j!=(*i).end(); j++) {
				cout << " , " << *j;
			}
			cout << " ]\n";
		}
	}
	if( final_results_set.empty() ) {
		cout << "INFERENCE returned 0 results\n";
	}
}

//remove a fact from the KB or a rule from the RB
void SRI::exe_drop(string name) {
	if( !KB.check_exist(name) && !RB.check_exist(name) ) {
		string err = "ERROR: DROP Failed\n" + name + " does not exist\n";
		throw err;
	}
	KB.remove(name);
	RB.remove(name);
}

//---------------------------------SRI:Public---------------------------------

//constructor for SRI object
SRI::SRI() {}

//destructor for SRI object
SRI::~SRI() {}

//takes in command, parses it, and executes the correct command
void SRI::scan(string line) {
	string err = "ERROR: invalid command\n";
	vector<string> vect; //used to store current, parsed line
	vect.push_back(	parse(line, " ") );
	//determine the correct command to execute and parse the line accordingly
	if( vect[0].compare("#")==0 || vect[0].empty() ) {}
	else if( vect[0].compare("LOAD")==0 ) {
		vect.erase( vect.begin() ); //remove "LOAD" from vector
		vect.push_back( parse(line, " ") );
		//check if invalid command
		if( !line.empty() || invalid_command(vect) ) {
			err += "did not execute \"LOAD\"\n";
			throw err;
		}
		exe_load(vect[0]);
	}
	else if( vect[0].compare("DUMP")==0 ) {
		vect.erase( vect.begin() ); //remove "DUMP" from vector
		vect.push_back( parse(line, " ") );
		//check if invalid command
		if( !line.empty() ) {
			err += "did not execute \"DUMP\"\n";
			throw err;
		}
		exe_dump(vect[0]);
	}
	else if( vect[0].compare("FACT")==0 ) {
		vect.erase( vect.begin() ); //remove "FACT" from vector
		vect.push_back( parse(line, "(") );
		vect.push_back( parse(line, ")") );
		//check if invalid command
		if( !line.empty() || invalid_command(vect) ) {
			err += "did not execute \"FACT\"\n";
			throw err;
		}
		exe_fact(vect);
	}
	else if( vect[0].compare("RULE")==0 ) {
		vect.erase( vect.begin() ); //remove "RULE" from vector
		vect.push_back( parse(line, "(") );
		vect.push_back( parse(line, "):- ") );
		vect.push_back( parse(line, " ") );
		vect.push_back( parse(line, "\n") );
		//check if invalid command
		if( !line.empty() || invalid_command(vect) ) {
			err += "did not execute \"RULE\"\n";
			throw err;
		}
		exe_rule(vect);
	}
	else if( vect[0].compare("INFERENCE")==0 ) {
		vect.erase( vect.begin() ); //remove "INFERENCE" from vector
		vect.push_back( parse(line, "(") );
		vect.push_back( parse(line, ")") );
		//check if the inference will also create facts out of the results
		if( !line.empty() ) {
			vect.push_back( parse(line, " ") );
			vect.erase( vect.rbegin().base()-1 );
			vect.push_back( parse(line, " ") );
		}
		//check if invalid command
		if( !line.empty() || invalid_command(vect) ) {
			err += "did not execute \"INFERENCE\"\n";
			throw err;
		}
		exe_inference(vect);
	}
	else if( vect[0].compare("DROP")==0 ) {
		vect.erase( vect.begin() ); //remove "DROP" from vector
		vect.push_back( parse(line, " ") );
		//check if invalid command
		if( !line.empty() || invalid_command(vect) ) {
			err += "did not execute \"DROP\"\n";
			throw err;
		}
		exe_drop(vect[0]);
	}
	else if( vect[0].compare("EXIT")==0 ) {
		if( !line.empty() ) {
			err += "did not execute \"EXIT\"\n";
			throw err;
		}
		cout << "\n-GOODBYE-\n\n";
	}
	else {
		throw err;
	}
}










//-------------------------------DataBase Class-------------------------------
//------------------------------DataBase:Private------------------------------
//-------------------------------DataBase:Public-------------------------------

//constructor for KnowledgeBase object
DataBase::DataBase() {}

//destructor for KnowledgeBase object
DataBase::~DataBase() {}










//-----------------------------KnowledgeBase Class-----------------------------
//----------------------------KnowledgeBase:Private----------------------------
//----------------------------KnowledgeBase:Public----------------------------

//constructor for KnowledgeBase object
KnowledgeBase::KnowledgeBase() {}

//destructor for KnowledgeBase object
KnowledgeBase::~KnowledgeBase() {}

//inserts a fact into the Knowledge Base
void KnowledgeBase::insert(vector<string> vect) {
	//parse the parameters to be used for inserting

	string param_str = vect[1]; //access the parameters of the fact

	//a vector to store parsed parameters
	vector<string> param_vect;
	while( !param_str.empty() ) {
		param_vect.push_back( parse(param_str, ",") );
	}

	//check if relation does not yet exist
	if( fact_map.find(vect[0]) == fact_map.end() ) {
		set<vector<string>> fact_set;
		fact_map.insert( make_pair(vect[0], fact_set) );
	}
	if( param_map.find(vect[0]) == param_map.end() ) {
		param_map.insert( make_pair(vect[0], param_vect.size()) );
	}

	//check if current FACT DOES NOT have appropriate number of parameters	
	auto check_param = param_map.find(vect[0]);
	if(param_vect.size() != check_param->second) {
		string err = "ERROR: FACT " + vect[0] + " requires "
				+ to_string(check_param->second) + " parameters\n";
		throw err;
	}

	//access current relation in relation_map
	auto relation_iter = fact_map.find(vect[0]);

	//insert the param_vect into the set of facts
	relation_iter->second.insert(param_vect);
}

//delete a specified relation from fact_map
void KnowledgeBase::remove(string fact_name) {
	if( check_exist(fact_name) ) {
		auto relation_iter = fact_map.find(fact_name);
		fact_map.erase( relation_iter );
	}
}

//check if a fact exists
bool KnowledgeBase::check_exist(string fact_name) {
	auto relation_iter = fact_map.find(fact_name);
	if( relation_iter == fact_map.end() ) {
		return false;;
	}
	return true;
}

//pass in a map and store results of query into it
void KnowledgeBase::query(set<vector<string>>& results, vector<string> vect) {
	string relation = vect[0];
	string param_str = vect[1];

	//parse parameters
	vector<string> param_vect;
	while( !param_str.empty() ) {
		param_vect.push_back( parse(param_str, ",") );
	}

	//check if correct number of parameters
	auto check_param = param_map.find(vect[0]);
	if(param_vect.size() != check_param->second) {
		cout << "ERROR: FACT " << vect[0] << " requires "
				<< check_param->second << " parameters\n";
		return;
	}

	//determine which parameters are specific
	vector<bool> specific_param;
	for(int i=0; i<param_vect.size(); i++) {
		specific_param.push_back(param_vect[i][0] != '$');
	}

	//check if relation exists in fact_map
	auto relation_iter = fact_map.find(relation);
	if( relation_iter == fact_map.end() ) {
		return;
	}

	//for each specific paramater, check if the parameter exists in the set of
	//	facts. if it does not, delete the fact vector
	results = relation_iter->second;
	for(int i=0; i<param_vect.size(); i++) {
		if(specific_param[i]) {
			for(auto j=results.begin(); j!=results.end();) {
				if( (*j)[i].compare(param_vect[i])!=0 ) {
					auto k = *j;
					j++;
					results.erase(k);
				}
				else {
					j++;
				}
			}
		}
	}
}

//iterate through the maps and print out the values in the format of facts
void KnowledgeBase::print_file(ostream& outfile) {

	//i=relationship iterator
	for(auto i=fact_map.begin(); i!=fact_map.end(); i++) {

		//j=set of facts iterator
		for(auto j=i->second.begin(); j!=i->second.end(); j++) {
			auto k=(*j).begin();
			outfile << "FACT " << i->first << "(" << *k;
			k++;
			for(; k!=(*j).end(); k++) {
				outfile << "," << *k;
			}
			outfile << ")\n";
		}
	}
}










//-------------------------------RuleBase Class-------------------------------
//------------------------------RuleBase:Private------------------------------
//-------------------------------RuleBase:Public-------------------------------

//constructor for RuleBase object
RuleBase::RuleBase() {}

//destructor for RuleBase object
RuleBase::~RuleBase() {}

//inserts a new rule into the rule_map
void RuleBase::insert(vector<string> vect) {

	//parse base parameters
	string base_param_str = vect[1];
	vector<string> base_param_vect;
	while( !base_param_str.empty() ) {
		base_param_vect.push_back( parse(base_param_str, ",") );
	}

	//check for duplicates or invalid parameters
	auto value_iter = rule_map.equal_range(vect[0]);
	for(auto i=value_iter.first; i!=value_iter.second; i++) {
		if(i->second == vect) {
			return;
		}

		//parse the current rule's base parameters
		string temp_param_str = i->second[1];
		vector<string> temp_param_vect;
		while( !temp_param_str.empty() ) {
			temp_param_vect.push_back( parse(temp_param_str, ",") );
		}

		//compare the two base parameters
		if(temp_param_vect.size() != base_param_vect.size()) {
			string err = "ERROR: cannot insert RULE\nalready existing rule "
					"requires "+to_string(i->second[1].size())+" parameters\n";
			throw err;
		}
	}

	//insert the rule into rule_map if it is not a duplicate
	rule_map.insert( make_pair(vect[0], vect) );
}

//delete a specified rule
void RuleBase::remove(string rule_name) {
	auto rule = rule_map.equal_range(rule_name);
	rule_map.erase(rule.first, rule.second);
}

//check if a specified rule exists
bool RuleBase::check_exist(string rule_name) {
	auto search_result = rule_map.find(rule_name);
	if( search_result == rule_map.end() ) {
		return false;;
	}
	return true;
}

//print all contents of RuleBase into given file
void RuleBase::print_file(ostream& outfile) {
	for(auto i=rule_map.begin(); i!=rule_map.end(); ++i) {
		outfile << "RULE " << i->second[0] << "(" << i->second[1] << "):- "
				<< i->second[2] << " " << i->second[3] << "\n";
	}
}

//Pulls rules from the RuleBase
multimap<string, vector<string>> RuleBase::get(string relation) {
	multimap<string, vector<string>> rule; //will be returned
	//iterator of all values for a specified key
	auto value_iter = rule_map.equal_range(relation);
	//loop through all values and insert into 'rule'
	for(auto i = value_iter.first; i != value_iter.second; i++) {
		rule.insert( make_pair(relation, i->second) );
	}
	return rule;
}










//------------------------------Public Functions------------------------------

//parses a string, returns the substring before the delimiter, and deletes the
//	parsed section of the string
string parse(string& line, string delimiter) {
	size_t position = line.find(delimiter); //position of delimiter in "line"
	//substring = all char before delimiter
	string substr = line.substr(0, position);
	//delete "line" past delimiter
	line.erase( 0, position + delimiter.length() );
	//if delimiter was not found, substring = string, and delete entire string
	if(position == string::npos) {
		string substr = line;
		line.erase( 0, line.length() );
	}
	return substr;
}

//checks if the inputted command is valid
bool invalid_command(vector<string> vect) {
	for(int i=0; i<vect.size(); i++) {
		if( vect[i].compare("")==0 ) {
			return true;
		}
	}
	return false;
}

//create all combinations of valid parameters
void make_combo(int i, vector<string> combo_vect,
		vector<set<string>> valid_results_vect,
		vector<vector<string>>& all_combo_vect) {
	for(auto a=valid_results_vect[i].begin();
			a!=valid_results_vect[i].end(); a++) {
		auto temp_vect = combo_vect;
		temp_vect.push_back(*a);
		if((i+1)==valid_results_vect.size()) {
			all_combo_vect.push_back(temp_vect);
		}
		else {
			make_combo(i+1, temp_vect, valid_results_vect, all_combo_vect);
		}
	}
}

//checks if current combination exists is valid
bool valid_combo(vector<string> combo_vect, vector<string> param_order_vect,
		vector<set<vector<string>>> all_results_vect,
		multimap<string,pair<int,int>> coords_mmap) {
	for(int a=0; a<combo_vect.size(); a++) {
		auto b = coords_mmap.equal_range(param_order_vect[a]);
		for(auto c=b.first; c!=b.second; c++) {
			if(c->second.first > 0) {
				for(auto d=all_results_vect[c->second.first-1].begin();
						d!=all_results_vect[c->second.first-1].end();) {
					if( (*d)[c->second.second-1].compare(combo_vect[a]) !=0 ) {
						auto e = d;
						d++;
						all_results_vect[c->second.first-1].erase(e);
					}
					else {
						d++;
					}
				}
				auto d=all_results_vect[c->second.first-1].begin();
				if( d==all_results_vect[c->second.first-1].end() ) {
					return false;
				}
			}
		}
	}
	return true;
}

void thread_print_constr(thread::id t_id) {
	mtx.lock();
	t_num++;
	if(t_num > 1) {
		cout << "Thread #" << t_id << " started.\n";
	}
	mtx.unlock();
}

void thread_print_destr(thread::id t_id) {
	mtx.lock();
	if(t_num > 1) {
		cout << "Thread #" << t_id << " completed.\n";
	}
	t_num--;
	mtx.unlock();
}










//--------------------------------Main Function--------------------------------

int main() {
	SRI sri_main;
	string command;	
	do {
		cout << "\nEnter Command: ";
		getline(cin, command);
		try {
			sri_main.scan(command);
		}
		catch(string err) {
			cerr << err;
		}
	}
	while( command.compare("EXIT")!=0 );
	return 0;
}
