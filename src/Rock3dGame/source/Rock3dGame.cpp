#ifdef __APPLE__
#include "util/util_error.h"
#endif

#include "stdafx.h"
#include "Rock3dGame.h"

#include "game/World.h"
#include <ctime>

namespace r3d
{

ROCK3DGAME_API IWorld* CreateWorld(const IView::Desc& viewDesc, bool steamInit)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);

	srand(_time32(NULL));

	// restrict this main game thread to the first processor, so query performance counter won't jump on crappy AMD cpus
	DWORD dwThreadAffinityMask = 0x01;
	::SetThreadAffinityMask( ::GetCurrentThread(), dwThreadAffinityMask );

	World* world = new game::World();
	try
	{
#ifdef STEAM_SERVICE
		if (steamInit)
		{
			switch (SteamService::Initialize())
			{
			case SteamService::irFailed:
				LSL_LOG("steam initialize irFailed");		
				break;
			case SteamService::irRestart:
				LSL_LOG("steam initialize irRestart");
				//_world->Terminate(EXIT_FAILURE);
				break;
			case SteamService::irSuccess:
				LSL_LOG("steam initialize irSuccess");
				break;
			}
		}
#endif

		world->Init(viewDesc);
	}
	catch (const dxvk::DxvkError& err)
	{
		//DXVK reports failures by throwing DxvkError, which derives from
		//nothing -- so it slips past every catch in this codebase and reaches
		//the catch-all below with its message discarded.
		LSL_LOG("CreateWorld failed, DxvkError: " + err.message());

		ReleaseWorld(world);
		world = 0;
		throw;
	}
	catch (const std::exception& err)
	{
		//Without this the reason vanishes: lsl::Error logs itself on
		//construction, but DXVK and the standard library throw types that do
		//not, and the catch-all below then discards what happened.
		LSL_LOG(lsl::StrFmt("CreateWorld failed: %s", err.what()));

		ReleaseWorld(world);
		world = 0;
		throw;
	}
	catch(...)
	{
		LSL_LOG("CreateWorld failed: unknown exception");

		ReleaseWorld(world);
		world = 0;
		throw;
	}

	return world;	
}

ROCK3DGAME_API void ReleaseWorld(IWorld* world)
{
	static_cast<World*>(world)->Free();

#ifdef STEAM_SERVICE
	SteamService::Finalize();
#endif

	delete world;

	lsl::ReleaseSDK();
	lsl::FileSystem::Release();
	CoUninitialize();
}

}