#include "Game.h"

class Server {
	private:
		// Constants - initialized in constructor.
		const std::string host;
		const int port;
		
		// Instance variables.
		int sockfd;
		int clientfd;
		Game& game;
		std::string clientOfInterest; // this is set to the IP of the last client that chose a word for the computer/other users to guess
		
		// Helper methods.
		std::string readFile(std::string fname);
		void handleRequest(int clientfd, std::string&& req);
		std::string getClientIP(int clientfd);
		void setClientOfInterest(int clientfd);
	public:
		Server(std::string host, int port, Game& game);
		~Server(){ }
		
		void start();
};