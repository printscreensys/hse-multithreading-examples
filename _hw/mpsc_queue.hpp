#pragma once

#include <iostream>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <thread>

constexpr uint32_t PROTOCOL_VERSION = 1;

struct MessageHeader {
    uint32_t type;
    uint32_t length;
    std::atomic<bool> is_ready;
};

struct QueueMeta {
    uint32_t version;
    size_t capacity;
    std::atomic<size_t> write_pos;
    std::atomic<size_t> read_pos;
};

class MPSCQueueBase {
protected:
    std::string name_;
    size_t capacity_;
    int shm_fd_ = -1;
    void* mapped_region_ = MAP_FAILED;
    QueueMeta* meta_ = nullptr;
    uint8_t* buffer_ = nullptr;

    void cleanup() {
        if (mapped_region_ != MAP_FAILED) {
            munmap(mapped_region_, sizeof(QueueMeta) + capacity_);
        }
        if (shm_fd_ != -1) {
            close(shm_fd_);
        }
    }
};

class ProducerNode : public MPSCQueueBase {
public:
    ProducerNode(const std::string& name, size_t capacity) {
        name_ = name;
        capacity_ = capacity;
        size_t total_size = sizeof(QueueMeta) + capacity_;

        shm_fd_ = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
        if (shm_fd_ == -1) throw std::runtime_error("shm_open failed");

        ftruncate(shm_fd_, total_size);

        mapped_region_ = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (mapped_region_ == MAP_FAILED) throw std::runtime_error("mmap failed");

        meta_ = static_cast<QueueMeta*>(mapped_region_);
        buffer_ = static_cast<uint8_t*>(mapped_region_) + sizeof(QueueMeta);

        meta_->version = PROTOCOL_VERSION;
        meta_->capacity = capacity_;
        meta_->write_pos.store(0, std::memory_order_relaxed);
        meta_->read_pos.store(0, std::memory_order_relaxed);
    }

    ~ProducerNode() {
        cleanup();
        shm_unlink(name_.c_str());
    }

    bool push(uint32_t type, const std::vector<uint8_t>& data) {
        size_t total_msg_size = sizeof(MessageHeader) + data.size();
        
        size_t current_write;
        size_t next_write;
        do {
            current_write = meta_->write_pos.load(std::memory_order_acquire);
            next_write = (current_write + total_msg_size) % meta_->capacity;
            
            size_t current_read = meta_->read_pos.load(std::memory_order_acquire);
            size_t available = (current_read <= current_write) ? 
                               (meta_->capacity - current_write + current_read) : 
                               (current_read - current_write);
                               
            if (available <= total_msg_size) {
                return false;
            }
        } while (!meta_->write_pos.compare_exchange_weak(current_write, next_write, std::memory_order_release));

        MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer_ + current_write);
        header->type = type;
        header->length = data.size();
        header->is_ready.store(false, std::memory_order_relaxed);

        std::memcpy(buffer_ + current_write + sizeof(MessageHeader), data.data(), data.size());

        header->is_ready.store(true, std::memory_order_release);
        return true;
    }
};

class ConsumerNode : public MPSCQueueBase {
public:
        ConsumerNode(const std::string& name) {
        name_ = name;
        
        shm_fd_ = shm_open(name.c_str(), O_RDWR, 0666);
        if (shm_fd_ == -1) throw std::runtime_error("shm_open failed");

        struct stat shm_stat;
        if (fstat(shm_fd_, &shm_stat) == -1) {
            throw std::runtime_error("fstat failed");
        }
        
        size_t total_size = shm_stat.st_size;

        mapped_region_ = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (mapped_region_ == MAP_FAILED) throw std::runtime_error("mmap failed");

        meta_ = static_cast<QueueMeta*>(mapped_region_);
        
        if (meta_->version != PROTOCOL_VERSION) {
            throw std::runtime_error("Protocol version mismatch");
        }
        
        capacity_ = meta_->capacity;
        buffer_ = static_cast<uint8_t*>(mapped_region_) + sizeof(QueueMeta);
    }

    ~ConsumerNode() {
        cleanup();
    }

    bool pop(uint32_t target_type, std::vector<uint8_t>& out_data) {
        size_t current_read = meta_->read_pos.load(std::memory_order_relaxed);
        size_t current_write = meta_->write_pos.load(std::memory_order_acquire);

        if (current_read == current_write) {
            return false;
        }

        MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer_ + current_read);
        
        while (!header->is_ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        size_t total_msg_size = sizeof(MessageHeader) + header->length;

        bool matched = (header->type == target_type);
        if (matched) {
            out_data.resize(header->length);
            std::memcpy(out_data.data(), buffer_ + current_read + sizeof(MessageHeader), header->length);
        }

        meta_->read_pos.store((current_read + total_msg_size) % meta_->capacity, std::memory_order_release);

        return matched;
    }
};
