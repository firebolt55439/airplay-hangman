#include "Words.h"
#include "Game.h"
#include "Server.h"

#define ASSERT(x, m) if(!(x)){ fprintf(stderr, m "\n"); ::exit(1); }
#define MAP_ITEM(r) {r, #r}

std::string SERVER_HOST = "127.0.0.1";
unsigned int SERVER_PORT = 8001;
GameMode GAME_MODE = MODE_COMPUTER_PICKS_WORD;

const std::map<GameMode, std::string> MODE_DESCRIPTORS = {
	{MODE_COMPUTER_PICKS_WORD, "MODE_COMPUTER_PICKS_WORD"}
};

int help(int argc, char** argv){
	fprintf(stderr, "%s [-h/--help] [-m/--mode (0-2)] [-p/--port (PORT)] [-h/--host (HOST)]\n", argv[0]);
	fprintf(stderr, "Port is set to %u and host is set to %s\n", SERVER_PORT, SERVER_HOST.c_str());
	fprintf(stderr, "Modes:\n\t0 = MODE_COMPUTER_PICKS_WORD\n\t1 = MODE_USER_PICKS_WORD\n\t2 = MODE_COMPUTER_GUESSES_WORD\n");
	return 0;
}

int main(int argc, char** argv){
	// Parse command-line arguments.
	if(argc == 1){
		return help(argc, argv);
	}
	for(unsigned int i = 1; i < argc; i++){
		std::string on(argv[i]);
		if(on == "-h" || on == "--help"){
			return help(argc, argv);
		} else if(on == "-m" || on == "--mode"){
			ASSERT((i + 1) < argc, "Not enough arguments to -m/--mode");
			GAME_MODE = GameMode(atoi(argv[i + 1]));
		} else if(on == "-p" || on == "--port"){
			ASSERT((i + 1) < argc, "Not enough arguments to -p/--port");
			SERVER_PORT = atoi(argv[i + 1]);
		} else if(on == "-h" || on == "--host"){
			ASSERT((i + 1) < argc, "Not enough arguments to -h/--host");
			SERVER_HOST = std::string(argv[i + 1]);
		}
	}
	printf("Host: %s | Port: %u | Mode: %d\n", SERVER_HOST.c_str(), SERVER_PORT, GAME_MODE);

	// Search for Airplay-compatible devices and select the first one.
	const auto devices = airplay_browser::get_devices();
	unsigned int num_discovered = devices.size();
	printf("Discovered %u device(s).\n", num_discovered);
	airplay_device conn(devices.begin()->second);
	printf("Sending message...");
	conn.send_message(MessageType::GetServices);
	printf("done!\n");

	// Initialize thread pool.
	std::vector<std::thread> threads;

	// Start the game with the selected mode.
	Game game(conn);
	threads.push_back(std::thread(&Game::start_game, &game, /*level=*/1, /*mode=*/GAME_MODE));

	// Start the web server.
	Server server(SERVER_HOST, SERVER_PORT, game);
	threads.push_back(std::thread(&Server::start, &server));

	// Wait for threads to finish execution.
	for(std::thread& t : threads){
		t.join();
	}
	return 0;
}
