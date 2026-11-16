#pragma once
// The dead camera and Theatre film playback (made on the Conceptual branch). See labtests.cpp.
#include <string>

namespace pdnet::labtests
{
    void Install();

    // Theatre: loads a film for the next Test Arsenal battle, which then plays it (every player driven by the film, the
    // dead camera's controls to watch). False if the film can't be read.
    bool QueueFilm(const std::wstring& file);
    // While a queued film's battle is being set up: the deck for battle slot `slot` from the film (false: none).
    bool FilmDeck(int slot, int16_t (&skills)[30]);
    // While a queued film's battle is being set up: how many battle slots it needs (its highest player's slot + 1, at
    // least 2; each film player keeps their own slot), and a slot's character (-1: not recorded). 0 when no film.
    int FilmPlayers();
    int FilmCharacter(int slot);
}
