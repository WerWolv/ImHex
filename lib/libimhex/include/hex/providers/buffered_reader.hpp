#pragma once

#include <span>
#include <vector>

#include <hex/providers/provider.hpp>
#include <hex/helpers/literals.hpp>

namespace hex::prv {

    using namespace hex::literals;

    class BufferedReader {
    public:
        explicit BufferedReader(Provider *provider, size_t bufferSize = 16_MiB)
        : m_provider(provider), m_bufferAddress(provider->getBaseAddress()), m_maxBufferSize(bufferSize),
          m_startAddress(provider->getBaseAddress()), m_endAddress(provider->getBaseAddress() + provider->getActualSize() - 1LLU),
          m_buffer(bufferSize) {

        }

        void seek(u64 address) {
            this->m_startAddress = address;
        }

        void setEndAddress(u64 address) {
            if (address >= this->m_provider->getActualSize())
                address = this->m_provider->getActualSize() - 1;

            this->m_endAddress = address;
        }

        [[nodiscard]] std::vector<u8> read(u64 address, size_t size) {
            if (size > this->m_buffer.size()) {
                std::vector<u8> result;
                result.resize(size);

                this->m_provider->read(address, result.data(), result.size());

                return result;
            }

            this->updateBuffer(address, size);

            auto result = &this->m_buffer[address -  this->m_bufferAddress];

            return { result, result + std::min(size, this->m_buffer.size()) };
        }

        [[nodiscard]] std::vector<u8> readReverse(u64 address, size_t size) {
            if (size > this->m_buffer.size()) {
                std::vector<u8> result;
                result.resize(size);

                this->m_provider->read(address, result.data(), result.size());

                return result;
            }

            this->updateBuffer(address - std::min<u64>(address, this->m_buffer.size()), size);

            auto result = &this->m_buffer[address - this->m_bufferAddress];

            return { result, result + std::min(size, this->m_buffer.size()) };
        }

        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = u8;
            using pointer           = const value_type*;
            using reference         = const value_type&;

            Iterator(BufferedReader *reader, u64 address) : m_reader(reader), m_address(address) {}

            Iterator& operator++() {
                this->m_address++;

                return *this;
            }

            Iterator operator++(int) {
                auto copy = *this;
                this->m_address++;

                return copy;
            }

            Iterator& operator--() {
                this->m_address--;

                return *this;
            }

            Iterator operator--(int) {
                auto copy = *this;
                this->m_address--;

                return copy;
            }

            Iterator& operator+=(i64 offset) {
                this->m_address += offset;

                return *this;
            }

            Iterator& operator-=(i64 offset) {
                this->m_address -= offset;

                return *this;
            }

            value_type operator*() const {
                return (*this)[0];
            }

            [[nodiscard]] u64 getAddress() const {
                return this->m_address;
            }

            void setAddress(u64 address) {
                this->m_address = address;
            }

            difference_type operator-(const Iterator &other) const {
                return this->m_address - other.m_address;
            }

            Iterator operator+(i64 offset) const {
                return { this->m_reader, this->m_address + offset };
            }

            value_type operator[](i64 offset) const {
                auto result = this->m_reader->read(this->m_address + offset, 1);
                if (result.empty())
                    return 0x00;

                return result[0];
            }

            friend bool operator== (const Iterator& left, const Iterator& right) { return left.m_address == right.m_address; };
            friend bool operator!= (const Iterator& left, const Iterator& right) { return left.m_address != right.m_address; };
            friend bool operator>  (const Iterator& left, const Iterator& right) { return left.m_address >  right.m_address; };
            friend bool operator<  (const Iterator& left, const Iterator& right) { return left.m_address <  right.m_address; };
            friend bool operator>= (const Iterator& left, const Iterator& right) { return left.m_address >= right.m_address; };
            friend bool operator<= (const Iterator& left, const Iterator& right) { return left.m_address <= right.m_address; };

        private:
            BufferedReader *m_reader;
            u64 m_address;
        };

        class ReverseIterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = u8;
            using pointer           = const value_type*;
            using reference         = const value_type&;

            ReverseIterator(BufferedReader *reader, u64 address) : m_reader(reader), m_address(address) {}

            ReverseIterator& operator++() {
                this->m_address--;

                return *this;
            }

            ReverseIterator operator++(int) {
                auto copy = *this;
                this->m_address--;

                return copy;
            }

            ReverseIterator& operator--() {
                this->m_address++;

                return *this;
            }

            ReverseIterator operator--(int) {
                auto copy = *this;
                this->m_address++;

                return copy;
            }

            ReverseIterator& operator+=(i64 offset) {
                this->m_address -= offset;

                return *this;
            }

            ReverseIterator& operator-=(i64 offset) {
                this->m_address += offset;

                return *this;
            }

            value_type operator*() const {
                return (*this)[0];
            }

            [[nodiscard]] u64 getAddress() const {
                return this->m_address;
            }

            void setAddress(u64 address) {
                this->m_address = address;
            }

            difference_type operator-(const ReverseIterator &other) const {
                return other.m_address - this->m_address;
            }

            ReverseIterator operator+(i64 offset) const {
                return { this->m_reader, this->m_address - offset };
            }

            value_type operator[](i64 offset) const {
                auto result = this->m_reader->readReverse(this->m_address - offset, 1);
                if (result.empty())
                    return 0x00;

                return result[0];
            }

            friend bool operator== (const ReverseIterator& left, const ReverseIterator& right) { return left.m_address == right.m_address; };
            friend bool operator!= (const ReverseIterator& left, const ReverseIterator& right) { return left.m_address != right.m_address; };
            friend bool operator>  (const ReverseIterator& left, const ReverseIterator& right) { return left.m_address >  right.m_address; };
            friend bool operator<  (const ReverseIterator& left, const ReverseIterator& right) { return left.m_address <  right.m_address; };
            friend bool operator>= (const ReverseIterator& left, const ReverseIterator& right) { return left.m_address >= right.m_address; };
            friend bool operator<= (const ReverseIterator& left, const ReverseIterator& right) { return left.m_address <= right.m_address; };

        private:
            BufferedReader *m_reader;
            u64 m_address = 0x00;
        };

        Iterator begin() {
            return { this, this->m_startAddress };
        }

        Iterator end() {
            return { this, this->m_endAddress + 1 };
        }

        ReverseIterator rbegin() {
            return { this, this->m_startAddress };
        }

        ReverseIterator rend() {
            return { this, 0 };
        }

    private:
        void updateBuffer(u64 address, size_t size) {
            if (address > this->m_endAddress)
                return;

            if (!this->m_bufferValid || address < this->m_bufferAddress || address + size > (this->m_bufferAddress + this->m_buffer.size())) {
                const auto remainingBytes = (this->m_endAddress - address) + 1;
                if (remainingBytes < this->m_maxBufferSize)
                    this->m_buffer.resize(remainingBytes);
                else
                    this->m_buffer.resize(this->m_maxBufferSize);

                this->m_provider->read(address, this->m_buffer.data(), this->m_buffer.size());
                this->m_bufferAddress = address;
                this->m_bufferValid = true;
            }
        }

    private:
        Provider *m_provider;

        u64 m_bufferAddress = 0x00;
        size_t m_maxBufferSize;
        bool m_bufferValid = false;
        u64 m_startAddress = 0x00, m_endAddress;
        std::vector<u8> m_buffer;
    };

}