#pragma once

#include <vector>
#include <memory>

class BufferList {
    protected:
        std::vector<std::unique_ptr<char[]>> list;
        std::vector<size_t> sizeList;
        size_t totalSize;
        size_t offset;
        char get(const size_t &k) {
            size_t left = 0;
            for (size_t i = 0; i < sizeList.size(); i++) {
                auto len = sizeList[i];
                auto actId = k + offset;
                if (left + len >= actId) {
                    auto item = list[i].get();
                    return item[actId - left];
                }
                left += len;
            }
            throw "not found";
        }
    public:
        BufferList (): totalSize(0), offset(0) {}
        ~BufferList () {}
        size_t size() {
            return totalSize - offset;
        }
        void clear() {
            list.clear();
            sizeList.clear();
            totalSize = 0;
            offset = 0;
        }
        void add(std::unique_ptr<char[]> buf, size_t size) {
            list.push_back(std::move(buf));
            sizeList.push_back(size);
            totalSize += size;
        }
        void advance(size_t n) {
            offset += n;
            while (offset >= sizeList[0]) {
                auto len = sizeList[0];

                list.erase(list.begin());
                sizeList.erase(sizeList.begin());

                totalSize -= len;
                offset -= len;
            }
        }
        void copyTo(size_t begin, char* dst, size_t n) {
            for (size_t i = 0; i < n; i++) {
                dst[i] = get(begin + i);
            }
        }
        char operator [](const size_t &k) {
            return get(k);
        }
};
