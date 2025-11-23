#pragma once
#ifndef __CACHE__H
#define __CACHE__H

#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstring>

extern double PageFault;
extern int BLOCK_SIZE; // definido en algún otro módulo / configuración

class CacheBlock
{
public:
    unsigned long page;
    char *data;
    size_t length;

    CacheBlock() : page(0), data(nullptr), length(0) {}
    // deep copy
    CacheBlock(const CacheBlock& CB) : page(CB.page), data(nullptr), length(CB.length)
    {
        if (CB.data && CB.length > 0) {
            data = new char[CB.length];
            memcpy(data, CB.data, CB.length);
        }
    }
    CacheBlock& operator=(const CacheBlock& CB)
    {
        if (this == &CB) return *this;
        delete[] data;
        page = CB.page;
        length = CB.length;
        data = nullptr;
        if (CB.data && length > 0) {
            data = new char[length];
            memcpy(data, CB.data, length);
        }
        return *this;
    }
    // move
    CacheBlock(CacheBlock&& o) noexcept : page(o.page), data(o.data), length(o.length)
    {
        o.data = nullptr; o.length = 0;
    }
    CacheBlock& operator=(CacheBlock&& o) noexcept
    {
        if (this != &o) {
            delete[] data;
            page = o.page;
            data = o.data;
            length = o.length;
            o.data = nullptr;
            o.length = 0;
        }
        return *this;
    }

    ~CacheBlock()
    {
        delete[] data;
        data = nullptr;
        length = 0;
    }
};

class Cache
{
    int size_;
    int maxSize_;
    std::string filename_;
    std::ifstream ifile_;
    std::vector<CacheBlock> blockVector_;

public:
    Cache(const std::string& fn, int maxSize)
        : size_(0), maxSize_(maxSize), filename_(fn)
    {
        ifile_.open(filename_, std::ios::in | std::ios::binary);
        if (!ifile_.is_open()) {
            throw std::runtime_error("Cache: failed to open file " + filename_);
        }
    }

    ~Cache()
    {
        // CacheBlock destructor frees data
        blockVector_.clear();
        if (ifile_.is_open()) ifile_.close();
    }

    bool isFull() const noexcept
    {
        return (size_ == maxSize_);
    }

    void reset()
    {
        blockVector_.clear();
        size_ = 0;
        if (ifile_.is_open()) {
            ifile_.clear();
            ifile_.seekg(0);
        }
    }

    // return the block (most-recently used at [0])
    CacheBlock& getBlock(unsigned long page)
    {
        // search
        for (size_t pos = 0; pos < blockVector_.size(); ++pos) {
            if (blockVector_[pos].page == page) {
                if (pos != 0) advanceBlock(static_cast<int>(pos));
                return blockVector_[0];
            }
        }

        // not found -> page fault
        ++PageFault;

        CacheBlock cur;
        cur.page = page;
        cur.length = static_cast<size_t>(BLOCK_SIZE);
        cur.data = new char[cur.length];

        // read from file; if not enough bytes, fill with zeros
        if (!ifile_.is_open()) {
            throw std::runtime_error("Cache: underlying file closed");
        }
        std::streampos off = static_cast<std::streampos>(page) * static_cast<std::streampos>(BLOCK_SIZE);
        ifile_.clear(); // clear eof flag
        ifile_.seekg(off, std::ios::beg);
        if (!ifile_.good()) {
            // if seekg failed (page beyond EOF), fill zero
            std::fill(cur.data, cur.data + cur.length, 0);
        } else {
            ifile_.read(cur.data, static_cast<std::streamsize>(cur.length));
            std::streamsize r = ifile_.gcount();
            if (r < static_cast<std::streamsize>(cur.length)) {
                // partial read: zero the remainder
                std::fill(cur.data + r, cur.data + cur.length, 0);
            }
        }

        addBlock(std::move(cur)); // insert MRU
        return blockVector_.front();
    }

    void addBlock(CacheBlock&& newBlock)
    {
        if (size_ + 1 > maxSize_) {
            lruStrategy(std::move(newBlock));
        } else {
            blockVector_.insert(blockVector_.begin(), std::move(newBlock));
            ++size_;
        }
    }

    // overload for lvalue
    void addBlock(const CacheBlock& newBlock)
    {
        if (size_ + 1 > maxSize_) {
            CacheBlock tmp(newBlock);
            lruStrategy(std::move(tmp));
        } else {
            blockVector_.insert(blockVector_.begin(), newBlock);
            ++size_;
        }
    }

private:
    void lruStrategy(CacheBlock&& newBlock)
    {
        if (maxSize_ <= 0) return;
        int position = maxSize_ - 1;
        if (position < 0 || position >= static_cast<int>(blockVector_.size())) {
            // just insert
            blockVector_.insert(blockVector_.begin(), std::move(newBlock));
            return;
        }
        // remove LRU at position
        blockVector_.erase(blockVector_.begin() + position);
        // insert new as MRU
        blockVector_.insert(blockVector_.begin(), std::move(newBlock));
        // size stays maxSize_
        size_ = static_cast<int>(blockVector_.size());
    }

    void advanceBlock(int position)
    {
        if (position < 0 || position >= static_cast<int>(blockVector_.size())) return;
        CacheBlock recBlock = std::move(blockVector_[position]);
        blockVector_.erase(blockVector_.begin() + position);
        blockVector_.insert(blockVector_.begin(), std::move(recBlock));
    }
};

#endif // __CACHE__H
