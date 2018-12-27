#include "Game.h"
#include <cassert>
#include <chrono>

Game::Game(airplay_device& c) : conn(c){
	// Initialize wordlist.
	list = Wordlist();
	if(!list.readWordlist(WORDLIST_PATH)){
		std::cerr << "Error: Could not open wordlist." << std::endl;
		std::exit(1);
	}
	list.scoreWords();

	// Initialize background image.
	FILE* in = fopen("background.png", "rb");
	background = gdImageCreateFromPng(in);
	fclose(in);
}

inline int getColor(gdImagePtr& img, int a, int b, int c){
	return gdImageColorResolve(img, a, b, c);
}

std::vector<char> Game::getIncorrectGuesses(){
	gameMutex.lock();
	std::vector<char> ret;
	if(mode != MODE_COMPUTER_GUESSES_WORD){
		for(char ch : guessed){
			if(word.find(ch) == std::string::npos) ret.push_back(ch);
		}
	} else {
		for(char ch : guessed){
			if(!guessValidity[ch]) ret.push_back(ch);
		}
	}
	gameMutex.unlock();
	return ret;
}

unsigned int Game::getIncorrectGuessesNum(){
	gameMutex.lock();
	unsigned int ret = 0;
	if(mode != MODE_COMPUTER_GUESSES_WORD){
		for(char ch : guessed){
			if(word.find(ch) == std::string::npos) ++ret;
		}
	} else {
		for(char ch : guessed){
			if(!guessValidity[ch]) ++ret;
		}
	}
	gameMutex.unlock();
	return ret;
}

std::string Game::getBlankedWord(){
	gameMutex.lock();
	std::string blankedWord = "";
	for(unsigned int i = 0; i < word.length(); i++){
		char on = word[i];
		if(std::find(guessed.begin(), guessed.end(), on) == guessed.end()){
			on = '_';
		}
		if(blankedWord.length()) blankedWord.push_back(' ');
		blankedWord.push_back(on);
	}
	gameMutex.unlock();
	return blankedWord;
}

std::string Game::getCurrentGameImage(bool result_screen){
	// Initialize constants and variables.
	int brect[8], xPos, yPos, diff;
	double size;
	char* err;
	const int width = 1920, height = 1080; // 1920 x 1080
	char* font_times = const_cast<char*>("fonts/times.ttf");

	// Initialize the image from the background image.
	gdImagePtr im = gdImageCreateTrueColor(background->sx, background->sy);
	gdImageCopy(im, background, 0, 0, 0, 0, im->sx, im->sy);

	// Initialize selected colors.
	int red = getColor(im, 255, 0, 0);
	int green = getColor(im, 0, 255, 0);
	int blue = getColor(im, 0, 0, 255);
	int blue_144_color = getColor(im, 144, 144, 144);
	int keyboard_color = getColor(im, 137, 138, 99);
	int rank_color = getColor(im, 65, 145, 75);
	int cross_color = getColor(im, 100, 90, 80);

	// Write the score at the bottom-left corner of the screen. //
	if(mode == MODE_COMPUTER_PICKS_WORD){
		std::stringstream score_text;
		score_text << "Score: " << score;
		xPos = 75;
		yPos = 70;
		err = gdImageStringFT(im, &brect[0], cross_color, font_times, 40.0, 0.0, xPos, yPos, const_cast<char*>(score_text.str().c_str()));
		if(err){
			std::cerr << err << std::endl;
			return "";
		}
	}

	// Write the level of the game and the total number of levels. //
	if(mode == MODE_COMPUTER_PICKS_WORD){
		std::stringstream level_text;
		level_text << "Level " << level << "/" << NUM_LEVELS;
		xPos = 1630;
		yPos = 70;
		err = gdImageStringFT(im, &brect[0], blue, font_times, 40.0, 0.0, xPos, yPos, const_cast<char*>(level_text.str().c_str()));
		if(err){
			std::cerr << err << std::endl;
			return "";
		}
	}

	if(result_screen && !waitingForWord){
		// Write the word across the screen, optimizing the size. //
		// Get the bounding rectangle and optimize the size and position based on that.
		size = 40.0;
		const int BASE_WORD_HEIGHT = 400;
		const int DIFF_MAX = 20;
		const int MAX_Y_REACH = BASE_WORD_HEIGHT + 180;
		const double GRANULARITY = 0.3;
		diff = DIFF_MAX + 1;
		xPos = 75;
		yPos = BASE_WORD_HEIGHT;
		std::string word_text = "The word was: " + word + ".";
		while(diff > DIFF_MAX){
			err = gdImageStringFT(NULL, &brect[0], blue_144_color, font_times, size, 0.0, xPos, yPos, const_cast<char*>(word_text.c_str()));
			if(err){
				std::cerr << err << std::endl;
				return "";
			}
			// 750 is lower y-line.
			diff = width - brect[2];
			if(brect[3] > MAX_Y_REACH){
				yPos += (MAX_Y_REACH - 100) - brect[3];
				break;
			} else if(diff > DIFF_MAX){
				size += GRANULARITY;
			} else {
				assert(diff <= DIFF_MAX);
				yPos += (MAX_Y_REACH - 100) - brect[3];
			}
		}

		// Write the word with the optimized size and/or position.
		gdImageStringFT(im, &brect[0], (lastGameResult == 1 ? green : red), font_times, size, 0.0, xPos, yPos, const_cast<char*>(word_text.substr(0, 14).c_str()));
		gdImageStringFT(im, &brect[0], blue_144_color, font_times, size, 0.0, brect[2], yPos, const_cast<char*>(word_text.substr(14).c_str()));

		// Show if the user levelled up or down, if applicable. //
		if(levelDiff != 0){
			// Generate the level up/down text.
			std::string level_text = (abs(levelDiff) == 1 ? "level" : "levels");
			int color = (lastGameResult == 1 ? green : red);
			std::stringstream text;
			if(lastGameResult == 1){
				text << "You win! ";
			} else {
				text << "You lose! ";
			}
			if(levelDiff > 0){
				text << "Congratulations! You moved up " << levelDiff << " " << level_text << "!";
			} else {
				text << "You moved down " << abs(levelDiff) << " " << level_text << ".";
			}
			level_text = text.str();

			// Optimize the size of it.
			xPos = 75;
			yPos = BASE_WORD_HEIGHT + 220;
			brect[2] = width + 1;
			size = 40.0;
			while(brect[2] > width){
				err = gdImageStringFT(NULL, &brect[0], blue_144_color, font_times, size, 0.0, xPos, yPos, const_cast<char*>(level_text.c_str()));
				if(err){
					std::cerr << err << std::endl;
					return "";
				}
				if(brect[2] > width) size -= GRANULARITY;
			}

			// Write it with the optimized size.
			gdImageStringFT(im, &brect[0], color, font_times, size, 0.0, xPos, yPos, const_cast<char*>(level_text.c_str()));
		}

		// Write the definition of the word. //
		// TODO
	} else if(!result_screen && !waitingForWord){
		// Illustrate the guessing progress with a progress bar and the image for the corresponding stage. //
		// Write the progress bar.
		int leftX = 298, leftY = 305;
		const int GUESS_RECT_WIDTH = 50, GUESS_RECT_HEIGHT = 50;
		const auto incorrect_guesses = getIncorrectGuessesNum();
		for(unsigned int i = 0; i < GUESS_LIMIT; i++){
			gdPointPtr pts = new gdPoint[4];
			pts[0].x = leftX; pts[0].y = leftY;
			pts[1].x = leftX + GUESS_RECT_WIDTH; pts[1].y = leftY;
			pts[2].x = leftX + GUESS_RECT_WIDTH; pts[2].y = leftY + GUESS_RECT_HEIGHT;
			pts[3].x = leftX; pts[3].y = leftY + GUESS_RECT_HEIGHT;
			leftX += GUESS_RECT_WIDTH * 1.5;
			gdImageFilledPolygon(im, pts, 4, (i < incorrect_guesses ? red : rank_color));
		}

		// And write the guessing stage as an image (a.k.a. the actual hangman).
		std::stringstream stage_file;
		stage_file << "data/stage" << (getIncorrectGuessesNum() + 1) << ".png";
		FILE* in = fopen(stage_file.str().c_str(), "rb");
		if(!in){
			std::cerr << "Could not open '" << stage_file.str() << "' for reading hangman stage." << std::endl;
			return "";
		}
		gdImagePtr stage_img = gdImageCreateFromPng(in);
		fclose(in);
		gdImageCopy(im, stage_img, 1375, 100, 0, 0, stage_img->sx, stage_img->sy);

		// Write the word down as blanks (underscores), substituting the actual letter where
		// guessed correctly, and optimize the size using the bounding rectangle.
		// Generate the "blanked" word.
		std::string blankedWord = getBlankedWord();

		// Get the bounding rectangle and optimize the size and position based on that.
		const int MIN_Y_REACH = brect[3] + 10;
		size = 40.0;
		const int DIFF_MAX = 20;
		const int MAX_Y_REACH = 720;
		const double GRANULARITY = 0.3;
		diff = DIFF_MAX + 1;
		xPos = 75;
		yPos = 540;
		while(diff > DIFF_MAX){
			err = gdImageStringFT(NULL, &brect[0], blue_144_color, font_times, size, 0.0, xPos, yPos, const_cast<char*>(blankedWord.c_str()));
			if(err){
				std::cerr << err << std::endl;
				return "";
			}
			// 750 is lower y-line.
			diff = width - brect[2];
			if(brect[5] < MIN_Y_REACH){
				size -= 1.75 * GRANULARITY;
				break;
			} else if(brect[3] > MAX_Y_REACH){
				yPos += (MAX_Y_REACH - 100) - brect[3];
				break;
			} else if(diff > DIFF_MAX){
				size += GRANULARITY;
			} else {
				assert(diff <= DIFF_MAX);
				yPos += (MAX_Y_REACH - 100) - brect[3];
				yPos = std::max(yPos, MIN_Y_REACH);
			}
		}

		// Write the blanked word with the optimized size and/or position.
		gdImageStringFT(im, &brect[0], blue_144_color, font_times, size, 0.0, xPos, yPos, const_cast<char*>(blankedWord.c_str()));

		// Draw a "keyboard" and mark all letters that have already been guessed, and whether
		// it was a correct/incorrect guess.
		xPos = 45;
		yPos = 750;
		char ch = 'A';
		while(ch <= 'Z' && xPos < width && yPos < height){
			// Figure out the right color to draw the letter in and draw a rectangle/cross
			// if necessary through the letter.
			int color = keyboard_color;
			bool should_cross = false;
			bool correct = false;
			if(std::find(guessed.begin(), guessed.end(), std::tolower(ch)) != guessed.end()){
				should_cross = true;
				if(word.find(std::tolower(ch)) != std::string::npos){
					correct = true;
					color = green;
				} else {
					color = red;
				}
			}

			// Write the letter.
			std::string letter;
			letter.push_back(ch);
			//printf("|(%s) - (%c)| at (%d, %d)\n", letter.c_str(), ch, xPos, yPos);
			err = gdImageStringFT(im, &brect[0], color, font_times, 40.0, 0.0, xPos, yPos, const_cast<char*>(letter.c_str()));
			if(err){
				std::cerr << err << std::endl;
				return "";
			}

			// Cross the letter out, if applicable.
			if(should_cross){
				gdImageSetThickness(im, 4);
				int offset = 5;
				brect[0] -= offset;
				brect[1] += offset;
				brect[2] += offset;
				brect[3] += offset;
				brect[4] += offset;
				brect[5] -= offset;
				brect[6] -= offset;
				brect[7] -= offset;
				gdImageRectangle(im, brect[6], brect[7], brect[2], brect[3], cross_color);
				gdImageLine(im, brect[0], brect[1], brect[4], brect[5], cross_color);
				gdImageLine(im, brect[2], brect[3], brect[6], brect[7], cross_color);
			}

			// Compute the next position.
			xPos += 200;
			if(xPos >= 1900){
				xPos = 45;
				yPos += 100;
			}
			++ch;
		}

		// Write the word rank, optimizing for space (right-aligning) using the bounding rectangle. //
		if(mode == MODE_COMPUTER_PICKS_WORD){
			// Generate word rank text.
			std::stringstream difficulty_text;
			auto& words = list.getSortedWords();
			difficulty_text << "Word Rank: #" << std::distance(words.begin(), std::find(words.begin(), words.end(), word));
			difficulty_text << "/" << words.size();

			// Optimize text position using the bounding rectangle.
			xPos = 1250;
			yPos = 1050;
			err = gdImageStringFT(NULL, &brect[0], rank_color, font_times, 40.0, 0.0, xPos, yPos, const_cast<char*>(difficulty_text.str().c_str()));
			if(err){
				std::cerr << err << std::endl;
				return "";
			}

			// Write the word rank text in the optimized position.
			diff = (width - 20) - brect[2];
			xPos += diff;
			gdImageStringFT(im, &brect[0], rank_color, font_times, 40.0, 0.0, xPos, yPos, const_cast<char*>(difficulty_text.str().c_str()));
		}
	} else if(waitingForWord){
		xPos = 75;
		yPos = 400;
		size = 72.0;
		err = gdImageStringFT(im, &brect[0], rank_color, font_times, size, 0.0, xPos, yPos, (char*)"Waiting for user to choose a word...");
		if(err){
			std::cerr << err << std::endl;
			return "";
		}
	}

	// Get JPEG text.
	int jpegSize;
	char* jpeg_data = (char*)gdImageJpegPtr(im, &jpegSize, 100);
	std::string ret;
	for(int i = 0; i < jpegSize; i++){
		ret.push_back(jpeg_data[i]);
	}
	gdFree(jpeg_data);

	// Clean up.
	gdImageDestroy(im);
	return ret;
}

void Game::showPicture(std::string& data){
	// AirplayImage* img = new AirplayImage();
	// img->size = data.length();
	// img->data = (void*)data.c_str();
	conn.send_message(MessageType::ShowPicture, data);
	// delete img;
}

int Game::computeScoreChange(bool won, unsigned int level){
	// Formula used:
	// w(x) = 1 if won, else -1
	// score(level) = w(x)*(level)^(3/2)
	// or: score(level) = -NUM_LEVELS*(1/level)^(3/2)
	double lvl = (double)level;
	if(won) return (int)std::sqrt(lvl * lvl * lvl);
	else return (int)((-2)*NUM_LEVELS*std::sqrt(1.0/(lvl*lvl)));
}

int Game::guessLetter(char letter){
	gameMutex.lock();
	guessed.push_back(letter);
	int ret = 0;
	for(unsigned long int i = 0; i < word.length(); i++){
		if(word[i] == letter) ++ret;
	}
	gameMutex.unlock();
	return ret;
}

std::string Game::chooseWord(std::string new_word){
	// Note: Level and other such variables are not filled in on purpose.
	level = 1e9; // signifies that level is N/A in this mode
	if(new_word.length() < MIN_LETTERS) return "Word too short!";
	auto& words = list.getSortedWords();
	std::transform(new_word.begin(), new_word.end(), new_word.begin(), ::tolower);
	// if(std::find(words.begin(), words.end(), new_word) == words.end()) return "Word not in dictionary!";
	gameMutex.lock();
	this->word = new_word;
	this->waitingForWord = false;
	gameMutex.unlock();
	return "";
}

std::string Game::chooseLength(int length){
	// Note: Level and other such variables are not filled in on purpose.
	level = 1e9; // signifies that level is N/A in this mode
	if(length < 1) return "Length too short!";
	auto& words = list.getSortedWords();
	bool works = false;
	for(std::string& word : words){
		if(word.length() == (unsigned)length){
			works = true;
			break;
		}
	}
	if(!works) return "No word with specified length in dictionary!";
	gameMutex.lock();
	this->wordLength = (unsigned int)length;
	gameMutex.unlock();
	return "";
}

std::string Game::saveGuessResult(std::string result){
	if(result == "yes" || result == "no"){
		bool suc = (result == "yes");
		gameMutex.lock();
		if(std::find(guessed.begin(), guessed.end(), lastComputerGuess) != guessed.end()){
			gameMutex.unlock();
			return "Someone already confirmed/denied if that letter was in the word!";
		}
		guessed.push_back(lastComputerGuess);
		guessValidity[lastComputerGuess] = suc;
		if(suc){ // if yes, ask where the letters are in it
			std::stringstream fmt;
			fmt << "%prompt(Letter's Location in Word, /setWordLocations, word, Where is the letter ";
			fmt << (char)toupper(lastComputerGuess) << " in the word? Put a dash where appropriate., wordFill)";
			alert = fmt.str();
		} else {
			alert = "";
			lastComputerGuess = '\0'; // letter incorrect - do next guess
		}
		gameMutex.unlock();
		return "";
	} else return "Invalid option sent by browser!";
}

std::string Game::saveWordLocations(std::string str){
	gameMutex.lock();
	const std::string word = this->word;
	gameMutex.unlock();
	if(word.length() != str.length()){
		return "Word is wrong length!";
	}
	for(unsigned int i = 0; i < str.length(); i++){
		if(word[i] == '_' && str[i] != '-'){
			if(str[i] != lastComputerGuess){
				return "A different letter than the one guessed by the computer was provided!";
			}
		} else if(word[i] == '_' && str[i] == '-'){
			str[i] = '_';
		} else if(word[i] != '_' && str[i] != word[i]){
			return "The word was modified in an unauthorized fashion!";
		}
	}
	gameMutex.lock();
	this->word = str;
	lastComputerGuess = '\0';
	gameMutex.unlock();
	return "";
}

std::string Game::getWordHTMLForm(){
	gameMutex.lock();
	const std::string str = this->word;
	gameMutex.unlock();
	/*
	<div class="input-group">
		<input class="form-control" type="text" maxlength="1" placeholder="a">
		<span class="input-group-addon">mb</span>
		<input class="form-control" type="text" maxlength="2" placeholder="er">
		<span class="input-group-addon">is</span>
		<input class="form-control" type="text" maxlength="1" placeholder="h">
	</div>
	*/
	std::stringstream ss;
	ss << "<div class=\"input-group\">";
	std::string s = "";
	unsigned int c = 0;
	for(unsigned int i = 0; i <= str.length(); i++){
		char on = (i < str.length() ? str[i] : '\0');
		if(on == '\0'){
			if(c > 0) on = 'a';
			else if(s.length() > 0UL) on = '_';
		}
		if(on != '_'){
			if(c > 0){
				ss << "\t<input class=\"form-control\" type=\"text\" maxlength=\"" << c << "\" value=\"" << std::string((size_t)c, '-') << "\">";
			}
			c = 0;
			s.push_back(on);
		} else {
			if(s.length() > 0UL){
				ss << "\t<span class=\"input-group-addon\">" << s << "</span>";
				s = "";
			}
			++c;
		}
	}
	ss << "</div>";
	return ss.str();
}

char Game::nextLetterToGuess(){
	/*
	* Guessing Algorithm:
	* 1. Generate a subset of the wordlist that matches the recorded constraints.
	*	- e.g. Must not contain any letters of incorrect guesses, the "blanked word"
	*	       must match, etc.
	* 2. Find the probability of each letter that appears in each blank.
	*	- e.g. "a_b_c" with two words "axby" and "axbc", the probability for blank #1
	*		   of "x" is 100%, and for blank #2, the probability is 50-50 b/w "y" and "c".
	* 3. Find the letter that has the highest probability for any blank and guess it.
	*	- e.g. In above example, we would guess "x" with a probability of 100% of being
	*		   on the blank, versus taking a 50-50 chance with "y" or "c".
	*/
	// Generate the subset of the word list. //
	bool guessedLetter[26]; // [letter #] --> [guessed or not]
	for(char c = 'a'; c <= 'z'; c++){
		int ind = (int)(c - 'a');
		if(std::find(guessed.begin(), guessed.end(), c) != guessed.end()){
			guessedLetter[ind] = true;
		} else {
			guessedLetter[ind] = false;
		}
	}
	auto& words = list.getSortedWords();
	auto orig = this->word;
	std::vector<std::reference_wrapper<std::string> > wordSubset;
	for(std::string& word : words){
		if(word.length() != wordLength) continue; // the words are not the same length
		bool works = true;
		for(unsigned int i = 0; works && i < word.length(); i++){
			char on = word[i];
			int ind = (int)(on - 'a');
			if(orig[i] != '_' && orig[i] != on) works = false; // the words thus far don't match up
			if(guessedLetter[ind]){
				if(!guessValidity[on]) works = false; // we guessed this letter and it was not there
				if(guessValidity[on] && orig[i] == '_') works = false; // we guessed this letter, and it was in the word but not here
			}
		}
		if(!works) continue;
		wordSubset.push_back(word);
	}

	// Count the occurences of each letter in each blank using the subset of possible words. //
	std::map<unsigned int, std::map<char, unsigned long long> > blank_counts;
	for(std::string& word : wordSubset){
		for(unsigned int i = 0; i < word.length(); i++){
			if(orig[i] == '_'){
				char on = word[i];
				auto& map = blank_counts[i];
				if(map.find(on) == map.end()){
					map[on] = 1ULL;
				} else {
					++map[on];
				}
			}
		}
	}

	// Compute the probability, by blank, of each letter appearing in the blank. //
	std::map<unsigned int, std::map<char, double> > probabilities;
	for(auto& pair : blank_counts){
		unsigned long long total = 0;
		for(auto& on : std::get<1>(pair)){
			total += std::get<1>(on);
		}
		std::cerr << "Blank #" << std::get<0>(pair) << ":\n";
		for(auto& on : std::get<1>(pair)){
			double prob = 100.0f * (double)std::get<1>(on) / (double)total;
			probabilities[std::get<0>(pair)][std::get<0>(on)] = prob;
			std::cerr << "\t" << std::get<0>(on) << ": " << prob << "%" << std::endl;
		}
	}

	// Find the letter that has the highest probability for any blank and guess it.
	char guessingLetter;
	double maxProb;
	for(auto& pair: probabilities){
		for(auto& on : std::get<1>(pair)){
			double prob = std::get<1>(on);
			if(prob > maxProb){
				maxProb = prob;
				guessingLetter = std::get<0>(on);
			}
		}
	}
	assert(std::find(guessed.begin(), guessed.end(), guessingLetter) == guessed.end());
	return guessingLetter;
}

void Game::start_game(unsigned int theLevel, GameMode theMode){
	/*
	* Modes:
	* 1. Computer picks a word at a specified level, and user guesses.
	* 	- Extension: Collaborative guessing, word suggestions, over wireless/HTTP server - CHECK.
	* 	- Extension: Every time the user guesses something correctly, the computer tries to alter the word
	* 	  instead and find another possibility that fits with the restrictions (length, guessed, etc.)
	* 	  imposed so far, making it much, much harder - TODO.
	* 2. User thinks of a word at a specified level, and computer guesses.
	* 3. User thinks of a word while other users try to guess it, and computer is an arbiter and a screen.
	*/

	// Instantiate necessary variables.
	this->level = theLevel;
	this->mode = theMode;

	while(true){ // everything is in a loop so the mode and level can be changed on-demand
		// Handle current mode. //
		while(mode == MODE_COMPUTER_PICKS_WORD){
			// Reset everything.
			levelDiff = 0;
			gameMutex.lock();
			word = ""; // clear word
			alert = ""; // clear alerts
			guessed.clear(); // clear guessed letters
			lastGameResult = -1; // set game to ongoing state
			gameMutex.unlock();

			// Mark the time. //
			auto now = std::chrono::system_clock::now();
			auto duration = now.time_since_epoch();
			auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

			// Pick a word at the specified level.
			gameMutex.lock();
			word = list.getWordAtLevel(level);
			printf("Chosen word: %s (%lu letters) at level %u.\n", word.c_str(), word.length(), level);
			gameMutex.unlock();

			// Loop, updating the game image each time.
			while(getIncorrectGuessesNum() < GUESS_LIMIT && getBlankedWord().find('_') != std::string::npos){
				// Generate the current game image and display it.
				std::string img = getCurrentGameImage();
				std::ofstream ofp("out.jpeg", std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
				ofp.write(img.c_str(), img.length());
				ofp.close();
				showPicture(img);

				// Sleep so as not to create a busy loop. //
				sleep(1);
			}

			// TODO: Bonus based on time.
			// Mark win/loss.
			if(getIncorrectGuessesNum() >= GUESS_LIMIT){
				// Loss.
				std::cout << "YOU LOSE!" << std::endl;
				lastGameResult = 0;
			} else if(getBlankedWord().find('_') == std::string::npos){
				// Win.
				std::cout << "YOU WIN!" << std::endl;
				alert = "You win! The word was '" + word + "'.";
				lastGameResult = 1;
			}

			// Compute and apply score difference.
			int score_diff = computeScoreChange((bool)lastGameResult, level);
			score += score_diff;

			// Compute and apply level difference.
			levelDiff = 0;
			if(lastGameResult == 0){
				// Loss.
				if(level > 1) levelDiff = -1;
			} else if(lastGameResult == 1){
				// Win.
				if(level < NUM_LEVELS) levelDiff = 1;
			}
			level = (unsigned int)((int)level + levelDiff);

			// Broadcast win/loss.
			if(lastGameResult == 0){
				alert = "You lost! ";
				if(levelDiff < 0) alert += "Level down! ";
			} else {
				alert = "You win! ";
				if(levelDiff > 0) alert += "Level up! ";
			}
			alert += "The word was '" + word + "'.";

			// Show result screen.
			std::string img = getCurrentGameImage(/*result_screen=*/true);
			showPicture(img);

			// Delay before starting next round.
			std::cerr << "Delaying...\n";
			flashDelay = true;
			sleep(5);
			std::cerr << "Starting next round.\n";
			flashDelay = false;
			++gameInd;
		}
		while(mode == MODE_USER_PICKS_WORD){
			// Reset everything, waiting for the user to pick the word.
			levelDiff = 0;
			score = -1e9; // signifies N/A
			gameMutex.lock();
			word = ""; // clear word
			alert = ""; // clear alerts
			guessed.clear(); // clear guessed letters
			lastGameResult = -1; // set game to ongoing state
			gameMutex.unlock();

			// Mark the time. //
			auto now = std::chrono::system_clock::now();
			auto duration = now.time_since_epoch();
			auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

			// Prompt the user for a word.
			// TODO: Have alerts/server broadcasts have a "timestamp" field, and provide a macro language
			// to embed constructs such as timers counting down to a specified unix time (e.g. for flash delay
			// between rounds).

			gameMutex.lock();
			waitingForWord = true;
			gameMutex.unlock();

			// Loop, updating the game image each time.
			while(waitingForWord || (getIncorrectGuessesNum() < GUESS_LIMIT && getBlankedWord().find('_') != std::string::npos)){
				// Generate the current game image and display it.
				std::string img = getCurrentGameImage();
				std::ofstream ofp("out.jpeg", std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
				ofp.write(img.c_str(), img.length());
				ofp.close();
				showPicture(img);

				// Sleep so as not to create a busy loop. //
				sleep(1);
			}

			// TODO: Flash result on screen for specified amount of time + broadcast to connected devices.
			// TODO: Bonus based on time.
			// Mark win/loss.
			if(getIncorrectGuessesNum() >= GUESS_LIMIT){
				// Loss.
				std::cout << "YOU LOSE!" << std::endl;
				lastGameResult = 0;
			} else if(getBlankedWord().find('_') == std::string::npos){
				// Win.
				std::cout << "YOU WIN!" << std::endl;
				alert = "You win! The word was '" + word + "'.";
				lastGameResult = 1;
			}

			// Broadcast win/loss.
			if(lastGameResult == 0){
				alert = "You lost! ";
			} else {
				alert = "You win! ";
			}
			alert += "The word was '" + word + "'.";

			// Show result screen.
			std::string img = getCurrentGameImage(/*result_screen=*/true);
			showPicture(img);

			// Delay before starting next round.
			std::cerr << "Delaying...\n";
			flashDelay = true;
			sleep(5);
			std::cerr << "Starting next round.\n";
			flashDelay = false;
			++gameInd;
		}
		while(mode == MODE_COMPUTER_GUESSES_WORD){
			// Reset everything, waiting for the user to pick the word.
			levelDiff = 0;
			wordLength = 0U;
			score = -1e9; // signifies N/A
			gameMutex.lock();
			word = ""; // clear word
			alert = ""; // clear alerts
			guessed.clear(); // clear guessed letters
			lastGameResult = -1; // set game to ongoing state
			guessValidity.clear(); // clear guesses
			gameMutex.unlock();

			// Mark the time. //
			auto now = std::chrono::system_clock::now();
			auto duration = now.time_since_epoch();
			auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

			// Prompt the user for the number of letters.
			gameMutex.lock();
			std::cerr << "Random word: " << list.getWordAtLevel(rand() % NUM_LEVELS + 1) << std::endl;
			// %prompt(Title, /url, variable_name_in_url, Label, Type ("text"|"number"))
			alert = "%prompt(Number of letters in word, /setWordLength, length, Length, number)";
			gameMutex.unlock();
			while(wordLength == 0U) usleep(500);
			gameMutex.lock();
			alert = "";
			word = std::string(wordLength, '_');
			gameMutex.unlock();

			// Loop, updating the game image each time.
			/*
			* Guessing Algorithm:
			* 1. Generate a subset of the wordlist that matches the recorded constraints.
			*	- e.g. Must not contain any letters of incorrect guesses, the "blanked word"
			*	       must match, etc.
			* 2. Find the probability of each letter that appears in each blank.
			*	- e.g. "a_b_c" with two words "axby" and "axbc", the probability for blank #1
			*		   of "x" is 100%, and for blank #2, the probability is 50-50 b/w "y" and "c".
			* 3. Find the letter that has the highest probability for any blank and guess it.
			*	- e.g. In above example, we would guess "x" with a probability of 100% of being
			*		   on the blank, versus taking a 50-50 chance with "y" or "c".
			*/
			while(getIncorrectGuessesNum() < GUESS_LIMIT && getBlankedWord().find('_') != std::string::npos){
				// Generate the current game image and display it.
				/*
				// TODO: enable & implement later
				std::string img = getCurrentGameImage();
				std::ofstream ofp("out.jpeg", std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
				ofp.write(img.c_str(), img.length());
				ofp.close();
				showPicture(img);
				*/

				// Decide which letter to guess. //
				char guessingLetter = nextLetterToGuess();

				// Send the letter to the user to affirm/deny. //
				gameMutex.lock();
				lastComputerGuess = guessingLetter;
				std::stringstream fmt;
				fmt << "%prompt(Computer Guesses: " << (char)toupper(lastComputerGuess) << ", /setLetterInWord, in_word, Is ";
				fmt << (char)toupper(lastComputerGuess) << " In Your Word?, choice)";
				alert = fmt.str();
				gameMutex.unlock();

				// Wait for the user's response. //
				while(lastComputerGuess != '\0') usleep(500);
			}

			// Ask the user what the word actually was if we didn't guess it. //
			if(getBlankedWord().find('_') != std::string::npos){
				gameMutex.lock();
				alert = "%prompt(Out of Guesses, /setActualWord, word, What was your word?, text)";
				gameMutex.unlock();
				while(true) usleep(500);
			}

			// TODO: Bonus based on time.
			// Mark win/loss.
			if(getIncorrectGuessesNum() >= GUESS_LIMIT){
				// User wins.
				std::cout << "Computer loses!" << std::endl;
				lastGameResult = 0;
			} else if(getBlankedWord().find('_') == std::string::npos){
				// User loses.
				std::cout << "Computer wins!" << std::endl;
				lastGameResult = 1;
			}

			// Broadcast win/loss.
			if(lastGameResult == 0){
				alert = "Computer lost!";
			} else {
				alert = "Computer won! The word was '" + word + "'.";
			}

			// Show result screen.
			std::string img = getCurrentGameImage(/*result_screen=*/true);
			showPicture(img);

			// Delay before starting next round.
			std::cerr << "Delaying...\n";
			flashDelay = true;
			sleep(5);
			std::cerr << "Starting next round.\n";
			flashDelay = false;
			++gameInd;
		}
	}
}
