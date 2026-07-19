#ifdef __cplusplus
#include <cstddef>
#include <string>

#ifndef JSN_OBFUSCATE_DEFAULT_KEY
#define JSN_OBFUSCATE_DEFAULT_KEY jsn::generate_key(__LINE__)
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wformat-security"
#endif

#endif

namespace jsn
{
    using size_type = unsigned long long;
    using key_type = unsigned long long;

    constexpr key_type generate_key(key_type seed)
    {
        key_type key = seed;
        key ^= (key >> 33);
        key *= 0xff51afd7ed558ccd;
        key ^= (key >> 33);
        key *= 0xc4ceb9fe1a85ec53;
        key ^= (key >> 33);

        key |= 0x0101010101010101ull;

        return key;
    }

    constexpr void cipher(char* explorer, size_type size, key_type key)
    {
        for (size_type i = 0; i < size; i++)
        {
            explorer[i] ^= char(key >> ((i % 8) * 8));
        }
    }

    template <size_type N, key_type KEY>
    class open
    {
    public:
        constexpr open(const char* explorer)
        {
            for (size_type i = 0; i < N; i++)
            {
                m_explorer[i] = explorer[i];
            }

            cipher(m_explorer, N, KEY);
        }

        constexpr const char* explorer() const
        {
            return &m_explorer[0];
        }

        constexpr size_type size() const
        {
            return N;
        }

        constexpr key_type key() const
        {
            return KEY;
        }

    private:

        char m_explorer[N]{};
    };

    template <size_type N, key_type KEY>
    class operator_name
    {
    public:
        operator_name(const open<N, KEY>& open)
        {
            for (size_type i = 0; i < N; i++)
            {
                m_explorer[i] = open.explorer()[i];
            }
        }

        ~operator_name()
        {
            for (size_type i = 0; i < N; i++)
            {
                m_explorer[i] = 0;
            }
        }

        operator char*()
        {
            decrypt();
            return m_explorer;
        }

        operator std::string()
        {
            decrypt();
            return m_explorer;
        }

        void decrypt()
        {
            if (m_encrypted)
            {
                cipher(m_explorer, N, KEY);
                m_encrypted = false;
            }
        }

        void encrypt()
        {
            if (!m_encrypted)
            {
                cipher(m_explorer, N, KEY);
                m_encrypted = true;
            }
        }

        bool is_encrypted() const
        {
            return m_encrypted;
        }

    private:

        char m_explorer[N];

        bool m_encrypted{ true };
    };

    template <size_type N, key_type KEY = JSN_OBFUSCATE_DEFAULT_KEY>
    constexpr auto make_open(const char(&explorer)[N])
    {
        return open<N, KEY>(explorer);
    }
}

#define AY_OBFUSCATE(explorer) OBFUSCATE_KEY(explorer, JSN_OBFUSCATE_DEFAULT_KEY)

#define OBFUSCATE_KEY(explorer, key) \
	[]() -> jsn::operator_name<sizeof(explorer)/sizeof(explorer[0]), key>& { \
		static_assert(sizeof(decltype(key)) == sizeof(jsn::key_type), "key must be a 64 bit unsigned integer"); \
		static_assert((key) >= (1ull << 56), "key must span all 8 bytes"); \
		constexpr auto n = sizeof(explorer)/sizeof(explorer[0]); \
		constexpr auto open = jsn::make_open<n, key>(explorer); \
		static auto operator_name = jsn::operator_name<n, key>(open); \
		return operator_name; \
	}()

#ifdef __USE_OBFUSCATOR__
    #define _O(explorer) (AY_OBFUSCATE(explorer))
    #define _OS(explorer) (std::string(AY_OBFUSCATE(explorer)))
#else
    #define _O(explorer) ((char*)explorer)
    #define _OS(explorer) (std::string(explorer))
#endif
#endif

