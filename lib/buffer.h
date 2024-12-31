#pragma once

#include <cstddef>

struct buffer {
    void *data;
    std::size_t size;

    buffer(void *data, std::size_t size):
        data(data),
        size(size) {
    }

    template <typename type, std::size_t size>
    buffer(type array[size]):
        buffer(array, size * sizeof(type)) {
    }

    template <typename type>
    buffer(type &element):
        buffer(&element, sizeof(type)) {
    }
};

