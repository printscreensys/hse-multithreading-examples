#include "mpsc_queue.hpp"
#include <string>
#include <chrono>
#include <thread>

int main() {
    try {
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
        
        ConsumerNode consumer("/my_shm_queue");
        std::cout << "Consumer is running. Waiting for messages of type 1...\n";

        while (true) {
            std::vector<uint8_t> data;
            if (consumer.pop(1, data)) {
                std::string text(data.begin(), data.end());
                std::cout << "Received: " << text << "\n";
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
