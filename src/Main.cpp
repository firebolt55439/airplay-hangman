#include "Words.h"
#include "Game.h"
#include "Server.h"

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8001

int main(int argc, char** argv){
	// Search for Airplay-compatible devices and select the first one.
	const auto devices = airplay_browser::get_devices();
	unsigned int num_discovered = devices.size();
	printf("Discovered %u device(s).\n", num_discovered);
	airplay_device conn(devices.begin()->second);
	printf("Sending message...\n");
	// conn.sendMessage(MESSAGE_GET_SERVICES, NULL, /*debug=*/true);
	std::cout << conn.send_message(MessageType::GetServices) << std::endl;
	printf("Sent!\n");
	
	// Initialize thread pool.
	std::vector<std::thread> threads;
	
	// Start the game with the selected mode.
	Game game(conn);
	threads.push_back(std::thread(&Game::start_game, &game, /*level=*/1, /*mode=*/MODE_COMPUTER_PICKS_WORD));
	
	// Start the web server.
	Server server(SERVER_HOST, SERVER_PORT, game);
	threads.push_back(std::thread(&Server::start, &server));
	
	// Wait for threads to finish execution.
	for(std::thread& t : threads){
		t.join();
	}
	return 0;
}
