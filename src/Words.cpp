#include "Words.h"
#include <iomanip>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <random>

Wordlist::Wordlist(){
	for(std::pair<std::string, double> pair : LETTER_FREQUENCIES){
		frequencies[std::tolower(std::get<0>(pair)[0])] = std::get<1>(pair);
	}
}

std::string Wordlist::dispProgress(int at, int total, std::string message, std::string style, int metering, int increment, bool silent){
	if(metering != -1){
		// We don't use I/O functions as frequently if metering is on.
		if(at % metering != 0 && at != total) return "";
	}
	double percent = double(at) / double(total) * 100.0;
	std::stringstream text;
	text << std::fixed << std::setprecision(2);
	if(style == "percent"){
		text << "\r" << message.c_str() << "... " << percent << "%";
	} else if(style == "progress"){
		text << "\r" << message << ": " << at << "/" << total << " = [";
		for(int i = increment; i <= 100; i += increment){
			if(i > percent) text << "-";
			else text << "=";
			if((i + increment) <= 100) text << " ";
		}
		text << "] " << percent << "%";
	}
	std::string finalText = text.str();
	if(!silent){
		std::cout << finalText;
		std::cout.flush();
	}
	if(at == total){
		std::cout << std::endl;
	}
	return finalText.substr(1);
}

// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}

bool Wordlist::readWordlist(std::string filename){
	/*
	wordlist = [x.strip().lower() for x in fp if '\'' not in x and len(x.strip()) > 2] # skip contractions and empty lines
	*/
	try {
		std::ifstream ifp(filename.c_str());
		if(!ifp.is_open()) throw "Could not open wordlist at " + filename + "!";
		std::string tmp;
		while(ifp >> tmp){
			// Skip contractions and empty lines.
			tmp = trim(tmp);
			if(!tmp.length()) continue;
			if(tmp.find('\'') != std::string::npos) continue;
			if(tmp.length() < MIN_LETTERS) continue;
			
			// Transform to lowercase.
			std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
			
			// Finally, save the word.
			words.push_back(tmp);
		}
	} catch(std::string e){
		std::cout << e << std::endl;
		return false;
	}
	return true;
}

inline bool Wordlist::isVowel(char ch){
	ch = (std::islower(ch) ? ch : std::tolower(ch));
	return (ch == 'a') || (ch == 'e') || (ch == 'i') || (ch == 'o') || (ch == 'u');
}

void Wordlist::initLevels(void){
	int diff = (int)words.size() / NUM_LEVELS;
	for(int i = 0; i < NUM_LEVELS; i++){
		levelIndices[i] = diff * i;
	}
	levelIndices[NUM_LEVELS] = (int)words.size();
}

void Wordlist::scoreWords(void){
	// Assign a score to each word.
	double score;
	unsigned int length, totalSize = words.size();
	for(unsigned int c = 0; c < totalSize; c++){
		std::string& word = words[c];
		
		// First, calculate the score based on the mean letter frequency and
		// adjust if no vowels.
		score = 0.0;
		length = word.length();
		bool hasVowels = false;
		for(unsigned int i = 0; i < length; i++){
			char on = word[i];
			hasVowels |= isVowel(on);
			score += frequencies[on];
		}
		score /= (double)length;
		
		// Adjust if no vowels.
		if(!hasVowels){
			score *= 0.75; // lower = harder
		}
		
		// Then, account for length (lower length implies a harder word).
		// Scaling Function: f(x) = 0.5x(x + 1)
		score *= 0.5 * length * (length + 1);
		score = std::max(0.0, score);
		
		// And save the score as well as displaying our progress.
		scores[word] = score;
		dispProgress(int(c + 1), int(totalSize), /*message=*/"Scoring words", /*style=*/"percent", /*metering=*/100);
	}
	
	// Sort words based on their score. //
	// Alert user to operation.
	std::cout << "Sorting words... ";
	std::cout.flush();
	
	// Copy over the map as pairs of values into a vector and sort that vector by value.
	std::vector<std::pair<std::string, double> > items;
	items.clear();
	std::copy(scores.begin(), scores.end(), std::back_inserter(items));
	std::sort(items.begin(), items.end(), [](std::pair<std::string, double>& a, std::pair<std::string, double>& b){
		return std::get<1>(a) > std::get<1>(b);
	});
	
	// Copy back into original words vector, overwriting.
	words.clear();
	for(unsigned int i = 0, e = items.size(); i < e; i++){
		std::string& word = std::get<0>(items[i]);
		words.push_back(word);
	}
	
	// Update user.
	std::cout << "done." << std::endl;
	std::cout.flush();
	/*
	for(unsigned int i = 0; i < 20; i++){
		printf("(%s, %f) ", words[i].c_str(), scores[words[i]]);
	}
	std::cout << std::endl;
	*/
	
	// Initialize level data.
	initLevels();
}

std::string Wordlist::getWordAtLevel(unsigned int level){
	/*
	start_ind, end_ind = word_stats["levels"][level - 1], word_stats["levels"][level]
	rand_ind = random.randrange(start_ind, end_ind)
	return sorted_words[rand_ind]
	*/
	int start_ind = levelIndices[level - 1], end_ind = levelIndices[level];
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_int_distribution<int> randGenerator(start_ind, end_ind);
	auto rand_ind = randGenerator(rng);
	return words[rand_ind];
}












