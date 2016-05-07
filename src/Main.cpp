#include "Words.h"
#include "Game.h"
#include "Server.h"

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8080

int main(int argc, char** argv){
	// Search for Airplay-compatible devices and select the first one.
	AirplayBrowser browser;
	unsigned int num_discovered = browser.browseForDevices();
	printf("Discovered %u device(s).\n", num_discovered);
	AirplayDevice& device = browser.getDiscovered()[0];
	AirplayConnection conn(device);
	conn.openConnection();
	printf("Sending message...\n");
	conn.sendMessage(MESSAGE_GET_SERVICES, NULL, /*debug=*/true);
	printf("Sent!\n");
	
	// Initialize thread pool.
	std::vector<std::thread> threads;
	
	// Start the game with the selected mode.
	Game game(conn);
	threads.push_back(std::thread(&Game::start_game, &game, /*level=*/1, /*mode=*/MODE_COMPUTER_GUESSES_WORD));
	
	// Start the web server.
	Server server(SERVER_HOST, SERVER_PORT, game);
	threads.push_back(std::thread(&Server::start, &server));
	
	// Wait for threads to finish execution.
	for(std::thread& t : threads){
		t.join();
	}
	return 0;
}