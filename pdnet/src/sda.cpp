// Replacement for Windows.Networking.XboxLive (Secure Device Association over Teredo).
//
// A device address is a pdnet snapshot (IP + ports + random device id). Creating an endpoint pair runs a
// CONNECT/ACK handshake on the pdnet control port; afterwards the game's own UDP socket talks to the peer's
// real address directly. Quality-of-service measurements are real pings over the control channel.
#include "pch.h"
#include "sda.h"
#include "net.h"
#include "log.h"
#include "config.h"
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.XboxLive.h>

namespace wf = winrt::Windows::Foundation;
namespace wfc = winrt::Windows::Foundation::Collections;
namespace wss = winrt::Windows::Storage::Streams;
namespace xl = winrt::Windows::Networking::XboxLive;
using winrt::hstring;
using pdnet::net::Snapshot;

namespace pdnet
{
    std::vector<uint8_t> PublishedPayload();

    namespace
    {
        template <typename RuntimeClass, typename Impl, typename... Args>
        RuntimeClass Make(Args&&... args)
        {
            return winrt::make<Impl>(std::forward<Args>(args)...).template as<RuntimeClass>();
        }

        template <typename RuntimeClass, typename Impl>
        RuntimeClass As(const winrt::com_ptr<Impl>& impl)
        {
            return impl.template as<RuntimeClass>();
        }

        // Reads the snapshot out of any XboxLiveDeviceAddress (ours serialize to pdnet snapshots).
        bool SnapshotOf(const xl::XboxLiveDeviceAddress& address, Snapshot& out)
        {
            if (!address) return false;
            std::vector<uint8_t> bytes(64);
            uint32_t written = 0;
            try
            {
                address.GetSnapshotAsBytes(bytes, written);
            }
            catch (...)
            {
                return false;
            }
            return Snapshot::Parse(bytes.data(), written, out);
        }

        // ------------------------------------------------------------------ device address

        struct DeviceAddress : winrt::implements<DeviceAddress, xl::IXboxLiveDeviceAddress>
        {
            Snapshot m_snapshot;
            explicit DeviceAddress(Snapshot s) : m_snapshot(s) {}

            winrt::event_token SnapshotChanged(wf::TypedEventHandler<xl::XboxLiveDeviceAddress, wf::IInspectable> const&) { return {}; }
            void SnapshotChanged(winrt::event_token const&) noexcept {}

            hstring GetSnapshotAsBase64() const { return hstring{ net::Base64Encode(m_snapshot.Serialize()) }; }

            wss::IBuffer GetSnapshotAsBuffer() const
            {
                auto bytes = m_snapshot.Serialize();
                wss::Buffer buffer(static_cast<uint32_t>(bytes.size()));
                memcpy(buffer.data(), bytes.data(), bytes.size());
                buffer.Length(static_cast<uint32_t>(bytes.size()));
                return buffer;
            }

            void GetSnapshotAsBytes(winrt::array_view<uint8_t> buffer, uint32_t& written) const
            {
                auto bytes = m_snapshot.Serialize();
                written = static_cast<uint32_t>(bytes.size());
                if (buffer.size() < bytes.size()) throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
                memcpy(buffer.data(), bytes.data(), bytes.size());
            }

            int32_t Compare(xl::XboxLiveDeviceAddress const& other) const
            {
                Snapshot o;
                if (!SnapshotOf(other, o)) return 1;
                return m_snapshot.deviceId < o.deviceId ? -1 : m_snapshot.deviceId > o.deviceId ? 1 : 0;
            }

            bool IsValid() const { return true; }
            bool IsLocal() const { return m_snapshot.deviceId == net::Local().deviceId; }
            xl::XboxLiveNetworkAccessKind NetworkAccessKind() const { return xl::XboxLiveNetworkAccessKind::Open; }
        };

        xl::XboxLiveDeviceAddress MakeAddress(const Snapshot& s)
        {
            return As<xl::XboxLiveDeviceAddress>(winrt::make_self<DeviceAddress>(s));
        }

        xl::XboxLiveDeviceAddress LocalAddress()
        {
            static xl::XboxLiveDeviceAddress local = MakeAddress(net::Local());
            return local;
        }

        xl::XboxLiveDeviceAddress ParseAddress(const std::vector<uint8_t>& bytes)
        {
            Snapshot s;
            if (!Snapshot::Parse(bytes.data(), bytes.size(), s))
            {
                Log(L"sda: rejected a device address that is not a pdnet snapshot (%zu bytes) - is the other player running pdnet?", bytes.size());
                throw winrt::hresult_invalid_argument(L"not a pdnet device address");
            }
            return s.deviceId == net::Local().deviceId ? LocalAddress() : MakeAddress(s);
        }

        struct DeviceAddressStatics : winrt::implements<DeviceAddressStatics, wf::IActivationFactory, xl::IXboxLiveDeviceAddressStatics>
        {
            wf::IInspectable ActivateInstance() const { throw winrt::hresult_not_implemented(); }

            xl::XboxLiveDeviceAddress CreateFromSnapshotBase64(hstring const& base64) const
            {
                std::vector<uint8_t> bytes;
                if (!net::Base64Decode(base64, bytes)) throw winrt::hresult_invalid_argument();
                return ParseAddress(bytes);
            }

            xl::XboxLiveDeviceAddress CreateFromSnapshotBuffer(wss::IBuffer const& buffer) const
            {
                return ParseAddress(std::vector<uint8_t>(buffer.data(), buffer.data() + buffer.Length()));
            }

            xl::XboxLiveDeviceAddress CreateFromSnapshotBytes(winrt::array_view<uint8_t const> buffer) const
            {
                return ParseAddress(std::vector<uint8_t>(buffer.begin(), buffer.end()));
            }

            xl::XboxLiveDeviceAddress GetLocal() const { return LocalAddress(); }
            uint32_t MaxSnapshotBytesSize() const { return 64; }
        };

        // ------------------------------------------------------------------ endpoint pairs

        struct Template;

        struct StateChangedArgs : winrt::implements<StateChangedArgs, xl::IXboxLiveEndpointPairStateChangedEventArgs>
        {
            xl::XboxLiveEndpointPairState m_old, m_new;
            StateChangedArgs(xl::XboxLiveEndpointPairState o, xl::XboxLiveEndpointPairState n) : m_old(o), m_new(n) {}
            xl::XboxLiveEndpointPairState OldState() const { return m_old; }
            xl::XboxLiveEndpointPairState NewState() const { return m_new; }
        };

        struct EndpointPair : winrt::implements<EndpointPair, xl::IXboxLiveEndpointPair>
        {
            winrt::com_ptr<Template> m_template;
            Snapshot m_remote;
            std::array<uint8_t, 16> m_remoteIp; // the address the peer's packets actually come from
            std::atomic<xl::XboxLiveEndpointPairState> m_state{ xl::XboxLiveEndpointPairState::Ready };
            winrt::event<wf::TypedEventHandler<xl::XboxLiveEndpointPair, xl::XboxLiveEndpointPairStateChangedEventArgs>> m_stateChanged;

            EndpointPair(winrt::com_ptr<Template> t, Snapshot remote, std::array<uint8_t, 16> observedIp)
                : m_template(std::move(t)), m_remote(remote), m_remoteIp(observedIp) {}

            winrt::event_token StateChanged(wf::TypedEventHandler<xl::XboxLiveEndpointPair, xl::XboxLiveEndpointPairStateChangedEventArgs> const& h) { return m_stateChanged.add(h); }
            void StateChanged(winrt::event_token const& token) noexcept { m_stateChanged.remove(token); }

            void SetState(xl::XboxLiveEndpointPairState next)
            {
                auto old = m_state.exchange(next);
                if (old == next) return;
                Log(L"sda: pair %016llX state %d -> %d", m_remote.deviceId, static_cast<int>(old), static_cast<int>(next));
                try
                {
                    m_stateChanged(get_strong().as<xl::XboxLiveEndpointPair>(), Make<xl::XboxLiveEndpointPairStateChangedEventArgs, StateChangedArgs>(old, next));
                }
                catch (...)
                {
                    Log(L"sda: StateChanged handler threw 0x%08X", winrt::to_hresult());
                }
            }

            wf::IAsyncAction DeleteAsync();

            static void WriteSockAddr(winrt::array_view<uint8_t> out, const sockaddr_in6& a)
            {
                if (out.size() < sizeof(a)) throw winrt::hresult_invalid_argument(L"socket address buffer too small");
                std::fill(out.begin(), out.end(), uint8_t{ 0 });
                memcpy(out.data(), &a, sizeof(a));
            }

            void GetRemoteSocketAddressBytes(winrt::array_view<uint8_t> out) const { WriteSockAddr(out, net::GameSockAddr(m_remoteIp, m_remote.gamePort)); }
            void GetLocalSocketAddressBytes(winrt::array_view<uint8_t> out) const { WriteSockAddr(out, net::GameSockAddr(net::Local().ip, net::Local().gamePort)); }

            xl::XboxLiveEndpointPairState State() const { return m_state; }
            xl::XboxLiveEndpointPairTemplate Template() const;
            xl::XboxLiveDeviceAddress RemoteDeviceAddress() const { return MakeAddress(m_remote); }
            winrt::Windows::Networking::HostName RemoteHostName() const { return winrt::Windows::Networking::HostName(net::IpToString(m_remoteIp)); }
            hstring RemotePort() const { return winrt::to_hstring(m_remote.gamePort); }
            winrt::Windows::Networking::HostName LocalHostName() const { return winrt::Windows::Networking::HostName(net::IpToString(net::Local().ip)); }
            hstring LocalPort() const { return winrt::to_hstring(net::Local().gamePort); }

            bool Matches(const std::array<uint8_t, 16>& ip, uint16_t port) const { return ip == m_remoteIp && port == m_remote.gamePort; }
        };

        struct CreationResult : winrt::implements<CreationResult, xl::IXboxLiveEndpointPairCreationResult>
        {
            xl::XboxLiveDeviceAddress m_address;
            xl::XboxLiveEndpointPairCreationStatus m_status;
            bool m_existing;
            xl::XboxLiveEndpointPair m_pair;
            CreationResult(xl::XboxLiveDeviceAddress a, xl::XboxLiveEndpointPairCreationStatus s, bool e, xl::XboxLiveEndpointPair p)
                : m_address(std::move(a)), m_status(s), m_existing(e), m_pair(std::move(p)) {}
            xl::XboxLiveDeviceAddress DeviceAddress() const { return m_address; }
            xl::XboxLiveEndpointPairCreationStatus Status() const { return m_status; }
            bool IsExistingPathEvaluation() const { return m_existing; }
            xl::XboxLiveEndpointPair EndpointPair() const { return m_pair; }
        };

        struct InboundArgs : winrt::implements<InboundArgs, xl::IXboxLiveInboundEndpointPairCreatedEventArgs>
        {
            xl::XboxLiveEndpointPair m_pair;
            explicit InboundArgs(xl::XboxLiveEndpointPair p) : m_pair(std::move(p)) {}
            xl::XboxLiveEndpointPair EndpointPair() const { return m_pair; }
        };

        std::mutex g_lock; // guards every template's pair list

        struct Template : winrt::implements<Template, xl::IXboxLiveEndpointPairTemplate>
        {
            std::wstring m_name;
            std::vector<winrt::com_ptr<EndpointPair>> m_pairs;
            winrt::event<wf::TypedEventHandler<xl::XboxLiveEndpointPairTemplate, xl::XboxLiveInboundEndpointPairCreatedEventArgs>> m_inbound;

            explicit Template(std::wstring name) : m_name(std::move(name)) {}

            winrt::event_token InboundEndpointPairCreated(wf::TypedEventHandler<xl::XboxLiveEndpointPairTemplate, xl::XboxLiveInboundEndpointPairCreatedEventArgs> const& h) { return m_inbound.add(h); }
            void InboundEndpointPairCreated(winrt::event_token const& token) noexcept { m_inbound.remove(token); }

            winrt::com_ptr<EndpointPair> FindLocked(uint64_t deviceId) const
            {
                for (auto& p : m_pairs)
                    if (p->m_remote.deviceId == deviceId) return p;
                return nullptr;
            }

            // Returns the pair for a peer, creating it if needed. `created` reports whether it is new.
            winrt::com_ptr<EndpointPair> GetOrAdd(const Snapshot& remote, const std::array<uint8_t, 16>& observedIp, bool& created)
            {
                std::lock_guard guard(g_lock);
                if (auto existing = FindLocked(remote.deviceId))
                {
                    existing->m_remoteIp = observedIp;
                    created = false;
                    return existing;
                }
                auto pair = winrt::make_self<EndpointPair>(get_strong(), remote, observedIp);
                m_pairs.push_back(pair);
                created = true;
                return pair;
            }

            void Remove(const EndpointPair* pair)
            {
                std::lock_guard guard(g_lock);
                std::erase_if(m_pairs, [&](auto& p) { return p.get() == pair; });
            }

            void RaiseInbound(const winrt::com_ptr<EndpointPair>& pair)
            {
                try
                {
                    m_inbound(get_strong().as<xl::XboxLiveEndpointPairTemplate>(), Make<xl::XboxLiveInboundEndpointPairCreatedEventArgs, InboundArgs>(As<xl::XboxLiveEndpointPair>(pair)));
                }
                catch (...)
                {
                    Log(L"sda: InboundEndpointPairCreated handler threw 0x%08X", winrt::to_hresult());
                }
            }

            wf::IAsyncOperation<xl::XboxLiveEndpointPairCreationResult> CreateEndpointPairAsync(xl::XboxLiveDeviceAddress address)
            {
                auto strong = get_strong();
                co_await winrt::resume_background();

                using Status = xl::XboxLiveEndpointPairCreationStatus;
                Snapshot remote;
                if (!SnapshotOf(address, remote))
                {
                    co_return Make<xl::XboxLiveEndpointPairCreationResult, CreationResult>(address, Status::NoCompatibleNetworkPaths, false, nullptr);
                }
                if (remote.deviceId == net::Local().deviceId)
                {
                    Log(L"sda: refusing endpoint pair to ourselves");
                    co_return Make<xl::XboxLiveEndpointPairCreationResult, CreationResult>(address, Status::NoCompatibleNetworkPaths, false, nullptr);
                }
                {
                    std::lock_guard guard(g_lock);
                    if (auto existing = FindLocked(remote.deviceId); existing && existing->m_state == xl::XboxLiveEndpointPairState::Ready)
                    {
                        co_return Make<xl::XboxLiveEndpointPairCreationResult, CreationResult>(address, Status::Succeeded, true, As<xl::XboxLiveEndpointPair>(existing));
                    }
                }

                Log(L"sda: connecting to %s (template %s)", remote.Describe().c_str(), m_name.c_str());
                auto body = net::Local().Serialize();
                std::string name = winrt::to_string(m_name);
                body.push_back(static_cast<uint8_t>(name.size()));
                body.insert(body.end(), name.begin(), name.end());

                net::Message reply;
                if (!net::Request(remote, net::MsgType::Connect, body, 10000, 250, reply))
                {
                    Log(L"sda: connect to %s TIMED OUT - check the other PC is running pdnet and UDP %u/%u are reachable", remote.Describe().c_str(), remote.gamePort, remote.controlPort);
                    co_return Make<xl::XboxLiveEndpointPairCreationResult, CreationResult>(address, Status::TimedOut, false, nullptr);
                }

                std::array<uint8_t, 16> observed;
                memcpy(observed.data(), &reply.from.sin6_addr, 16);
                bool created;
                auto pair = GetOrAdd(remote, observed, created);
                Log(L"sda: connected to %s, packets go to %s:%u", remote.Describe().c_str(), net::IpToString(observed).c_str(), remote.gamePort);
                co_return Make<xl::XboxLiveEndpointPairCreationResult, CreationResult>(address, Status::Succeeded, !created, As<xl::XboxLiveEndpointPair>(pair));
            }

            wf::IAsyncOperation<xl::XboxLiveEndpointPairCreationResult> CreateEndpointPairAsync(xl::XboxLiveDeviceAddress address, xl::XboxLiveEndpointPairCreationBehaviors)
            {
                return CreateEndpointPairAsync(address);
            }

            wf::IAsyncOperation<xl::XboxLiveEndpointPairCreationResult> CreateEndpointPairForPortsAsync(xl::XboxLiveDeviceAddress address, hstring, hstring)
            {
                return CreateEndpointPairAsync(address);
            }

            wf::IAsyncOperation<xl::XboxLiveEndpointPairCreationResult> CreateEndpointPairForPortsAsync(xl::XboxLiveDeviceAddress address, hstring, hstring, xl::XboxLiveEndpointPairCreationBehaviors)
            {
                return CreateEndpointPairAsync(address);
            }

            hstring Name() const { return hstring{ m_name }; }
            xl::XboxLiveSocketKind SocketKind() const { return xl::XboxLiveSocketKind::Datagram; }
            uint16_t InitiatorBoundPortRangeLower() const { return net::Local().gamePort; }
            uint16_t InitiatorBoundPortRangeUpper() const { return net::Local().gamePort; }
            uint16_t AcceptorBoundPortRangeLower() const { return net::Local().gamePort; }
            uint16_t AcceptorBoundPortRangeUpper() const { return net::Local().gamePort; }

            wfc::IVectorView<xl::XboxLiveEndpointPair> EndpointPairs() const
            {
                std::lock_guard guard(g_lock);
                std::vector<xl::XboxLiveEndpointPair> pairs;
                for (auto& p : m_pairs) pairs.push_back(As<xl::XboxLiveEndpointPair>(p));
                return winrt::single_threaded_vector(std::move(pairs)).GetView();
            }
        };

        xl::XboxLiveEndpointPairTemplate EndpointPair::Template() const
        {
            return As<xl::XboxLiveEndpointPairTemplate>(m_template);
        }

        wf::IAsyncAction EndpointPair::DeleteAsync()
        {
            auto strong = get_strong();
            co_await winrt::resume_background();
            SetState(xl::XboxLiveEndpointPairState::DeletingLocally);
            auto body = net::Local().Serialize();
            std::string name = winrt::to_string(m_template->m_name);
            body.push_back(static_cast<uint8_t>(name.size()));
            body.insert(body.end(), name.begin(), name.end());
            net::Send(m_remote, net::MsgType::Delete, net::NewNonce(), body);
            m_template->Remove(this);
            SetState(xl::XboxLiveEndpointPairState::Deleted);
        }

        std::mutex g_templatesLock;
        std::map<std::wstring, winrt::com_ptr<Template>> g_templates;

        winrt::com_ptr<Template> GetTemplate(const std::wstring& name)
        {
            std::lock_guard guard(g_templatesLock);
            auto& t = g_templates[name];
            if (!t)
            {
                t = winrt::make_self<Template>(name);
                Log(L"sda: template '%s' (UDP %u)", name.c_str(), net::Local().gamePort);
            }
            return t;
        }

        // Unsolicited control messages: inbound connects, remote deletes, pings.
        void OnControlMessage(const net::Message& m)
        {
            auto readName = [&](size_t offset) -> std::wstring
            {
                if (m.body.size() <= offset) return {};
                size_t len = (std::min)(static_cast<size_t>(m.body[offset]), m.body.size() - offset - 1);
                return winrt::to_hstring(std::string(m.body.begin() + offset + 1, m.body.begin() + offset + 1 + len)).c_str();
            };

            if (m.type == net::MsgType::Ping)
            {
                auto payload = PublishedPayload();
                std::vector<uint8_t> body{ static_cast<uint8_t>(payload.size() >> 8), static_cast<uint8_t>(payload.size() & 0xff) };
                body.insert(body.end(), payload.begin(), payload.end());
                net::SendTo(m.from, net::MsgType::Pong, m.nonce, body);
                return;
            }

            Snapshot remote;
            if (!Snapshot::Parse(m.body.data(), m.body.size(), remote)) return;
            std::array<uint8_t, 16> observed;
            memcpy(observed.data(), &m.from.sin6_addr, 16);

            if (m.type == net::MsgType::Connect)
            {
                auto t = GetTemplate(readName(Snapshot::kSize));
                bool created;
                auto pair = t->GetOrAdd(remote, observed, created);
                net::SendTo(m.from, net::MsgType::Ack, m.nonce, net::Local().Serialize());
                if (created)
                {
                    Log(L"sda: inbound connection from %s (seen as %s)", remote.Describe().c_str(), net::IpToString(observed).c_str());
                    t->RaiseInbound(pair);
                }
            }
            else if (m.type == net::MsgType::Delete)
            {
                auto t = GetTemplate(readName(Snapshot::kSize));
                winrt::com_ptr<EndpointPair> pair;
                {
                    std::lock_guard guard(g_lock);
                    pair = t->FindLocked(remote.deviceId);
                }
                if (pair)
                {
                    Log(L"sda: %s closed the connection", remote.Describe().c_str());
                    pair->SetState(xl::XboxLiveEndpointPairState::RemoteEndpointTerminating);
                    t->Remove(pair.get());
                    pair->SetState(xl::XboxLiveEndpointPairState::Deleted);
                }
            }
        }

        void EnsureNetwork()
        {
            net::Start(OnControlMessage);
        }

        struct TemplateStatics : winrt::implements<TemplateStatics, wf::IActivationFactory, xl::IXboxLiveEndpointPairTemplateStatics>
        {
            wf::IInspectable ActivateInstance() const { throw winrt::hresult_not_implemented(); }

            xl::XboxLiveEndpointPairTemplate GetTemplateByName(hstring const& name) const
            {
                return As<xl::XboxLiveEndpointPairTemplate>(GetTemplate(std::wstring{ name }));
            }

            wfc::IVectorView<xl::XboxLiveEndpointPairTemplate> Templates() const
            {
                std::lock_guard guard(g_templatesLock);
                std::vector<xl::XboxLiveEndpointPairTemplate> list;
                for (auto& [name, t] : g_templates) list.push_back(As<xl::XboxLiveEndpointPairTemplate>(t));
                return winrt::single_threaded_vector(std::move(list)).GetView();
            }
        };

        struct PairStatics : winrt::implements<PairStatics, wf::IActivationFactory, xl::IXboxLiveEndpointPairStatics>
        {
            wf::IInspectable ActivateInstance() const { throw winrt::hresult_not_implemented(); }

            static xl::XboxLiveEndpointPair Find(const std::array<uint8_t, 16>& ip, uint16_t port)
            {
                std::lock_guard guard(g_templatesLock);
                std::lock_guard pairsGuard(g_lock);
                for (auto& [name, t] : g_templates)
                    for (auto& p : t->m_pairs)
                        if (p->Matches(ip, port)) return As<xl::XboxLiveEndpointPair>(p);
                return nullptr;
            }

            xl::XboxLiveEndpointPair FindEndpointPairBySocketAddressBytes(winrt::array_view<uint8_t const>, winrt::array_view<uint8_t const> remote) const
            {
                std::array<uint8_t, 16> ip{};
                uint16_t port = 0;
                if (remote.size() >= sizeof(sockaddr_in6) && reinterpret_cast<const sockaddr_in6*>(remote.data())->sin6_family == AF_INET6)
                {
                    auto a = reinterpret_cast<const sockaddr_in6*>(remote.data());
                    memcpy(ip.data(), &a->sin6_addr, 16);
                    port = ntohs(a->sin6_port);
                }
                else if (remote.size() >= sizeof(sockaddr_in) && reinterpret_cast<const sockaddr_in*>(remote.data())->sin_family == AF_INET)
                {
                    auto a = reinterpret_cast<const sockaddr_in*>(remote.data());
                    ip[10] = ip[11] = 0xff;
                    memcpy(&ip[12], &a->sin_addr, 4);
                    port = ntohs(a->sin_port);
                }
                auto pair = Find(ip, port);
                if (!pair)
                {
                    static std::atomic<int> misses;
                    if (misses++ < 20) Log(L"sda: packet from unknown peer %s:%u", net::IpToString(ip).c_str(), port);
                }
                return pair;
            }

            xl::XboxLiveEndpointPair FindEndpointPairByHostNamesAndPorts(winrt::Windows::Networking::HostName const&, hstring const&, winrt::Windows::Networking::HostName const& remoteHost, hstring const& remotePort) const
            {
                std::array<uint8_t, 16> ip{};
                in_addr v4;
                if (InetPtonW(AF_INET, remoteHost.CanonicalName().c_str(), &v4) == 1)
                {
                    ip[10] = ip[11] = 0xff;
                    memcpy(&ip[12], &v4, 4);
                }
                else
                {
                    InetPtonW(AF_INET6, remoteHost.CanonicalName().c_str(), ip.data());
                }
                return Find(ip, static_cast<uint16_t>(std::stoul(std::wstring{ remotePort })));
            }
        };

        // ------------------------------------------------------------------ quality of service

        std::mutex g_payloadLock;
        std::vector<uint8_t> g_payload;
        uint32_t g_maxProbeConnections = 8;
        bool g_outboundConstrained = false, g_inboundConstrained = false;
        constexpr uint32_t kMaxPayload = 4096;
        constexpr uint64_t kAssumedBitsPerSecond = 20'000'000;

        struct MetricResult : winrt::implements<MetricResult, xl::IXboxLiveQualityOfServiceMetricResult>
        {
            xl::XboxLiveQualityOfServiceMeasurementStatus m_status;
            xl::XboxLiveDeviceAddress m_address;
            xl::XboxLiveQualityOfServiceMetric m_metric;
            uint64_t m_value;
            MetricResult(xl::XboxLiveQualityOfServiceMeasurementStatus s, xl::XboxLiveDeviceAddress a, xl::XboxLiveQualityOfServiceMetric m, uint64_t v)
                : m_status(s), m_address(std::move(a)), m_metric(m), m_value(v) {}
            xl::XboxLiveQualityOfServiceMeasurementStatus Status() const { return m_status; }
            xl::XboxLiveDeviceAddress DeviceAddress() const { return m_address; }
            xl::XboxLiveQualityOfServiceMetric Metric() const { return m_metric; }
            uint64_t Value() const { return m_value; }
        };

        struct PayloadResult : winrt::implements<PayloadResult, xl::IXboxLiveQualityOfServicePrivatePayloadResult>
        {
            xl::XboxLiveQualityOfServiceMeasurementStatus m_status;
            xl::XboxLiveDeviceAddress m_address;
            wss::IBuffer m_value;
            PayloadResult(xl::XboxLiveQualityOfServiceMeasurementStatus s, xl::XboxLiveDeviceAddress a, wss::IBuffer v)
                : m_status(s), m_address(std::move(a)), m_value(std::move(v)) {}
            xl::XboxLiveQualityOfServiceMeasurementStatus Status() const { return m_status; }
            xl::XboxLiveDeviceAddress DeviceAddress() const { return m_address; }
            wss::IBuffer Value() const { return m_value; }
        };

        struct Measurement : winrt::implements<Measurement, xl::IXboxLiveQualityOfServiceMeasurement>
        {
            wfc::IVector<xl::XboxLiveQualityOfServiceMetric> m_metrics = winrt::single_threaded_vector<xl::XboxLiveQualityOfServiceMetric>();
            wfc::IVector<xl::XboxLiveDeviceAddress> m_devices = winrt::single_threaded_vector<xl::XboxLiveDeviceAddress>();
            bool m_requestPayloads = false;
            uint32_t m_timeoutMs = 2000;
            uint32_t m_probes = 0; // 0 = config default
            std::atomic<uint32_t> m_pending{ 0 };
            std::mutex m_lock;
            std::vector<xl::XboxLiveQualityOfServiceMetricResult> m_results;
            std::vector<xl::XboxLiveQualityOfServicePrivatePayloadResult> m_payloads;

            wf::IAsyncAction MeasureAsync()
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                using QStatus = xl::XboxLiveQualityOfServiceMeasurementStatus;
                using Metric = xl::XboxLiveQualityOfServiceMetric;

                std::vector<xl::XboxLiveDeviceAddress> devices(m_devices.Size(), nullptr);
                m_devices.GetMany(0, devices);
                std::vector<Metric> metrics(m_metrics.Size());
                m_metrics.GetMany(0, metrics);
                uint32_t probes = m_probes ? m_probes : GetConfig().qosProbes;
                m_pending = static_cast<uint32_t>(devices.size() * metrics.size());

                std::vector<xl::XboxLiveQualityOfServiceMetricResult> results;
                std::vector<xl::XboxLiveQualityOfServicePrivatePayloadResult> payloads;
                for (auto& device : devices)
                {
                    Snapshot s;
                    std::vector<uint64_t> rtts;
                    std::vector<uint8_t> payload;
                    if (SnapshotOf(device, s))
                    {
                        uint32_t perProbe = (std::max)(100u, m_timeoutMs / probes);
                        for (uint32_t i = 0; i < probes; ++i)
                        {
                            net::Message pong;
                            auto start = std::chrono::steady_clock::now();
                            if (net::Request(s, net::MsgType::Ping, {}, perProbe, perProbe, pong))
                            {
                                rtts.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
                                if (pong.body.size() >= 2)
                                {
                                    size_t len = (std::min)(static_cast<size_t>(pong.body[0] << 8 | pong.body[1]), pong.body.size() - 2);
                                    payload.assign(pong.body.begin() + 2, pong.body.begin() + 2 + len);
                                }
                            }
                        }
                    }
                    QStatus status = rtts.empty() ? QStatus::TimedOut : QStatus::Succeeded;
                    uint64_t sum = 0, lo = UINT64_MAX, hi = 0;
                    for (auto r : rtts) { sum += r; lo = (std::min)(lo, r); hi = (std::max)(hi, r); }
                    uint64_t avg = rtts.empty() ? 0 : sum / rtts.size();
                    Log(L"qos: %s -> %zu/%u replies, avg %llu ms", s.Describe().c_str(), rtts.size(), probes, avg);

                    for (auto metric : metrics)
                    {
                        uint64_t value = 0;
                        switch (metric)
                        {
                        case Metric::AverageLatencyInMilliseconds: value = avg; break;
                        case Metric::MinLatencyInMilliseconds: value = rtts.empty() ? 0 : lo; break;
                        case Metric::MaxLatencyInMilliseconds: value = hi; break;
                        default: value = rtts.empty() ? 0 : kAssumedBitsPerSecond; break;
                        }
                        results.push_back(Make<xl::XboxLiveQualityOfServiceMetricResult, MetricResult>(status, device, metric, value));
                        --m_pending;
                    }
                    if (m_requestPayloads)
                    {
                        wss::Buffer buffer(static_cast<uint32_t>(payload.size()));
                        if (!payload.empty()) memcpy(buffer.data(), payload.data(), payload.size());
                        buffer.Length(static_cast<uint32_t>(payload.size()));
                        payloads.push_back(Make<xl::XboxLiveQualityOfServicePrivatePayloadResult, PayloadResult>(status, device, buffer));
                    }
                }
                std::lock_guard guard(m_lock);
                m_results = std::move(results);
                m_payloads = std::move(payloads);
                m_pending = 0;
            }

            wfc::IVectorView<xl::XboxLiveQualityOfServiceMetricResult> Filter(std::function<bool(const xl::XboxLiveQualityOfServiceMetricResult&)> keep)
            {
                std::lock_guard guard(m_lock);
                std::vector<xl::XboxLiveQualityOfServiceMetricResult> out;
                for (auto& r : m_results)
                    if (keep(r)) out.push_back(r);
                return winrt::single_threaded_vector(std::move(out)).GetView();
            }

            static bool Same(const xl::XboxLiveDeviceAddress& a, const xl::XboxLiveDeviceAddress& b)
            {
                Snapshot x, y;
                return SnapshotOf(a, x) && SnapshotOf(b, y) && x.deviceId == y.deviceId;
            }

            wfc::IVectorView<xl::XboxLiveQualityOfServiceMetricResult> GetMetricResultsForDevice(xl::XboxLiveDeviceAddress const& device)
            {
                return Filter([&](auto& r) { return Same(r.DeviceAddress(), device); });
            }

            wfc::IVectorView<xl::XboxLiveQualityOfServiceMetricResult> GetMetricResultsForMetric(xl::XboxLiveQualityOfServiceMetric const& metric)
            {
                return Filter([&](auto& r) { return r.Metric() == metric; });
            }

            xl::XboxLiveQualityOfServiceMetricResult GetMetricResult(xl::XboxLiveDeviceAddress const& device, xl::XboxLiveQualityOfServiceMetric const& metric)
            {
                auto view = Filter([&](auto& r) { return r.Metric() == metric && Same(r.DeviceAddress(), device); });
                return view.Size() ? view.GetAt(0) : nullptr;
            }

            xl::XboxLiveQualityOfServicePrivatePayloadResult GetPrivatePayloadResult(xl::XboxLiveDeviceAddress const& device)
            {
                std::lock_guard guard(m_lock);
                for (auto& p : m_payloads)
                    if (Same(p.DeviceAddress(), device)) return p;
                return nullptr;
            }

            wfc::IVector<xl::XboxLiveQualityOfServiceMetric> Metrics() const { return m_metrics; }
            wfc::IVector<xl::XboxLiveDeviceAddress> DeviceAddresses() const { return m_devices; }
            bool ShouldRequestPrivatePayloads() const { return m_requestPayloads; }
            void ShouldRequestPrivatePayloads(bool v) { m_requestPayloads = v; }
            uint32_t TimeoutInMilliseconds() const { return m_timeoutMs; }
            void TimeoutInMilliseconds(uint32_t v) { m_timeoutMs = v; }
            uint32_t NumberOfProbesToAttempt() const { return m_probes ? m_probes : GetConfig().qosProbes; }
            void NumberOfProbesToAttempt(uint32_t v) { m_probes = v; }
            uint32_t NumberOfResultsPending() const { return m_pending; }

            wfc::IVectorView<xl::XboxLiveQualityOfServiceMetricResult> MetricResults()
            {
                return Filter([](auto&) { return true; });
            }

            wfc::IVectorView<xl::XboxLiveQualityOfServicePrivatePayloadResult> PrivatePayloadResults()
            {
                std::lock_guard guard(m_lock);
                return winrt::single_threaded_vector(std::vector(m_payloads)).GetView();
            }
        };

        struct MeasurementFactory : winrt::implements<MeasurementFactory, wf::IActivationFactory, xl::IXboxLiveQualityOfServiceMeasurementStatics>
        {
            wf::IInspectable ActivateInstance() const { return winrt::make<Measurement>(); }

            void PublishPrivatePayloadBytes(winrt::array_view<uint8_t const> payload) const
            {
                if (payload.size() > kMaxPayload) throw winrt::hresult_invalid_argument();
                std::lock_guard guard(g_payloadLock);
                g_payload.assign(payload.begin(), payload.end());
            }

            void ClearPrivatePayload() const
            {
                std::lock_guard guard(g_payloadLock);
                g_payload.clear();
            }

            uint32_t MaxSimultaneousProbeConnections() const { return g_maxProbeConnections; }
            void MaxSimultaneousProbeConnections(uint32_t v) const { g_maxProbeConnections = v; }
            bool IsSystemOutboundBandwidthConstrained() const { return g_outboundConstrained; }
            void IsSystemOutboundBandwidthConstrained(bool v) const { g_outboundConstrained = v; }
            bool IsSystemInboundBandwidthConstrained() const { return g_inboundConstrained; }
            void IsSystemInboundBandwidthConstrained(bool v) const { g_inboundConstrained = v; }

            wss::IBuffer PublishedPrivatePayload() const
            {
                auto payload = PublishedPayload();
                wss::Buffer buffer(static_cast<uint32_t>(payload.size()));
                if (!payload.empty()) memcpy(buffer.data(), payload.data(), payload.size());
                buffer.Length(static_cast<uint32_t>(payload.size()));
                return buffer;
            }

            void PublishedPrivatePayload(wss::IBuffer const& value) const
            {
                std::lock_guard guard(g_payloadLock);
                g_payload.assign(value.data(), value.data() + value.Length());
            }

            uint32_t MaxPrivatePayloadSize() const { return kMaxPayload; }
        };
    }

    std::vector<uint8_t> PublishedPayload()
    {
        std::lock_guard guard(g_payloadLock);
        return g_payload;
    }

    wf::IActivationFactory GetXboxLiveNetworkingFactory(std::wstring_view cls)
    {
        constexpr std::wstring_view ns = L"Windows.Networking.XboxLive.";
        if (cls.substr(0, ns.size()) != ns || GetConfig().teredo) return nullptr; // teredo mode: Windows handles it
        auto name = cls.substr(ns.size());

        EnsureNetwork();
        static auto addresses = winrt::make<DeviceAddressStatics>();
        static auto templates = winrt::make<TemplateStatics>();
        static auto pairs = winrt::make<PairStatics>();
        static auto qos = winrt::make<MeasurementFactory>();
        if (name == L"XboxLiveDeviceAddress") return addresses;
        if (name == L"XboxLiveEndpointPairTemplate") return templates;
        if (name == L"XboxLiveEndpointPair") return pairs;
        if (name == L"XboxLiveQualityOfServiceMeasurement") return qos;
        return nullptr;
    }
}
