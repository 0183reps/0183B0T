#include "../pch.h"

#include <d3d9.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "MinHook.h"
#include "chams.h"

namespace
{
    // =========================================================
    // Proven x64 iw5mp.exe RVAs
    // =========================================================

    constexpr std::uintptr_t kAddDObjSurfacesRva =
        0x1FAD50;

    constexpr std::uintptr_t kPackedSurfaceConsumerRva =
        0x1AAB20;

    constexpr std::uintptr_t kDrawIndexedPrimitiveRva =
        0x2039A0;


    // =========================================================
    // Scene / entity layout
    // =========================================================

    constexpr std::uintptr_t kSceneEntityOffset =
        0x80;

    constexpr std::uintptr_t kEntityTypeOffset =
        0xDC;

    constexpr std::uintptr_t kEntityClientIndexOffset =
        0x168;


    // =========================================================
    // Team data
    // =========================================================

    // Proven from:
    //
    // dword_1405A60D0[
    //     350 * clientIndex + 279745
    // ]
    //
    // 0x5A60D0 + (279745 * 4)
    // = 0x6B73D4
    constexpr std::uintptr_t kTeamArrayRva =
        0x6B73D4;

    constexpr std::uintptr_t kClientStateStride =
        350 * sizeof(std::uint32_t);


    // qword_1405A6220
    //
    // Game uses:
    // (int)qword_1405A6220
    //
    // This is the local client index used by the
    // game's own team comparison.
    constexpr std::uintptr_t kLocalClientIndexRva =
        0x5A6220;


    // =========================================================
    // Renderer layout
    // =========================================================

    constexpr std::uintptr_t kPrimStateMaterialOffset =
        0x50;

    constexpr std::uintptr_t kMaterialNameOffset =
        0x00;

    constexpr std::uintptr_t kPackedCurrentOffset =
        0xB0;


    // =========================================================
    // Entity values
    // =========================================================

    constexpr std::uint32_t kPlayerEntityType =
        1;

    constexpr std::uint32_t kCorpseEntityType =
        2;

    constexpr std::size_t kSurfaceIndexCount =
        0x8000;


    // =========================================================
    // Metadata
    // =========================================================

    struct SurfaceMetadata
    {
        std::uint8_t entityType;
        std::uint8_t team;
    };


    // =========================================================
    // Engine function typedefs
    // =========================================================

    using AddDObjSurfacesFn =
        std::uint64_t* (__fastcall*)(
            void* sceneRecord,
            int viewIndex,
            std::uint64_t* output,
            std::uint64_t outputEnd
            );

    using PackedSurfaceConsumerFn =
        bool(__fastcall*)(
            void* renderState,
            void* context
            );

    struct DrawArgs
    {
        std::uint32_t numVertices;
        std::uint32_t startIndex;
        std::uint32_t primitiveCount;
    };

    using DrawIndexedPrimitiveFn =
        HRESULT(__fastcall*)(
            void* primState,
            const DrawArgs* args
            );


    // =========================================================
    // Original functions
    // =========================================================

    AddDObjSurfacesFn g_originalAddDObjSurfaces =
        nullptr;

    PackedSurfaceConsumerFn g_originalPackedSurfaceConsumer =
        nullptr;

    DrawIndexedPrimitiveFn g_originalDrawIndexedPrimitive =
        nullptr;


    // =========================================================
    // Settings
    // =========================================================

    ChamsSettings g_settings{};


    // =========================================================
    // Surface index -> entity metadata
    // =========================================================

    std::array<
        std::atomic<std::uint16_t>,
        kSurfaceIndexCount
    > g_surfaceMetadata{};


    // =========================================================
    // Current draw context
    // =========================================================

    thread_local SurfaceMetadata g_activeSurface{};

    thread_local bool g_hasActiveSurface =
        false;


    // =========================================================
    // Chams textures
    // =========================================================

    IDirect3DTexture9* g_hiddenTexture =
        nullptr;

    IDirect3DTexture9* g_visibleTexture =
        nullptr;

    D3DCOLOR g_hiddenTextureColor =
        0;

    D3DCOLOR g_visibleTextureColor =
        0;


    // =========================================================
    // Helpers
    // =========================================================

    std::uintptr_t GetGameBase()
    {
        return reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(
                L"iw5mp.exe"
            )
            );
    }


    std::uint16_t PackMetadata(
        const SurfaceMetadata metadata
    )
    {
        return
            static_cast<std::uint16_t>(
                metadata.entityType
                )
            |
            (
                static_cast<std::uint16_t>(
                    metadata.team
                    )
                << 8
                );
    }


    SurfaceMetadata UnpackMetadata(
        const std::uint16_t value
    )
    {
        SurfaceMetadata metadata{};

        metadata.entityType =
            static_cast<std::uint8_t>(
                value & 0xFF
                );

        metadata.team =
            static_cast<std::uint8_t>(
                (value >> 8) & 0xFF
                );

        return metadata;
    }


    // =========================================================
    // Local team
    //
    // Mirrors the game's own logic from sub_140092600:
    //
    // localClientIndex =
    //     (int)qword_1405A6220;
    //
    // localTeam =
    //     dword_1405A60D0[
    //         350 * localClientIndex + 279745
    //     ];
    // =========================================================

    std::uint32_t GetLocalTeam()
    {
        const std::uintptr_t gameBase =
            GetGameBase();

        if (!gameBase)
        {
            return 0;
        }

        __try
        {
            const int localClientIndex =
                *reinterpret_cast<const int*>(
                    gameBase +
                    kLocalClientIndexRva
                    );

            if (
                localClientIndex < 0 ||
                localClientIndex >= 18
                )
            {
                return 0;
            }

            return
                *reinterpret_cast<const std::uint32_t*>(
                    gameBase +
                    kTeamArrayRva +
                    (
                        static_cast<std::uintptr_t>(
                            localClientIndex
                            )
                        *
                        kClientStateStride
                        )
                    );
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
    }


    SurfaceMetadata ClassifyEntity(
        void* entity
    )
    {
        SurfaceMetadata metadata{};

        if (!entity)
        {
            return metadata;
        }

        __try
        {
            const auto entityAddress =
                reinterpret_cast<std::uintptr_t>(
                    entity
                    );

            const auto entityType =
                *reinterpret_cast<const std::uint32_t*>(
                    entityAddress +
                    kEntityTypeOffset
                    );


            // -------------------------------------------------
            // Corpse
            // -------------------------------------------------

            if (entityType == kCorpseEntityType)
            {
                metadata.entityType =
                    static_cast<std::uint8_t>(
                        kCorpseEntityType
                        );

                return metadata;
            }


            // -------------------------------------------------
            // Not a live player
            // -------------------------------------------------

            if (entityType != kPlayerEntityType)
            {
                return metadata;
            }


            // -------------------------------------------------
            // Live player
            // -------------------------------------------------

            const int clientIndex =
                *reinterpret_cast<const int*>(
                    entityAddress +
                    kEntityClientIndexOffset
                    );

            if (
                clientIndex < 0 ||
                clientIndex >= 18
                )
            {
                return metadata;
            }

            const std::uintptr_t gameBase =
                GetGameBase();

            if (!gameBase)
            {
                return metadata;
            }

            const auto team =
                *reinterpret_cast<const std::uint32_t*>(
                    gameBase +
                    kTeamArrayRva +
                    (
                        static_cast<std::uintptr_t>(
                            clientIndex
                            )
                        *
                        kClientStateStride
                        )
                    );

            metadata.entityType =
                static_cast<std::uint8_t>(
                    kPlayerEntityType
                    );

            metadata.team =
                static_cast<std::uint8_t>(
                    team
                    );
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            metadata = {};
        }

        return metadata;
    }


    bool IsSelected(
        const SurfaceMetadata metadata
    )
    {
        if (!g_settings.enabled)
        {
            return false;
        }


        // -----------------------------------------------------
        // Corpses remain controlled separately.
        // We'll fix the remaining corpse bug separately.
        // -----------------------------------------------------

        if (metadata.entityType == kCorpseEntityType)
        {
            return g_settings.deadBodies;
        }


        if (metadata.entityType != kPlayerEntityType)
        {
            return false;
        }


        // -----------------------------------------------------
        // All
        //
        // Keep team 0 excluded. The game's own comparison also
        // explicitly checks that the local team is non-zero.
        // -----------------------------------------------------

        if (g_settings.target == ChamsTarget::All)
        {
            return metadata.team != 0;
        }


        const std::uint32_t localTeam =
            GetLocalTeam();


        // No valid local team -> don't guess.
        if (
            localTeam == 0 ||
            metadata.team == 0
            )
        {
            return false;
        }


        // -----------------------------------------------------
        // Friendlies
        //
        // Same comparison used by the game:
        //
        // localTeam == playerTeam
        // -----------------------------------------------------

        if (
            g_settings.target ==
            ChamsTarget::Friendlies
            )
        {
            return
                static_cast<std::uint32_t>(
                    metadata.team
                    )
                ==
                localTeam;
        }


        // -----------------------------------------------------
        // Enemies
        //
        // Both teams valid, but different.
        // -----------------------------------------------------

        if (
            g_settings.target ==
            ChamsTarget::Enemies
            )
        {
            return
                static_cast<std::uint32_t>(
                    metadata.team
                    )
                !=
                localTeam;
        }


        return false;
    }


    bool Contains(
        const char* text,
        const char* token
    )
    {
        return
            text &&
            token &&
            std::strstr(
                text,
                token
            ) != nullptr;
    }


    bool IsPlayerBodyMaterial(
        void* primState
    )
    {
        if (!primState)
        {
            return false;
        }

        __try
        {
            const auto primAddress =
                reinterpret_cast<std::uintptr_t>(
                    primState
                    );

            void* material =
                *reinterpret_cast<void**>(
                    primAddress +
                    kPrimStateMaterialOffset
                    );

            if (!material)
            {
                return false;
            }

            const char* name =
                *reinterpret_cast<const char**>(
                    reinterpret_cast<std::uintptr_t>(
                        material
                        )
                    +
                    kMaterialNameOffset
                    );

            if (!name)
            {
                return false;
            }

            if (
                std::strncmp(
                    name,
                    "mc/mtl_",
                    7
                ) != 0
                )
            {
                return false;
            }

            if (
                Contains(name, "weapon") ||
                Contains(name, "viewmodel") ||
                Contains(name, "_uk_sas_van")
                )
            {
                return false;
            }

            return
                Contains(name, "henchmen") ||
                Contains(name, "russian") ||
                Contains(name, "militia") ||
                Contains(name, "malitia") ||
                Contains(name, "sniper") ||
                Contains(name, "delta") ||
                Contains(name, "sas") ||
                Contains(name, "gign") ||
                Contains(name, "us_army") ||
                Contains(name, "africa") ||
                Contains(name, "ghillie") ||
                Contains(name, "jugg");
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }


    D3DCOLOR ToD3DColor(
        const ChamsColor& color
    )
    {
        const auto clampByte =
            [](float value) -> BYTE
            {
                if (value < 0.0f)
                {
                    value = 0.0f;
                }

                if (value > 1.0f)
                {
                    value = 1.0f;
                }

                return static_cast<BYTE>(
                    value * 255.0f + 0.5f
                    );
            };

        return D3DCOLOR_ARGB(
            clampByte(color.a),
            clampByte(color.r),
            clampByte(color.g),
            clampByte(color.b)
        );
    }


    bool EnsureColorTexture(
        IDirect3DDevice9* device,
        IDirect3DTexture9** texture,
        D3DCOLOR* cachedColor,
        const ChamsColor& requestedColor
    )
    {
        if (
            !device ||
            !texture ||
            !cachedColor
            )
        {
            return false;
        }

        const D3DCOLOR color =
            ToD3DColor(
                requestedColor
            );

        if (!*texture)
        {
            if (
                FAILED(
                    device->CreateTexture(
                        1,
                        1,
                        1,
                        0,
                        D3DFMT_A8R8G8B8,
                        D3DPOOL_MANAGED,
                        texture,
                        nullptr
                    )
                )
                )
            {
                return false;
            }

            *cachedColor =
                ~color;
        }

        if (*cachedColor != color)
        {
            D3DLOCKED_RECT locked{};

            if (
                FAILED(
                    (*texture)->LockRect(
                        0,
                        &locked,
                        nullptr,
                        0
                    )
                )
                )
            {
                return false;
            }

            *reinterpret_cast<D3DCOLOR*>(
                locked.pBits
                ) = color;

            (*texture)->UnlockRect(
                0
            );

            *cachedColor =
                color;
        }

        return true;
    }


    // =========================================================
    // Hook 1
    // Scene entity -> packed surface metadata
    // =========================================================

    std::uint64_t* __fastcall HookedAddDObjSurfaces(
        void* sceneRecord,
        int viewIndex,
        std::uint64_t* output,
        std::uint64_t outputEnd
    )
    {
        SurfaceMetadata metadata{};

        if (sceneRecord)
        {
            __try
            {
                void* entity =
                    *reinterpret_cast<void**>(
                        reinterpret_cast<std::uintptr_t>(
                            sceneRecord
                            )
                        +
                        kSceneEntityOffset
                        );

                metadata =
                    ClassifyEntity(
                        entity
                    );
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                metadata = {};
            }
        }

        std::uint64_t* end =
            g_originalAddDObjSurfaces(
                sceneRecord,
                viewIndex,
                output,
                outputEnd
            );

        if (
            !output ||
            !end ||
            end < output
            )
        {
            return end;
        }

        const std::uint16_t packedMetadata =
            PackMetadata(
                metadata
            );

        for (
            std::uint64_t* entry = output;
            entry < end;
            ++entry
            )
        {
            const std::size_t surfaceIndex =
                static_cast<std::size_t>(
                    *entry &
                    0x7FFFULL
                    );

            g_surfaceMetadata[
                surfaceIndex
            ].store(
                packedMetadata,
                std::memory_order_release
            );
        }

        return end;
    }


    // =========================================================
    // Hook 2
    // Current packed surface -> TLS metadata
    // =========================================================

    bool __fastcall HookedPackedSurfaceConsumer(
        void* renderState,
        void* context
    )
    {
        const SurfaceMetadata previousSurface =
            g_activeSurface;

        const bool previousHasSurface =
            g_hasActiveSurface;

        SurfaceMetadata metadata{};

        bool hasMetadata =
            false;

        if (context)
        {
            __try
            {
                auto** current =
                    reinterpret_cast<std::uint64_t**>(
                        reinterpret_cast<std::uintptr_t>(
                            context
                            )
                        +
                        kPackedCurrentOffset
                        );

                if (
                    current &&
                    *current
                    )
                {
                    const std::uint64_t packedEntry =
                        **current;

                    const std::size_t surfaceIndex =
                        static_cast<std::size_t>(
                            packedEntry &
                            0x7FFFULL
                            );

                    metadata =
                        UnpackMetadata(
                            g_surfaceMetadata[
                                surfaceIndex
                            ].load(
                                std::memory_order_acquire
                            )
                                    );

                    hasMetadata =
                        metadata.entityType ==
                        kPlayerEntityType
                        ||
                        metadata.entityType ==
                        kCorpseEntityType;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                metadata = {};
                hasMetadata = false;
            }
        }

        g_activeSurface =
            metadata;

        g_hasActiveSurface =
            hasMetadata;

        const bool result =
            g_originalPackedSurfaceConsumer(
                renderState,
                context
            );

        g_activeSurface =
            previousSurface;

        g_hasActiveSurface =
            previousHasSurface;

        return result;
    }


    // =========================================================
    // Hook 3
    // Engine DrawIndexedPrimitive wrapper
    // =========================================================

    HRESULT __fastcall HookedDrawIndexedPrimitive(
        void* primState,
        const DrawArgs* args
    )
    {
        if (
            !g_hasActiveSurface ||
            !IsSelected(
                g_activeSurface
            ) ||
            !IsPlayerBodyMaterial(
                primState
            )
            )
        {
            return
                g_originalDrawIndexedPrimitive(
                    primState,
                    args
                );
        }

        if (!primState)
        {
            return
                g_originalDrawIndexedPrimitive(
                    primState,
                    args
                );
        }

        auto* device =
            *reinterpret_cast<IDirect3DDevice9**>(
                primState
                );

        if (!device)
        {
            return
                g_originalDrawIndexedPrimitive(
                    primState,
                    args
                );
        }


        // =====================================================
        // Visible texture
        // =====================================================

        if (
            !EnsureColorTexture(
                device,
                &g_visibleTexture,
                &g_visibleTextureColor,
                g_settings.visibleColor
            )
            )
        {
            return
                g_originalDrawIndexedPrimitive(
                    primState,
                    args
                );
        }


        // =====================================================
        // Wall Hack OFF
        //
        // Normal depth-tested visible pass only.
        // =====================================================

        if (!g_settings.wallHack)
        {
            IDirect3DBaseTexture9* oldTexture =
                nullptr;

            device->GetTexture(
                0,
                &oldTexture
            );

            device->SetTexture(
                0,
                g_visibleTexture
            );

            const HRESULT result =
                g_originalDrawIndexedPrimitive(
                    primState,
                    args
                );

            device->SetTexture(
                0,
                oldTexture
            );

            if (oldTexture)
            {
                oldTexture->Release();
            }

            return result;
        }


        // =====================================================
        // Wall Hack ON
        //
        // Keep the original working wall-hack implementation
        // for now. The hidden/visible overlap will be fixed
        // separately after the team/corpse issues.
        // =====================================================

        if (
            !EnsureColorTexture(
                device,
                &g_hiddenTexture,
                &g_hiddenTextureColor,
                g_settings.hiddenColor
            )
            )
        {
            return
                g_originalDrawIndexedPrimitive(
                    primState,
                    args
                );
        }


        DWORD oldZEnable =
            TRUE;

        IDirect3DBaseTexture9* oldTexture =
            nullptr;


        device->GetRenderState(
            D3DRS_ZENABLE,
            &oldZEnable
        );

        device->GetTexture(
            0,
            &oldTexture
        );


        // -----------------------------------------------------
        // PASS 1 - HIDDEN
        //
        // Original working implementation.
        // -----------------------------------------------------

        device->SetRenderState(
            D3DRS_ZENABLE,
            FALSE
        );

        device->SetTexture(
            0,
            g_hiddenTexture
        );

        g_originalDrawIndexedPrimitive(
            primState,
            args
        );


        // -----------------------------------------------------
        // PASS 2 - VISIBLE
        // -----------------------------------------------------

        device->SetRenderState(
            D3DRS_ZENABLE,
            oldZEnable
        );

        device->SetTexture(
            0,
            g_visibleTexture
        );

        const HRESULT result =
            g_originalDrawIndexedPrimitive(
                primState,
                args
            );


        // -----------------------------------------------------
        // Restore game state
        // -----------------------------------------------------

        device->SetTexture(
            0,
            oldTexture
        );

        device->SetRenderState(
            D3DRS_ZENABLE,
            oldZEnable
        );

        if (oldTexture)
        {
            oldTexture->Release();
        }

        return result;
    }


    // =========================================================
    // MinHook helper
    // =========================================================

    bool CreateHook(
        std::uintptr_t target,
        void* detour,
        void** original
    )
    {
        return
            MH_CreateHook(
                reinterpret_cast<void*>(
                    target
                    ),
                detour,
                original
            )
            ==
            MH_OK;
    }
}


// =============================================================
// Public API
// =============================================================

ChamsSettings& GetChamsSettings()
{
    return g_settings;
}


bool InstallChamsHooks()
{
    const std::uintptr_t gameBase =
        GetGameBase();

    if (!gameBase)
    {
        return false;
    }


    if (
        !CreateHook(
            gameBase +
            kAddDObjSurfacesRva,

            reinterpret_cast<void*>(
                &HookedAddDObjSurfaces
                ),

            reinterpret_cast<void**>(
                &g_originalAddDObjSurfaces
                )
        )
        )
    {
        return false;
    }


    if (
        !CreateHook(
            gameBase +
            kPackedSurfaceConsumerRva,

            reinterpret_cast<void*>(
                &HookedPackedSurfaceConsumer
                ),

            reinterpret_cast<void**>(
                &g_originalPackedSurfaceConsumer
                )
        )
        )
    {
        return false;
    }


    if (
        !CreateHook(
            gameBase +
            kDrawIndexedPrimitiveRva,

            reinterpret_cast<void*>(
                &HookedDrawIndexedPrimitive
                ),

            reinterpret_cast<void**>(
                &g_originalDrawIndexedPrimitive
                )
        )
        )
    {
        return false;
    }


    return true;
}