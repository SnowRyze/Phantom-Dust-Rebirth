#pragma once
// C++/WinRT coroutines hand out their result only once: IAsyncOperation::GetResults moves it out, so a second call
// returns null. XSAPI's own operations can be read any number of times, and the game's multiplayer tasks poll an
// operation every frame and may read its result again - a null WriteSessionResult crashed the game when leaving a
// lobby (PDUWP.exe+0xA7083, FUN_14009c7c0). Repeatable() wraps an operation so every GetResults returns the same value.
#include <mutex>
#include <optional>
#include "weak.h"

namespace pdnet::xsapi
{
    template <typename T>
    struct RepeatableOperation : winrt::implements<RepeatableOperation<T>, winrt::Windows::Foundation::IAsyncOperation<T>, winrt::Windows::Foundation::IAsyncInfo>
    {
        using AsyncStatus = winrt::Windows::Foundation::AsyncStatus;
        using Handler = winrt::Windows::Foundation::AsyncOperationCompletedHandler<T>;

        explicit RepeatableOperation(winrt::Windows::Foundation::IAsyncOperation<T> inner) : m_inner(std::move(inner)) {}

        // Called once right after construction (needs a reference to itself).
        void Start()
        {
            m_inner.Completed([weak = SafeWeak<RepeatableOperation<T>>(*this)](auto const&, AsyncStatus status)
            {
                auto self = weak.get();
                if (!self) return;
                Handler handler;
                {
                    std::lock_guard guard(self->m_lock);
                    self->m_done = true;
                    handler = self->m_handler;
                }
                if (handler) handler(*self, status);
            });
        }

        void Completed(Handler const& handler)
        {
            bool done;
            {
                std::lock_guard guard(m_lock);
                if (m_handlerSet) throw winrt::hresult_illegal_delegate_assignment();
                m_handlerSet = true;
                m_handler = handler;
                done = m_done;
            }
            if (done && handler) handler(*this, Status());
        }
        Handler Completed()
        {
            std::lock_guard guard(m_lock);
            return m_handler;
        }

        T GetResults()
        {
            std::lock_guard guard(m_lock);
            if (!m_result) m_result = m_inner.GetResults(); // throws (and caches nothing) if the operation failed
            return *m_result;
        }

        uint32_t Id() { return m_inner.as<winrt::Windows::Foundation::IAsyncInfo>().Id(); }
        AsyncStatus Status() { return m_inner.as<winrt::Windows::Foundation::IAsyncInfo>().Status(); }
        winrt::hresult ErrorCode() { return m_inner.as<winrt::Windows::Foundation::IAsyncInfo>().ErrorCode(); }
        void Cancel() { m_inner.as<winrt::Windows::Foundation::IAsyncInfo>().Cancel(); }
        void Close() { m_inner.as<winrt::Windows::Foundation::IAsyncInfo>().Close(); }

    private:
        winrt::Windows::Foundation::IAsyncOperation<T> m_inner;
        std::mutex m_lock;
        Handler m_handler{ nullptr };
        bool m_handlerSet = false, m_done = false;
        std::optional<T> m_result;
    };

    template <typename T>
    winrt::Windows::Foundation::IAsyncOperation<T> Repeatable(winrt::Windows::Foundation::IAsyncOperation<T> operation)
    {
        auto wrapper = winrt::make_self<RepeatableOperation<T>>(std::move(operation));
        wrapper->Start();
        return wrapper.template as<winrt::Windows::Foundation::IAsyncOperation<T>>();
    }
}
