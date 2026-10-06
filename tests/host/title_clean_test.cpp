#include "src/frontend/database/title_clean.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int failures = 0;

void expect(const char* stem, const char* title, const char* variant) {
    const rs::db::CleanTitle c = rs::db::cleanTitle(stem);
    if (c.title != title || c.variant != variant) {
        std::fprintf(stderr, "'%s'\n  got   title='%s' variant='%s'\n"
                             "  wants title='%s' variant='%s'\n",
                     stem, c.title.c_str(), c.variant.c_str(), title, variant);
        failures++;
    }
}
}  // namespace

int main() {
    /* The benchmark's own example. */
    expect("Pokemon - Crystal Version (USA, Europe) (Rev 1)",
           "Pok\xC3\xA9mon Crystal", "USA, Europe, Rev 1");
    expect("Pokemon - Red Version (USA, Europe) (SGB Enhanced)",
           "Pok\xC3\xA9mon Red", "USA, Europe, SGB Enhanced");
    expect("Tetris DX", "Tetris DX", "");

    /* Articles move to the front; " - " becomes a subtitle colon. */
    expect("Legend of Zelda, The - Link's Awakening DX (USA, Europe) (Rev 2)",
           "The Legend of Zelda: Link's Awakening DX", "USA, Europe, Rev 2");
    expect("Adventures of Lolo, The (USA)", "The Adventures of Lolo", "USA");
    expect("Super Mario Land 2 - 6 Golden Coins (World)",
           "Super Mario Land 2: 6 Golden Coins", "World");

    /* Noise: verified-dump marker, dump codes, language lists. */
    expect("Sonic the Hedgehog (USA, Europe) [!]", "Sonic the Hedgehog",
           "USA, Europe");
    expect("Game [b1] (Japan) [h1]", "Game", "Japan");
    expect("Game (En,Fr,De)", "Game", "");

    /* Hacks, translations, homebrew keep their tag so they stay findable. */
    expect("Super Mario Bros (Hack)", "Super Mario Bros", "Hack");
    expect("Final Fantasy III [T+Eng v1.1]", "Final Fantasy III", "T+Eng v1.1");
    expect("Cave Dave (Homebrew) (v1.0)", "Cave Dave", "Homebrew, v1.0");

    /* Multi-disc. */
    expect("Chrono Cross (USA) (Disc 2)", "Chrono Cross", "USA, Disc 2");

    /* Underscores, stray spacing, nested brackets. */
    expect("Metroid_II__Return_of_Samus (World)", "Metroid II Return of Samus",
           "World");
    expect("  Game   Name  ", "Game Name", "");
    expect("Game (USA (Rev A))", "Game", "USA (Rev A)");

    /* Never blank, never mangled: a name that is nothing but tags keeps the
     * raw stem; an unbalanced bracket stays literal text. */
    expect("(USA)", "(USA)", "");
    expect("Game (USA", "Game (USA", "");
    expect("", "", "");

    /* Non-ASCII passes through untouched. */
    expect("Pok\xC3\xA9mon Pinball (USA)", "Pok\xC3\xA9mon Pinball", "USA");
    expect("1942 (Japan)", "1942", "Japan");

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
