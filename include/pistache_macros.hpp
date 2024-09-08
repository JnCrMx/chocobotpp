#pragma once

#define SAFE_HEADER_CAST

#ifdef SAFE_HEADER_CAST
#define NAME(header_name)                                                               \
    static constexpr uint64_t Hash = Pistache::Http::Header::detail::hash(header_name); \
    uint64_t hash() const override { return Hash; }                                     \
    static constexpr const char* Name = header_name;                                    \
    const char* name() const override { return Name; }
#else
#define NAME(header_name)                            \
    static constexpr const char* Name = header_name; \
    const char* name() const override { return Name; }
#endif

/* Crazy macro machinery to generate a unique variable name
 * Don't touch it !
 */
#define CAT(a, b) CAT_I(a, b)
#define CAT_I(a, b) a##b

#define UNIQUE_NAME(base) CAT(base, __LINE__)

#define RegisterHeader(Header) \
    Registrar<Header> UNIQUE_NAME(CAT(CAT_I(__, Header), __))
