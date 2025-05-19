#pragma once

#include <iostream>
#include <vector>

namespace log
{
    const size_t BUFFER_DEFAULT_SIZE = 8 * 1024 * 1024;
    const size_t BUFFER_INCREACE_SIZE = 1 * 1024 * 1024;
    const size_t BUFFER_THRESHOLD_SIZE = 8 * 1024 * 1024;
    class Buffer
    {
    public:
        Buffer()
            : _read_ptr(0),
              _write_ptr(0),
              _container(BUFFER_DEFAULT_SIZE)
        {}
        bool empty() { return _read_ptr == _write_ptr; }
        size_t readAbleSize() { return _write_ptr - _read_ptr; }
        size_t writeAbleSize() { return _container.size() - _write_ptr; }
        void reset() { _write_ptr = _read_ptr = 0; }
        void swap(Buffer &buffer)
        {
            std::swap(_read_ptr, buffer._read_ptr);
            std::swap(_write_ptr, buffer._write_ptr);
            _container.swap(buffer._container);
        }
        void push(const char *data, size_t len)
        {
            ensureEnoughSpace(len);
            std::copy(data, data + len, _container.data() + _write_ptr);
            _write_ptr += len;
        }
        const char *begin() {return _container.data() + _read_ptr;}
        void pop(size_t len) {
            if(len > readAbleSize()) throw std::runtime_error("Unauthorized access to container");
            _read_ptr += len;
        }

    private:
        void ensureEnoughSpace(size_t len)
        {
            //写入长度超出可写长度范围，需要扩容
            while(len > writeAbleSize())
            {
                //小于阈值则指数增长扩容
                if(len < BUFFER_THRESHOLD_SIZE) _container.resize(_container.size() << 1);
                //否则线性增长
                else _container.resize(_container.size() + BUFFER_INCREACE_SIZE);
            }
        }
        size_t _read_ptr;
        size_t _write_ptr;
        std::vector<char> _container;
    };
};