#pragma once
// Conceptual branch only: offline tests that run inside a Lab > Test Arsenal battle. See labtests.cpp.
#include <string>

namespace pdnet::labtests
{
    void Install();

    // Theatre: loads a film for the next Test Arsenal battle, which then plays it (every player driven by the film, the
    // dead camera's controls to watch). False if the film can't be read.
    bool QueueFilm(const std::wstring& file);
    // While a queued film's battle is being set up: the deck for battle slot `slot` from the film (false: none).
    bool FilmDeck(int slot, int16_t (&skills)[30]);
}
