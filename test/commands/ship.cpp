#include <catch2/catch_all.hpp>
#include <spdlog/spdlog.h>

namespace chocobot
{
    class ship_command /* incomplete class is okay for using only static methods it seems */
    {
        public:
        static unsigned int ship(std::string word1, std::string word2);
    };
}

TEST_CASE( "ship command", "[command][ship]" ) {
    SECTION( "general ship tests ") {
        std::string word1 = GENERATE("schaf", "kuh", "schwein", "fisch", "ziege");
        std::string word2 = GENERATE("baum", "blatt", "grass", "teich", "wiese");
        
        SECTION( "shipping a word with itself yields 0%" ) {
            REQUIRE(chocobot::ship_command::ship(word1, word1) == 0);
            REQUIRE(chocobot::ship_command::ship(word2, word2) == 0);
        }
        SECTION( "shipping is case insensitive" ) {
            std::string upper1 = word1; std::transform(upper1.begin(), upper1.end(), upper1.begin(), [](char a){return (char)std::toupper(a);});
            std::string upper2 = word2; std::transform(upper2.begin(), upper2.end(), upper2.begin(), [](char a){return (char)std::toupper(a);});

            REQUIRE(chocobot::ship_command::ship(word1, upper1) == 0);
            REQUIRE(chocobot::ship_command::ship(word2, upper2) == 0);

            REQUIRE(chocobot::ship_command::ship(word1, upper2) == chocobot::ship_command::ship(word1, word2));
            REQUIRE(chocobot::ship_command::ship(upper1, upper2) == chocobot::ship_command::ship(word1, word2));
            REQUIRE(chocobot::ship_command::ship(upper1, word2) == chocobot::ship_command::ship(word1, word2));
        }
        SECTION( "shipping is commutative" ) {
            REQUIRE(chocobot::ship_command::ship(word1, word2) == chocobot::ship_command::ship(word2, word1));
        }
    }
    SECTION( "specific testss" ) {
        REQUIRE(chocobot::ship_command::ship("Femboys", "Weltherrschaft") >= 75);
    }
}
