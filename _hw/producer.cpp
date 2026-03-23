#include "mpsc_queue.hpp"
#include <string>
#include <ctime>

int main() {
    std::srand(std::time(nullptr));
    try {
        ProducerNode producer("/my_shm_queue", 1024 * 1024);
        std::string text;
        std::cout << "Producer is running. Sending messages...\n";

        while (true) {
            int rand = std::rand() % 2;
            if (rand == 0) {
                text = "Foo" + std::to_string(std::time(0));
                std::vector<uint8_t> data(text.begin(), text.end());

                producer.push(1, data);
                std::cout << "Sent msg type 1\n";
            } else {
                text = "Bar";
                std::vector<uint8_t> trash(text.begin(), text.end());
                producer.push(2, trash);
                std::cout << "Sent msg type 2\n";
            }
            sleep(1);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
