#pragma once

template <typename data_type, typename error_type>
struct either {
    union {
        data_type data;
        error_type error;
    };

    enum its_kind { SUCCESS, FAILURE } kind;

    bool is_success() { return kind == SUCCESS; }
    bool is_failure() { return kind == FAILURE; }

    either<data_type, error_type> map(auto &&lambda) {
        if (is_failure())
            return *this;

        return { lambda(data), error };
    }

    either<data_type, error_type> and_then(auto &&lambda) {
        if (is_failure())
            return *this;

        return lambda(data);
    }

    data_type* operator->() { return &data; }
    data_type& operator* () { return  data; }

    operator bool() { return is_success(); }
};
