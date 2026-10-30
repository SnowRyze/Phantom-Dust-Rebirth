#pragma once
#include "Privacy/PermissionIdConstants.g.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    struct PermissionIdConstants : PermissionIdConstantsT<PermissionIdConstants>
    {
        PermissionIdConstants() = default;

        static hstring CommunicateUsingText();
        static hstring CommunicateUsingVideo();
        static hstring CommunicateUsingVoice();
        static hstring ViewTargetProfile();
        static hstring ViewTargetGameHistory();
        static hstring ViewTargetVideoHistory();
        static hstring ViewTargetMusicHistory();
        static hstring ViewTargetExerciseInfo();
        static hstring ViewTargetPresence();
        static hstring ViewTargetVideoStatus();
        static hstring ViewTargetMusicStatus();
        static hstring PlayMultiplayer();
        static hstring BroadcastWithTwitch();
        static hstring ViewTargetUserCreatedContent();
    };
}
namespace winrt::Microsoft::Xbox::Services::Privacy::factory_implementation
{
    struct PermissionIdConstants : PermissionIdConstantsT<PermissionIdConstants, implementation::PermissionIdConstants>
    {
    };
}
