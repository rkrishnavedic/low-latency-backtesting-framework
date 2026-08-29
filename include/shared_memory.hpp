#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <stdexcept>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

template <typename T>
class SharedMemoryRegion {
    std::string name_;
    bool is_owner_{false};
    size_t size_{0};
    T* ptr_{nullptr};
    int fd_{-1};

public:
    SharedMemoryRegion(const std::string &name, bool is_owner)
        : name_(name), is_owner_(is_owner), size_(sizeof(T)), ptr_(nullptr), fd_(-1) {
            if(is_owner_){
            // Unlink any leftover segment from prior crashed runs
            shm_unlink(name_.c_str());

            //create new shared memory object
            fd_ = shm_open(name_.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
            if (fd_ == -1){
                throw std::runtime_error("shm_open creation failed: " + std::string(strerror(errno)));
            }
            // Set total byte capacity (page-aligned for macOS compatibility)
            if (ftruncate(fd_, static_cast<off_t>(size_)) == -1) {
                close(fd_);
                shm_unlink(name_.c_str());
                throw std::runtime_error("ftruncate failed: " + std::string(strerror(errno)));
            }
            } else {
            // Attach to existing shared memory object
            fd_ = shm_open(name_.c_str(), O_RDWR, 0666);
            if (fd_ == -1) {
                throw std::runtime_error("shm_open attach failed: " + std::string(strerror(errno)));
            }
        }

        // Map into process virtual memory
        void* mapped = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mapped == MAP_FAILED) {
            close(fd_);
            if (is_owner_) shm_unlink(name_.c_str());
            throw std::runtime_error("mmap failed: " + std::string(strerror(errno)));
        }

        ptr_ = static_cast<T*>(mapped);

        // Construct object in shared memory if owner
        if (is_owner_) {
            new (ptr_) T();
        }
    }

    ~SharedMemoryRegion() {
        if (ptr_ && ptr_ != MAP_FAILED) {
            if (is_owner_) {
                ptr_->~T();
            }
            munmap(static_cast<void*>(ptr_), size_);
        }
        if (fd_ != -1) {
            close(fd_);
        }
        if (is_owner_) {
            shm_unlink(name_.c_str());
        }
    }

    // Disable copy semantics
    SharedMemoryRegion(const SharedMemoryRegion&) = delete;
    SharedMemoryRegion& operator=(const SharedMemoryRegion&) = delete;

    // Move semantics
    SharedMemoryRegion(SharedMemoryRegion&& other) noexcept
        : name_(std::move(other.name_)), is_owner_(other.is_owner_), size_(other.size_), ptr_(other.ptr_), fd_(other.fd_) {
        other.ptr_ = nullptr;
        other.fd_ = -1;
        other.is_owner_ = false;
    }

    [[nodiscard]] T* get() noexcept { return ptr_; }
    [[nodiscard]] const T* get() const noexcept { return ptr_; }
    [[nodiscard]] T* operator->() noexcept { return ptr_; }
    [[nodiscard]] const T* operator->() const noexcept { return ptr_; }
};