#pragma once
// Conceptual branch only: offline tests that run inside a Lab > Test Arsenal battle. See labtests.cpp.
#include <string>

namespace pdnet::labtests
{
    void Install();

    // Theatre: loads a film for the next Test Arsenal battle, which then plays it (every player driven by the film, the
    // dead camera's controls to watch). False if the film can't be read.
    bool QueueFilm(const std::wstring& file);
}
