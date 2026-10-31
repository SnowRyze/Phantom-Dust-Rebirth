#pragma once
// A weak reference to one of our own implementation objects.
//
// C++/WinRT 2.0.201201.7 (the version xsapi/gen is made with) has a bug in weak_ref<implementation>::get(): when the
// object is gone, Resolve gives a null interface and get_self turns it into "null minus an offset", a non-null
// garbage pointer, so `if (auto self = weak.get())` passes and the first AddRef reads address -8. Later versions check
// for null first. The multiplayer change events crashed that way when a second player joined a lobby: a
// MultiplayerService the game had released was still in the list. This keeps the weak reference on the object's
// default interface, whose get() returns null correctly, and goes to the implementation only after that check.
#include <winrt/base.h>

namespace pdnet::xsapi
{
    template <typename D>
    class SafeWeak
    {
    public:
        using Interface = winrt::default_interface<D>;

        SafeWeak() = default;
        explicit SafeWeak(D& self) : m_ref(self.get_strong().template as<Interface>()) {}

        winrt::com_ptr<D> get() const noexcept
        {
            winrt::com_ptr<D> result;
            if (auto object = m_ref.get()) result.copy_from(winrt::get_self<D>(object));
            return result;
        }

    private:
        winrt::weak_ref<Interface> m_ref;
    };
}
