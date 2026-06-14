#include "stdafx.h"
#include "AutoEmpire.h"

void Initialize()
{
    SimulatorSystem.AddStrategy(new AutoEmpire(), AutoEmpire::NOUN_ID);
    App::ConsolePrintF("JohnsSolarEmpires: AutoEmpire loaded");
}

void Dispose()
{
}

void AttachDetours()
{
    // No detours needed for this mod.
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ModAPI::AddPostInitFunction(Initialize);
        ModAPI::AddDisposeFunction(Dispose);
        PrepareDetours(hModule);
        AttachDetours();
        CommitDetours();
        break;

    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
