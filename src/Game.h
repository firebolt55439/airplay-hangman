#ifndef GAME_INC
#define GAME_INC
#include "Words.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <random>
#include <gd.h>
#include <gdfontg.h>
#include <sys/stat.h>
#include <mutex>
#include <memory>
#include <functional>

enum GameMode {
	MODE_COMPUTER_PICKS_WORD = 0, // computer picks word, user(s) guess
	MODE_USER_PICKS_WORD, // user picks word, other users guess
	MODE_COMPUTER_GUESSES_WORD // user picks word, computer guesses
};

class Game {
	private:
		// Private use.
		Wordlist list; // wordlist
		airplay_device& conn; // airplay connection
		gdImagePtr background; // background image
		
		// Other threads allowed access, must be thread-safe.
		std::mutex gameMutex; // mutex for protecting variables
		unsigned int level; // level of game
		int levelDiff; // change in level based on result
		GameMode mode; // game mode
		std::string word; // chosen word
		unsigned int wordLength; // for use with computer guessing word
		std::vector<char> guessed; // guessed letters
		std::string alert; // any broadcasts for clients
		unsigned int gameInd = 0; // index of game
		int score = 0; // game score
		int lastGameResult = -1; // result of last game (-1 = ongoing/TBD, 0 = lost, 1 = won)
		bool flashDelay = false; // whether we are in the period after a round ended, before the next
		bool waitingForWord = false; // if we are waiting on the user for a word
		char lastComputerGuess = '\0'; // the last guess by the computer of a letter
		std::map<char, bool> guessValidity; // for computer guesses - map [letter guessed] --> [correct guess or not]
		
		// Helper methods.
		std::string getCurrentGameImage(bool result_screen = false); // generate image for airplay
		void showPicture(std::string& data); // send image over airplay
		int computeScoreChange(bool won, unsigned int level); // compute score change
		char nextLetterToGuess(); // figure out the next letter to guess
	public:
		Game(airplay_device& conn);
		~Game(){ }
		
		// Main methods. //
		int guessLetter(char letter); // guesses the letter, returns number of instances of letter in word
		void start_game(unsigned int level = 15, GameMode mode = MODE_COMPUTER_PICKS_WORD); // start the game
		
		// Accessor methods. //
		// For the mutex.
		std::mutex& getLock(){ return gameMutex; }
		// Level & score.
		unsigned int getLevel(){ return level; }
		int getScore(){ return score; }
		// Guesses (and information about them).
		unsigned int getGuessesNum(){ return guessed.size(); } // get number of guesses
		std::vector<char> getGuessedLetters(){ return guessed; } // get all guesses
		unsigned int getIncorrectGuessesNum(); // get number of incorrect guesses
		std::vector<char> getIncorrectGuesses(); // get all incorrect guesses
		// Game index & result.
		unsigned int getGameIndex(){ return gameInd; } // get game index #
		int getLastGameResult(){ return lastGameResult; } // get last game result
		// Game information.
		GameMode getMode(){ return mode; }
		bool inFlashDelay(){ return flashDelay; }
		bool isWaitingForWord(){ return waitingForWord; }
		// Externally-Triggered Actions.
		std::string chooseWord(std::string word); // user chooses word - returns error message
		std::string chooseLength(int length); // user provides word length - returns error message
		std::string saveGuessResult(std::string result); // result of last guess by computer
		std::string saveWordLocations(std::string word); // save user-provided word locations
		// Word information.
		unsigned int getWordLength(){ return word.length(); }
		std::string getWord(){ return word; }
		std::string getBlankedWord(); // get correctly blanked word based on guesses
		std::string getWordHTMLForm(); // get form version of word
		// Alerts.
		std::string getLatestAlert(){ return alert; }
};

#endif
