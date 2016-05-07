#include "Server.h"
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define MAX_BACKLOG 500

Server::Server(std::string h, int p, Game& g) : host(h), port(p), game(g) {
	//
}

std::string Server::readFile(std::string fname){
	std::ifstream ifp(fname);
	if(!ifp.is_open()){
		std::cerr << "Warning: Could not open file '" << fname << "' for reading." << std::endl;
		return "";
	}
	std::string ret;
	ifp.seekg(0, std::ios::end);
	ret.reserve(ifp.tellg());
	ifp.seekg(0, std::ios::beg);
	ret.assign((std::istreambuf_iterator<char>(ifp)), std::istreambuf_iterator<char>());
	return ret;
}

std::string Server::getClientIP(int sock){
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
    int res = getpeername(sock, (struct sockaddr*)&addr, &addr_size);
    if(res == -1){
    	std::cerr << "Warning: Could not determine IP address of client." << std::endl;
    	perror("getpeername()");
    	return "(unknown)";
    }
    char* clientIP = new char[20];
    strcpy(clientIP, inet_ntoa(addr.sin_addr));
    std::string ret(clientIP);
    delete[] clientIP;
    return ret;
}

void Server::setClientOfInterest(int sock){
    clientOfInterest = getClientIP(sock);
}

void Server::handleRequest(int sock, std::string&& req){
	// Parse request, only handling HTTP GET requests. //
	if(req.find("GET ") == 0U && req.find(" HTTP/1.1") != std::string::npos){
		// Extract requested path from GET request. //
		std::string path = req.substr(4, req.find(" HTTP/1.1") - 4);
		//printf("Requested path: |%s|.\n", path.c_str());
		
		// Decide what to send back based on requested path.
		int code = 200; // return code
		std::string ret; // return body
		std::string mime = "text/html"; // MIME-type
		if(path.find("../") != std::string::npos || path.find("/..") != std::string::npos){
			code = 403;
		} else if(path == "/" || path == "/index.html"){
			ret = readFile("data/index.html");
		} else if(path == "/getExtantLetters"){
			mime = "application/json";
			auto guessed = game.getGuessedLetters();
			std::vector<char> extant;
			for(char c = 'a'; c <= 'z'; c++){
				if(std::find(guessed.begin(), guessed.end(), c) == guessed.end()){
					extant.push_back(c);
				}
			}
			ret = "{\"letters\": [";
			for(auto it = extant.begin(); it != extant.end(); it++){
				ret += "\"";
				ret.push_back(*it);
				ret += "\"";
				if((it + 1) != extant.end()) ret += ",";
			}
			ret += "]}";
		} else if(path.find("/guessLetter?letter=") == 0U){
			mime = "application/json";
			int letter_code = 0;
			for(unsigned long int i = 20; i < path.length(); i++){
				letter_code *= 10;
				letter_code += path[i] - '0';
			}
			char letter = (char)letter_code;
			auto guessed = game.getGuessedLetters();
			bool error = false; // error with input
			bool success = false; // correctness of guess
			std::stringstream msg;
			if(game.getIncorrectGuessesNum() >= GUESS_LIMIT){
				error = true;
				msg << "All " << GUESS_LIMIT << " guesses have been used.";
			} else if(!std::isalpha(letter) || !std::islower(letter)){
				error = true;
				msg << "Invalid character '" << letter << "'- must be a lowercase letter.";
			} else if(std::find(guessed.begin(), guessed.end(), letter) != guessed.end()){
				error = true;
				msg << "Someone already guessed that letter!";
			} else {
				int instances = game.guessLetter(letter);
				error = false;
				if(instances > 0){
					success = true;
					msg << "Correct! There ";
					if(instances == 1) msg << "was 1 instance";
					else msg << "were " << instances << " instances";
					msg << " of '" << letter << "' in the word.";
				} else {
					msg << "The letter '" << letter << "' was not in the word.";
				}
			}
			ret = "{\"error\": " + std::string(error ? "true" : "false");
			ret += ", \"message\": \"" + msg.str() + "\", \"success\": ";
			ret += std::string(success ? "true" : "false") + "}";
		} else if(path == "/guessPercentage"){
			mime = "application/json";
			double percent = double(game.getIncorrectGuessesNum()) / GUESS_LIMIT * 100.0f;
			char buf[20];
			sprintf(buf, "%.2f", percent);
			ret = "{\"percentage\": \"" + std::string(buf) + "\"}";
		} else if(path == "/getBlankedWord"){
			mime = "application/json";
			std::string blanked = game.getBlankedWord();
			std::stringstream fmt;
			fmt << "{\"blanked\": \"" << blanked << "\", \"length\": " << game.getWordLength() << "}";
			ret = fmt.str();
		} else if(path == "/getLatestAlert"){
			mime = "application/json";
			std::string alert = game.getLatestAlert();
			ret = "{\"alert\": \"" + alert + "\"}";
		} else if(path == "/getGameInfo"){
			mime = "application/json";
			std::stringstream fmt;
			std::string word = (game.inFlashDelay() ? game.getWord() : "");
			unsigned int level = game.getLevel();
			fmt << "{\"level\":" << level;
			fmt << ", \"index\": " << game.getGameIndex();
			fmt << ", \"result\": " << game.getLastGameResult();
			fmt << ", \"word\": \"" << word << "\"";
			fmt << ", \"ip_addr\": \"" << getClientIP(sock) << "\"";
			fmt << ", \"waitingForWord\": " << (game.isWaitingForWord() ? "true" : "false");
			fmt << ", \"score\": " << game.getScore() << "}";
			ret = fmt.str();
		} else if(path.find("/chooseWord?word=") == 0U){
			mime = "application/json";
			std::stringstream fmt;
			std::string err = game.chooseWord(path.substr(17U));
			bool suc = (err.length() == 0UL);
			if(suc){
				setClientOfInterest(sock);
			}
			fmt << "{\"success\": " << (suc ? "true" : "false");
			fmt << ", \"error\": \"" << err << "\"";
			fmt << "}";
			ret = fmt.str();
		} else if(path.find("/setWordLength?length=") == 0U){
			mime = "application/json";
			std::stringstream fmt;
			std::string err = game.chooseLength(atoi(path.substr(22U).c_str()));
			bool suc = (err.length() == 0UL);
			if(suc){
				setClientOfInterest(sock);
			}
			fmt << "{\"success\": " << (suc ? "true" : "false");
			fmt << ", \"error\": \"" << err << "\"";
			fmt << "}";
			ret = fmt.str();
		} else if(path.find("/setLetterInWord?in_word=") == 0U){
			mime = "application/json";
			std::stringstream fmt;
			std::string err = game.saveGuessResult(path.substr(25U));
			bool suc = (err.length() == 0UL);
			if(suc){
				setClientOfInterest(sock);
			}
			fmt << "{\"success\": " << (suc ? "true" : "false");
			fmt << ", \"error\": \"" << err << "\"";
			fmt << "}";
			ret = fmt.str();
		} else if(path.find("/setWordLocations?word=") == 0U){
			mime = "application/json";
			std::stringstream fmt;
			std::string err = game.saveWordLocations(path.substr(23U));
			bool suc = (err.length() == 0UL);
			if(suc){
				setClientOfInterest(sock);
			}
			fmt << "{\"success\": " << (suc ? "true" : "false");
			fmt << ", \"error\": \"" << err << "\"";
			fmt << "}";
			ret = fmt.str();
		} else if(path == "/getWordFillForm"){
			ret = game.getWordHTMLForm();
		} else {
			path = "data/" + path; // must be in the data directory
			if(access(path.c_str(), F_OK ) != -1){
				// File exists, return it.
				ret = readFile(path.c_str());
			} else {
				code = 404;
			}
		}
		
		// Generate response based on code.
		std::stringstream response;
		std::string blurb = "OK"; // e.g. 200 OK
		if(code == 403){
			blurb = "Forbidden";
		} else if(code == 404){
			blurb = "Not Found";
		}
		if(code != 200){
			ret = "<html><head><title>" + blurb + "</title></head><body><h1>" + blurb + "</h1></body></html>";
		}
		response << "HTTP/1.1 " << code << " " << blurb << "\r\nContent-Type: " << mime << "\r\nContent-Length: " << (ret.length() + 4) << "\r\nCache-Control: no-cache\r\n\r\n" << ret << "\r\n\r\n";
		
		// Send response.
		int at = 0;
		ret = response.str();
		while((unsigned long int)at < ret.length()){
			unsigned int length = std::min(1500UL, ret.length() - at);
			int n = ::write(sock, ret.substr(at, length).c_str(), length);
			if(n < 0){
				std::cerr << "Warning: Could not send response to socket." << std::endl;
				break;
			}
			at += n;
		}
	}
	
	// Close the socket. //
	::close(clientfd);
}

bool setTimeout(int sockfd, int timeout_secs){
	struct timeval timeout;      
	timeout.tv_sec = timeout_secs;
	timeout.tv_usec = 0;
	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		return false;
	if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		return false;
	return true;
}

void Server::start(void){
	// Initialize variables.
	struct sockaddr_in serv_addr, cli_addr;
	
	// Create the socket.
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		std::cerr << "Error creating socket." << std::endl;
		return;
	}
	
	// Set SO_REUSEADDR on it.
	int flagVal = 1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flagVal, sizeof(flagVal)) < 0){
		std::cerr << "Could not set SO_REUSEADDR on socket." << std::endl;
		return;
	}
	
	// Bind it to the specified host and port.
	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	while(::bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		std::cerr << "Error: Could not bind to port. Retrying in 5 seconds..." << std::endl;
		sleep(5);
	}
	
	// Start listening and loop, accepting new connections.
	::listen(sockfd, MAX_BACKLOG);
	std::cerr << "Listening on port " << port << "..." << std::endl;
	unsigned int cli_len = sizeof(cli_addr);
	while(true){
		// Accept a connection (blocking).
		clientfd = ::accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);
		if(clientfd < 0){
			std::cerr << "Warning: Could not accept connection." << std::endl;
			continue;
		}
		
		// Set a timeout on the socket.
		if(!setTimeout(clientfd, 3)){
			std::cerr << "Warning: Could not set timeout on client socket." << std::endl;
			continue;
		}
		
		// Read request.
		const int MESSAGE_SIZE = 256;
		char buf[MESSAGE_SIZE];
		bzero(buf, MESSAGE_SIZE);
		if(::read(clientfd, buf, MESSAGE_SIZE - 1) < 0){
			std::cerr << "Warning: Could not read from client socket." << std::endl;
			continue;
		}
		
		// Handle request - also handles socket closing, etc.
		handleRequest(clientfd, std::string(buf));
	}
}































