#include <iostream>
#include <vector>
#include <coroutine>
#include <stack>

template<typename T>
struct Generator {
    struct promise_type {
        T current_value;
        Generator get_return_object() { 
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; 
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T value) {
            current_value = value;
            return {};
        }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> coro;
    
    Generator(std::coroutine_handle<promise_type> h) : coro(h) {}
    ~Generator() { if (coro) coro.destroy(); }
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    Generator(Generator&& other) noexcept : coro(other.coro) { other.coro = nullptr; }

    bool move_next() {
        if (coro && !coro.done()) {
            coro.resume();
            return !coro.done();
        }
        return false;
    }
    
    T current_value() const { return coro.promise().current_value; }
};

Generator<int> cooperative_dfs(const std::vector<std::vector<int>>& graph, int start_node) {
    std::vector<bool> visited(graph.size(), false);
    std::stack<int> s;
    s.push(start_node);

    while (!s.empty()) {
        int node = s.top();
        s.pop();

        if (!visited[node]) {
            visited[node] = true;
            
            co_yield node;

            for (auto it = graph[node].rbegin(); it != graph[node].rend(); ++it) {
                if (!visited[*it]) {
                    s.push(*it);
                }
            }
        }
    }
}

int main() {
    std::vector<std::vector<int>> graph = {
        {1, 2},    // 0
        {0, 3, 4}, // 1
        {0, 5},    // 2
        {1},       // 3
        {1},       // 4
        {2}        // 5
    };

    auto dfs_gen = cooperative_dfs(graph, 0);

    while (dfs_gen.move_next()) {
        int current_node = dfs_gen.current_value();
        std::cout << "Visited: " << current_node << "\n";
    }

    return 0;
}
