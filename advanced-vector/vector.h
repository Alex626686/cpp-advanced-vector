#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
        :buffer_(std::move(other.buffer_)), capacity_(std::move(other.capacity_)) {
        other.buffer_ = nullptr;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (&rhs != this) {
            Deallocate(buffer_);
            buffer_ = std::move(rhs.buffer_);
            capacity_ = std::move(rhs.capacity_);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

//////////////////////////////////////////////////

template <typename T>
class Vector {
public:
    Vector() = default;

    Vector(size_t size)
        :size_(size),
        data_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);

    }
    Vector(const Vector& other)
        :size_(other.size_),
        data_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector copy(rhs);
                Swap(copy);
            }
            else {
                size_t i = 0;   //If i call std::copy at rhs.size > size_ programm fall
                for (; i < size_ && i < rhs.size_; ++i) {
                    data_[i] = rhs.data_[i];
                }
                if (rhs.size_ < size_) {
                    std::destroy_n(data_.GetAddress() + i, size_ - rhs.size_);
                }
                else {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + i, rhs.size_ - size_, data_.GetAddress() + i);
                }
            }
            size_ = rhs.size_;
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    T& Back() noexcept {
        return data_[size_ - 1];
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void FillTmpData(RawMemory<T>& data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, data.GetAddress());
        }
    }

    void Reserve(size_t new_capacity) {
        if (Capacity() >= new_capacity) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        FillTmpData(new_data);

        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
    }

    void Resize(size_t new_size) {
        if (size_ >= new_size) {
            std::destroy_n(data_.GetAddress() + (new_size), size_ - new_size);
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    template <typename Type>
    void PushBack(Type&& value) {
        EmplaceBack(std::forward<Type>(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        Emplace(end(), std::forward<Args>(args)...);
        return Back();
    }



    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (size_ < Capacity()) {
            return EasyEmplace(pos, std::forward<Args>(args)...);
        }
        else {
            return RealocationEmplace(pos, std::forward<Args>(args)...);
        }
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        std::move(const_cast<iterator>(pos) + 1, end(), const_cast<iterator>(pos));
        std::destroy_at(end() - 1);
        --size_;
        return const_cast<iterator>(pos);
    }
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    void PopBack() noexcept {
        std::destroy_at(data_.GetAddress() + --size_);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:
    size_t size_ = 0;
    RawMemory<T> data_;

    template <typename... Args>
    iterator EasyEmplace(const_iterator pos, Args&&... args) {
        if (pos != cend()) {
            T tmp(std::forward<Args>(args)...);
            new (end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(const_cast<iterator>(pos), end() - 1, end());
            *const_cast<iterator>(pos) = std::move(tmp);
        }
        else {
            new (end()) T(std::forward<Args>(args)...);
        }
        ++size_;
        return const_cast<iterator>(pos);
    }

    template <typename... Args>
    iterator RealocationEmplace(const_iterator pos, Args&&... args) {
        size_t pos_step = pos - begin();
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data + pos_step) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), pos_step, new_data.GetAddress());
            std::uninitialized_move_n(data_.GetAddress() + pos_step, size_ - pos_step, new_data.GetAddress() + pos_step + 1);
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), pos_step, new_data.GetAddress());
            std::uninitialized_copy_n(data_.GetAddress() + pos_step, size_ - pos_step, new_data.GetAddress() + pos_step + 1);
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
        ++size_;
        return begin() + pos_step;
    }
};