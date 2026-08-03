/*
 * The map editor, on Dear ImGui.
 *
 * Replaces the MFC one for everything that is not Windows. The MFC sources
 * stay, still built by MSVC, until someone decides whether a Windows editor is
 * worth keeping alongside a cross-platform one -- so nothing here removes a
 * working thing.
 *
 * WHAT THIS IS NOT. It is not a port of the MFC code. The editor's logic never
 * lived there: CMapEditorDoc was a facade over the engine's own
 * r3d::edit::IEdit/IMap/ITrace/ISceneControl, and those 1,595 lines in
 * src/Rock3dGame/source/edit already compiled on macOS untouched. What MFC held
 * was ~90 HTREEITEM sites keeping tree controls in step with engine objects,
 * and in immediate mode that bookkeeping does not exist -- the panes read the
 * engine's containers each frame. That is where the line count goes.
 *
 * The editor differs from the game by two calls: CreateWorld(desc, false)
 * rather than true, and RunWorldEdit() rather than RunGame().
 *
 * A SEPARATE BINARY, not a mode of the game. The game is the thing currently
 * under measurement -- eight-run batches for the FAudio crash, frame dumps for
 * the renderer -- and adding a mode variable to it would add one to every
 * future measurement. A binary that does not exist in editor mode is a fact; a
 * binary that behaves identically in both modes is an assumption.
 *
 * TWO THINGS TO KNOW BEFORE CHANGING ANY OF IT:
 *
 *   The engine caches D3D9 state and only forwards changes
 *   (ContextInfo::SetRenderState and friends). Anything ImGui fails to restore
 *   desynchronises that cache and corrupts the NEXT frame's scene, not the UI
 *   -- the same defect 0811b5b already fixed once. imgui_impl_dx9 captures and
 *   restores a full D3DSBT_ALL state block, which is why this works; if the
 *   scene ever renders wrong after a UI change, that is where to look.
 *
 *   IMapObjRef comparison must use Equal(), never ==. The refs are wrappers
 *   handed out fresh by every accessor, so == compares two wrapper addresses
 *   and is almost always false even for the same object. Writing == compiles,
 *   runs, and silently takes the "changed" branch every frame.
 */

/*
 * ImGui FIRST, and this is not cosmetic.
 *
 * LexStd's lslCommon.h does `#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)`
 * under _DEBUG, for the MSVC debug heap. imgui.h declares a placement operator
 * new for its ImNew helper, and with that macro in force the declaration is
 * rewritten into nonsense -- "function cannot return function type". Including
 * ImGui before anything that reaches lslCommon.h avoids it without touching
 * either library.
 */
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_dx9.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#include "xplatform.h"

#include "IWorld.h"
#include "IView.h"
#include "IOverlay.h"
#include "IEdit.h"
#include "IMap.h"
#include "ITrace.h"
#include "IDataBase.h"
#include "ISceneControl.h"
#include "ICameraManager.h"
#include "Rock3dGame.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace r3d;

namespace
{

const int cWidth = 1600;
const int cHeight = 900;

SDL_Window* gWindow = NULL;
SDL_MetalView gMetalView = NULL;
void* gLayer = NULL;
game::IWorld* gWorld = NULL;
bool gQuit = false;

/* ------------------------------------------------------------- the document --
 *
 * What CMapEditorDoc was, minus CDocument and minus the observer list.
 * Immediate mode re-reads the model every frame, so OnAddMapObj/OnDelMapObj/
 * OnSelectMapObj collapse into nothing at all.
 */
struct Document
{
	edit::IEdit* Edit()
	{
		return gWorld ? gWorld->GetEdit() : NULL;
	}

	edit::IMap* Map()
	{
		edit::IEdit* e = Edit();
		return e ? e->GetMap() : NULL;
	}

	edit::ITrace* Trace()
	{
		edit::IMap* m = Map();
		return m ? m->GetTrace() : NULL;
	}

	edit::ISceneControl* ScControl()
	{
		edit::IEdit* e = Edit();
		return e ? e->GetScControl() : NULL;
	}

	edit::IDataBase* DB()
	{
		edit::IEdit* e = Edit();
		return e ? e->GetDB() : NULL;
	}

	/*
	 * Selection, with the Equal() guard the MFC version had at
	 * MapEditorDoc.cpp:202. Without it every frame would look like a change --
	 * see the note at the top of this file.
	 */
	void SelectMapObj(const edit::IMapObjRef& value)
	{
		if (selMapObj && value && selMapObj->Equal(value.Pnt()))
			return;

		selMapObj = value;

		if (edit::ISceneControl* sc = ScControl())
		{
			if (value)
				sc->SelectNode(Map()->GetMapObjControl(value, edit::IMap::ControlEventRef()));
			else
				sc->SelectNode(edit::IScNodeContRef());
		}
	}

	edit::IMapObjRef selMapObj;
	edit::IWayPointRef selWayPoint;

	std::string path;
	std::string status;
};

Document gDoc;

void SetStatus(const std::string& text)
{
	gDoc.status = text;
	LSL_LOG(text.c_str());
}

/*
 * Placement, which is CClassView's link mode.
 *
 * The object is created the moment a library record is chosen, not when the
 * viewport is clicked, and the scene control is put into smLink so the engine
 * drags it under the cursor. A click then commits it and creates a fresh one at
 * the same position, so repeated placement is click-click-click.
 *
 * Doing it the other way round -- create on click -- was the first attempt, and
 * it had no way to know where to put the object: nothing has raycast the world
 * at that point, so everything landed at the origin. smLink is what already
 * solves that, and it is the engine's own mechanism rather than a new one.
 */
edit::IMapObjRecRef gPendingRecord;
edit::IMapObjRef gPendingObject;
bool gAutoRotate = false;
bool gAutoScale = false;

/*
 * The jitter from ClassView.cpp:155-164, unchanged.
 *
 * Scale is uniform, +/-30%; rotation is +/-15 degrees on each of yaw, pitch and
 * roll. Both are what makes a hand-placed forest not look stamped, and the
 * constants are the shipped ones rather than a guess.
 */
void ApplyPlacementJitter(const edit::IMapObjRef& obj)
{
	if (!obj)
		return;

	if (gAutoScale)
		obj->SetScale(IdentityVector * (1.0f + 0.3f * RandomRange(-1.0f, 1.0f)));

	if (gAutoRotate)
	{
		D3DXQUATERNION rot;
		D3DXQuaternionRotationYawPitchRoll(&rot,
			D3DX_PI / 12 * RandomRange(-1.0f, 1.0f),
			D3DX_PI / 12 * RandomRange(-1.0f, 1.0f),
			D3DX_PI / 12 * RandomRange(-1.0f, 1.0f));
		obj->SetRot(rot);
	}
}

void CancelPlacement();

void BeginPlacement(const edit::IMapObjRecRef& record, const D3DXVECTOR3& pos)
{
	CancelPlacement();

	edit::IMap* map = gDoc.Map();
	edit::ISceneControl* sc = gDoc.ScControl();
	if (!map || !sc || !record)
		return;

	sc->SetSelMode(edit::ISceneControl::smLink);

	gPendingRecord = record;
	gPendingObject = map->AddMapObj(record);
	if (!gPendingObject)
		return;

	gPendingObject->SetPos(pos);
	ApplyPlacementJitter(gPendingObject);
	gDoc.SelectMapObj(gPendingObject);
}

void CancelPlacement()
{
	if (!gPendingObject)
	{
		gPendingRecord = edit::IMapObjRecRef();
		return;
	}

	edit::IMap* map = gDoc.Map();
	edit::ISceneControl* sc = gDoc.ScControl();

	/*
	 * Deselect before deleting, and only if this is still what is selected.
	 * CClassView::DeselectItem guards the same way, and its comment says why:
	 * focus could move between the two, leaving the selection pointing at an
	 * object about to be destroyed.
	 */
	if (gDoc.selMapObj && gDoc.selMapObj->Equal(gPendingObject.Pnt()))
		gDoc.SelectMapObj(edit::IMapObjRef());

	if (sc && sc->GetSelMode() == edit::ISceneControl::smLink)
		sc->SetSelMode(edit::ISceneControl::smNone);

	if (map)
		map->DelMapObj(gPendingObject);

	gPendingObject = edit::IMapObjRef();
	gPendingRecord = edit::IMapObjRecRef();
}

/* ------------------------------------------------------------------ panes -- */

/*
 * The object library, which CClassView was.
 *
 * Cached rather than walked each frame, and that is a correctness requirement
 * rather than an optimisation: every FirstRecord/NextRecord/FirstNode/NextNode
 * hands back a *fresh* wrapper held by an AutoRef, and AutoRef maintains a list
 * of back-references for leak tracking. Walking db.xml's whole library at 60Hz
 * would allocate tens of thousands of list nodes a second.
 */
struct LibraryNode
{
	std::string name;
	std::vector<LibraryNode> children;
	std::vector<std::pair<std::string, edit::IMapObjRecRef> > records;
};

std::vector<LibraryNode> gLibrary;
bool gLibraryBuilt = false;

void BuildLibraryNode(const edit::IRecordNodeRef& node, LibraryNode& out)
{
	out.name = node->GetName();

	for (edit::IMapObjRecRef rec = node->FirstRecord(); rec; node->NextRecord(rec))
		out.records.push_back(std::make_pair(rec->GetName(), rec));

	for (edit::IRecordNodeRef child = node->FirstNode(); child; node->NextNode(child))
	{
		out.children.push_back(LibraryNode());
		BuildLibraryNode(child, out.children.back());
	}
}

void BuildLibrary()
{
	gLibrary.clear();
	gLibraryBuilt = true;

	edit::IDataBase* db = gDoc.DB();
	if (!db)
		return;

	for (unsigned i = 0; i < db->GetMapObjLibCnt(); ++i)
	{
		edit::IMapObjLibRef lib = db->GetMapObjLib(i);
		if (!lib)
			continue;

		gLibrary.push_back(LibraryNode());
		BuildLibraryNode(edit::IRecordNodeRef(lib.Pnt()), gLibrary.back());
	}
}


void DrawLibraryNode(const LibraryNode& node)
{
	if (!ImGui::TreeNode(node.name.c_str()))
		return;

	for (size_t i = 0; i < node.children.size(); ++i)
		DrawLibraryNode(node.children[i]);

	for (size_t i = 0; i < node.records.size(); ++i)
	{
		ImGui::PushID(int(i));
		const bool selected = gPendingRecord && node.records[i].second &&
			gPendingRecord->Equal(node.records[i].second.Pnt());

		if (ImGui::Selectable(node.records[i].first.c_str(), selected))
		{
			BeginPlacement(node.records[i].second, D3DXVECTOR3(0.0f, 0.0f, 0.0f));
			SetStatus("placing " + node.records[i].first +
				" -- move to position, click to drop, Escape to cancel");
		}
		ImGui::PopID();
	}

	ImGui::TreePop();
}

void DrawLibraryPane()
{
	ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320.0f, 420.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Library"))
	{
		ImGui::End();
		return;
	}

	if (!gLibraryBuilt)
		BuildLibrary();

	if (ImGui::Button("Refresh"))
		BuildLibrary();

	ImGui::SameLine();
	ImGui::Checkbox("Auto-rot", &gAutoRotate);
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scale", &gAutoScale);

	if (gPendingRecord)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "placing: %s",
			gPendingRecord->GetName().c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("cancel"))
		{
			CancelPlacement();
			SetStatus("placement cancelled");
		}
	}

	ImGui::Separator();

	for (size_t i = 0; i < gLibrary.size(); ++i)
		DrawLibraryNode(gLibrary[i]);

	ImGui::End();
}

/*
 * The scene outliner, which CFileView was. Objects grouped by the map's own
 * categories, read fresh each frame -- there are far fewer placed objects than
 * library records, and the selection has to stay in step with the viewport.
 */
void DrawScenePane()
{
	ImGui::SetNextWindowPos(ImVec2(10.0f, 460.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320.0f, 420.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Scene"))
	{
		ImGui::End();
		return;
	}

	edit::IMap* map = gDoc.Map();
	if (!map)
	{
		ImGui::TextUnformatted("no map");
		ImGui::End();
		return;
	}

	for (unsigned cat = 0; cat < map->GetCatCount(); ++cat)
	{
		const std::string catName = map->GetCatName(cat);
		if (!ImGui::TreeNode(catName.c_str()))
			continue;

		int index = 0;
		for (edit::IMapObjRef obj = map->GetFirst(cat); obj; map->GetNext(cat, obj), ++index)
		{
			/* Index-based ids: object names duplicate freely -- map1.r3dMap has
			   palma20, palma21, palma22 and so on -- so the name is not unique
			   enough to key an ImGui item by. */
			ImGui::PushID(index);

			const bool selected = gDoc.selMapObj && obj &&
				gDoc.selMapObj->Equal(obj.Pnt());

			if (ImGui::Selectable(obj->GetName().c_str(), selected))
				gDoc.SelectMapObj(obj);

			ImGui::PopID();
		}

		ImGui::TreePop();
	}

	ImGui::End();
}

/*
 * Waypoints and paths, which CTraceView was -- minus the CSplitterWnd and the
 * two helper window classes that existed only to draw a focus rectangle.
 */
void DrawTracePane()
{
	ImGui::SetNextWindowPos(ImVec2(1270.0f, 30.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320.0f, 500.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Trace"))
	{
		ImGui::End();
		return;
	}

	edit::ITrace* trace = gDoc.Trace();
	if (!trace)
	{
		ImGui::TextUnformatted("no trace");
		ImGui::End();
		return;
	}

	if (ImGui::Button("Add point"))
	{
		edit::IWayPointRef point = trace->AddPoint();
		if (point)
		{
			/* The size CTraceView used, TraceView.cpp:343-348. */
			point->SetSize(7.0f);
			SetStatus("waypoint added");
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Add path"))
	{
		trace->AddPath();
		SetStatus("path added");
	}

	bool visualize = trace->GetEnableVisualize();
	ImGui::SameLine();
	if (ImGui::Checkbox("Visualise", &visualize))
		trace->EnableVisualize(visualize);

	ImGui::Separator();

	if (ImGui::CollapsingHeader("Points", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::BeginChild("points", ImVec2(0, 180), true);

		for (edit::IWayPointRef point = trace->FirstPoint(); point; trace->NextPoint(point))
		{
			/* GetId is genuinely stable, unlike the object names next door, so
			   it is both the label and the ImGui id. */
			const unsigned id = point->GetId();
			ImGui::PushID(int(id));

			char label[64];
			std::snprintf(label, sizeof(label), "wp %u%s", id,
				point->IsUsedByPath() ? " (in path)" : "");

			const bool selected = gDoc.selWayPoint && gDoc.selWayPoint->Equal(point.Pnt());
			if (ImGui::Selectable(label, selected))
			{
				gDoc.selWayPoint = point;
				if (edit::ISceneControl* sc = gDoc.ScControl())
					sc->SelectNode(trace->GetPointControl(point, edit::ITrace::ControlEventRef()));
			}

			if (ImGui::BeginPopupContextItem())
			{
				/*
				 * Refused inline rather than by exception. The MFC version
				 * threw lsl::Error for what is an ordinary user mistake
				 * (TraceView.cpp:350-366).
				 */
				if (point->IsUsedByPath())
					ImGui::TextDisabled("in use by a path");
				else if (ImGui::MenuItem("Delete"))
				{
					edit::IWayPointRef doomed = point;
					gDoc.selWayPoint = edit::IWayPointRef();
					trace->DelPoint(doomed);
					SetStatus("waypoint deleted");
					ImGui::EndPopup();
					ImGui::PopID();
					break;
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		ImGui::EndChild();
	}

	if (ImGui::CollapsingHeader("Paths", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::BeginChild("paths", ImVec2(0, 180), true);

		int pathIndex = 0;
		for (edit::IWayPathRef path = trace->FirstPath(); path; trace->NextPath(path), ++pathIndex)
		{
			ImGui::PushID(pathIndex);

			char label[64];
			std::snprintf(label, sizeof(label), "path %d", pathIndex);

			if (ImGui::TreeNode(label))
			{
				int nodeIndex = 0;
				for (edit::IWayNodeRef node = path->First(); node; path->Next(node), ++nodeIndex)
				{
					ImGui::PushID(nodeIndex);
					char nodeLabel[64];
					std::snprintf(nodeLabel, sizeof(nodeLabel), "node %d", nodeIndex);

					if (ImGui::Selectable(nodeLabel))
						trace->SelectNode(node);

					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

/*
 * The selected object's transform.
 *
 * New -- CPropertiesWnd was the MFC wizard's stock sample, populated with
 * "Appearance / 3D Look / Border / Font" and not one r3d:: reference. It is
 * nearly free because ISceneControl::GetSelNode returns an IScNodeCont, and
 * both map objects and waypoints implement it, so one pane inspects either with
 * no type switch.
 */
void DrawInspectorPane()
{
	ImGui::SetNextWindowPos(ImVec2(1270.0f, 540.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320.0f, 300.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Inspector"))
	{
		ImGui::End();
		return;
	}

	edit::ISceneControl* sc = gDoc.ScControl();
	edit::IScNodeContRef node = sc ? sc->GetSelNode() : edit::IScNodeContRef();

	if (!node)
	{
		ImGui::TextDisabled("nothing selected");
		ImGui::End();
		return;
	}

	if (gDoc.selMapObj)
	{
		ImGui::Text("%s", gDoc.selMapObj->GetName().c_str());
		if (edit::IMapObjRecRef rec = gDoc.selMapObj->GetRecord())
			ImGui::TextDisabled("%s / %s", rec->GetCategoryName().c_str(),
				rec->GetName().c_str());
		ImGui::Separator();
	}

	D3DXVECTOR3 pos = node->GetPos();
	float p[3] = { pos.x, pos.y, pos.z };
	if (ImGui::DragFloat3("Position", p, 0.25f))
		node->SetPos(D3DXVECTOR3(p[0], p[1], p[2]));

	D3DXVECTOR3 scale = node->GetScale();
	float s[3] = { scale.x, scale.y, scale.z };
	if (ImGui::DragFloat3("Scale", s, 0.01f))
		node->SetScale(D3DXVECTOR3(s[0], s[1], s[2]));

	D3DXQUATERNION rot = node->GetRot();
	float q[4] = { rot.x, rot.y, rot.z, rot.w };
	if (ImGui::DragFloat4("Rotation", q, 0.01f))
	{
		D3DXQUATERNION out(q[0], q[1], q[2], q[3]);
		D3DXQuaternionNormalize(&out, &out);
		node->SetRot(out);
	}

	if (gDoc.selWayPoint)
	{
		float size = gDoc.selWayPoint->GetSize();
		if (ImGui::DragFloat("Waypoint size", &size, 0.1f, 0.1f, 200.0f))
			gDoc.selWayPoint->SetSize(size);
	}

	ImGui::End();
}

/* The Edit-Map toolbar, which was MainFrm.cpp:293-406. Immediate mode makes
   OnEditMapToolBarUpdateCommandUI disappear: the checked state is simply read
   each frame. */
void DrawToolbar()
{
	edit::ISceneControl* sc = gDoc.ScControl();
	edit::IMap* map = gDoc.Map();
	if (!sc || !map)
		return;

	const edit::ISceneControl::SelMode mode = sc->GetSelMode();

	struct Entry { const char* label; edit::ISceneControl::SelMode mode; };
	static const Entry cModes[] =
	{
		{ "None",   edit::ISceneControl::smNone },
		{ "Move",   edit::ISceneControl::smMove },
		{ "Rotate", edit::ISceneControl::smRotate },
		{ "Scale",  edit::ISceneControl::smScale },
	};

	for (size_t i = 0; i < sizeof(cModes) / sizeof(cModes[0]); ++i)
	{
		if (i)
			ImGui::SameLine();

		const bool active = mode == cModes[i].mode;
		if (active)
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));

		if (ImGui::Button(cModes[i].label))
			sc->SetSelMode(cModes[i].mode);

		if (active)
			ImGui::PopStyleColor();
	}

	ImGui::SameLine();
	bool showBBox = map->GetShowBBox();
	if (ImGui::Checkbox("BBox", &showBBox))
		map->SetShowBBox(showBBox);

	ImGui::SameLine();
	bool linkBB = sc->GetLinkBB();
	if (ImGui::Checkbox("Link BB", &linkBB))
		sc->SetLinkBB(linkBB);

	if (game::ICameraManager* cam = gWorld->GetICamera())
	{
		ImGui::SameLine();
		ImGui::TextUnformatted("|");
		ImGui::SameLine();
		if (ImGui::Button("Persp"))
			cam->ChangeStyle(game::ICameraManager::csFreeView);
		ImGui::SameLine();
		if (ImGui::Button("Iso"))
			cam->ChangeStyle(game::ICameraManager::csIsoView);
	}
}

/* ------------------------------------------------------------ file dialogs --
 *
 * SDL's dialogs are asynchronous and answer on a callback, which may arrive on
 * another thread. So the callback does nothing but record the path, and the
 * frame loop acts on it -- loading a level from a arbitrary thread while the
 * engine is mid-frame is not a thing to attempt.
 *
 * This is better than the modal dialog MFC used, not a workaround for lacking
 * one: the engine keeps rendering while the sheet is open.
 */
enum PendingFileAction { pfNone, pfOpen, pfSave };

PendingFileAction gPendingFile = pfNone;
std::string gPendingFilePath;
bool gDialogOpen = false;

void SDLCALL OnFileChosen(void* userdata, const char* const* filelist, int filter)
{
	(void)filter;

	const PendingFileAction action =
		static_cast<PendingFileAction>(reinterpret_cast<intptr_t>(userdata));

	gDialogOpen = false;

	/* NULL means an error, an empty list means the user cancelled. Neither is
	   worth reporting as a fault. */
	if (!filelist || !filelist[0])
		return;

	gPendingFilePath = filelist[0];
	gPendingFile = action;
}

void ShowLevelDialog(PendingFileAction action)
{
	if (gDialogOpen)
		return;

	gDialogOpen = true;

	static const SDL_DialogFileFilter cFilters[] =
	{
		{ "Rock3D map", "r3dMap" },
		{ "All files", "*" },
	};

	void* tag = reinterpret_cast<void*>(static_cast<intptr_t>(action));

	if (action == pfOpen)
		SDL_ShowOpenFileDialog(OnFileChosen, tag, gWindow, cFilters, 2, NULL, false);
	else
		SDL_ShowSaveFileDialog(OnFileChosen, tag, gWindow, cFilters, 2, NULL);
}

/*
 * Acted on from the frame loop, never from the callback.
 *
 * Note LoadLevel and SaveLevel both go through GetAppFilePath, which PREPENDS
 * the application directory -- so an absolute path from a dialog produces
 * nonsense on the first attempt and succeeds only through the raw-name retry at
 * lslResource.cpp:58-66. MFC relied on exactly the same fallback with its own
 * absolute paths, so this is long-standing rather than new, but it means level
 * loading has an untested primary path and a load-bearing secondary one.
 */
void ProcessPendingFile()
{
	if (gPendingFile == pfNone)
		return;

	const PendingFileAction action = gPendingFile;
	gPendingFile = pfNone;

	CancelPlacement();

	try
	{
		if (action == pfOpen)
		{
			gDoc.SelectMapObj(edit::IMapObjRef());
			gDoc.selWayPoint = edit::IWayPointRef();
			gWorld->LoadLevel(gPendingFilePath);
			gDoc.path = gPendingFilePath;
			gLibraryBuilt = false;      /* the database may have changed */
			SetStatus("opened " + gPendingFilePath);
		}
		else
		{
			gWorld->SaveLevel(gPendingFilePath);
			gDoc.path = gPendingFilePath;
			SetStatus("saved " + gPendingFilePath);
		}
	}
	catch (const std::exception& e)
	{
		SetStatus(std::string("failed: ") + e.what());
	}
}

/* ----------------------------------------------------- the round-trip check --
 *
 * RRR3D_EDITOR_CHECK=roundtrip -- does an edit actually reach the file?
 *
 * This is the one thing about an editor that cannot be checked by looking at
 * it. A screenshot proves an object is in the scene graph; it says nothing
 * about whether saving wrote it, or whether loading reads back what was
 * written. Everything in between -- the placement, the transform, the
 * serialisation, the reload -- either survives that round trip or the editor is
 * a viewer with extra steps.
 *
 * Verified two ways on purpose, because they can fail independently: the
 * reloaded scene is asked what it holds, AND the file on disk is searched for
 * the coordinates as text. A save that writes nothing and a load that ignores
 * the file would agree with each other; neither agrees with grep.
 */
const D3DXVECTOR3 cCheckPos(1234.5f, 678.25f, 90.125f);

unsigned CountMapObjects()
{
	edit::IMap* map = gDoc.Map();
	if (!map)
		return 0;

	unsigned total = 0;
	for (unsigned cat = 0; cat < map->GetCatCount(); ++cat)
		for (edit::IMapObjRef obj = map->GetFirst(cat); obj; map->GetNext(cat, obj))
			++total;

	return total;
}

/* The first record anywhere in the library -- which one does not matter, only
   that it is a real one the database will serialise. */
bool FirstRecord(const std::vector<LibraryNode>& nodes, edit::IMapObjRecRef& out,
	std::string& name)
{
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		if (!nodes[i].records.empty())
		{
			out = nodes[i].records[0].second;
			name = nodes[i].records[0].first;
			return true;
		}

		if (FirstRecord(nodes[i].children, out, name))
			return true;
	}

	return false;
}

bool FileContains(const std::string& path, const std::string& needle)
{
	std::FILE* f = std::fopen(path.c_str(), "rb");
	if (!f)
		return false;

	std::string contents;
	char buffer[8192];
	size_t got = 0;
	while ((got = std::fread(buffer, 1, sizeof(buffer), f)) > 0)
		contents.append(buffer, got);
	std::fclose(f);

	return contents.find(needle) != std::string::npos;
}

int RunRoundTripCheck()
{
	std::fprintf(stderr, "MapEditor: round trip\n");

	/*
	 * Frames first. The interactive editor has always rendered several times
	 * before anyone can click a library entry, and parts of the engine finish
	 * arriving on those frames -- running the check against a world that has
	 * never stepped crashes in the database walk. Ten is arbitrary and cheap.
	 */
	for (int i = 0; i < 10; ++i)
		gWorld->MainProgress();

	BuildLibrary();

	edit::IMapObjRecRef record;
	std::string recordName;
	if (!FirstRecord(gLibrary, record, recordName))
	{
		std::fprintf(stderr, "  FAIL no records in the library\n");
		return 1;
	}

	const unsigned before = CountMapObjects();
	std::fprintf(stderr, "  record '%s', %u objects before\n", recordName.c_str(), before);

	/* Place it the way a click does, then commit it the way the next click
	   does -- through the same code, so this tests the editor and not a
	   parallel path written for the test. */
	BeginPlacement(record, cCheckPos);
	if (!gPendingObject)
	{
		std::fprintf(stderr, "  FAIL placement produced nothing\n");
		return 1;
	}

	gPendingObject->SetPos(cCheckPos);
	const std::string placedName = gPendingObject->GetName();
	gPendingObject = edit::IMapObjRef();          /* committed */
	CancelPlacement();                            /* leaves link mode */

	const unsigned placed = CountMapObjects();
	if (placed != before + 1)
	{
		std::fprintf(stderr, "  FAIL after placing: %u objects, expected %u\n",
			placed, before + 1);
		return 1;
	}
	std::fprintf(stderr, "  placed '%s' at %.3f %.3f %.3f\n", placedName.c_str(),
		double(cCheckPos.x), double(cCheckPos.y), double(cCheckPos.z));

	const std::string path = "roundtrip_check.r3dMap";
	gWorld->SaveLevel(path);

	/* The file, read as text. GetAppFilePath prepends the application
	   directory, and the level was saved through it, so look there. */
	char coords[128];
	std::snprintf(coords, sizeof(coords), "%g %g %g",
		double(cCheckPos.x), double(cCheckPos.y), double(cCheckPos.z));

	if (!FileContains(path, coords))
	{
		std::fprintf(stderr,
			"  FAIL '%s' is not in the saved file -- the edit did not reach disk\n",
			coords);
		return 1;
	}
	std::fprintf(stderr, "  saved, and '%s' is in the file\n", coords);

	/* Reload, and ask the scene rather than trusting the save. */
	gDoc.SelectMapObj(edit::IMapObjRef());
	gWorld->LoadLevel(path);

	const unsigned after = CountMapObjects();
	if (after != placed)
	{
		std::fprintf(stderr, "  FAIL after reload: %u objects, expected %u\n",
			after, placed);
		return 1;
	}

	bool found = false;
	edit::IMap* map = gDoc.Map();
	for (unsigned cat = 0; cat < map->GetCatCount() && !found; ++cat)
	{
		for (edit::IMapObjRef obj = map->GetFirst(cat); obj; map->GetNext(cat, obj))
		{
			const D3DXVECTOR3 p = obj->GetPos();
			if (std::fabs(p.x - cCheckPos.x) < 0.01f &&
			    std::fabs(p.y - cCheckPos.y) < 0.01f &&
			    std::fabs(p.z - cCheckPos.z) < 0.01f)
			{
				found = true;
				break;
			}
		}
	}

	if (!found)
	{
		std::fprintf(stderr,
			"  FAIL nothing at %.3f %.3f %.3f after reload\n",
			double(cCheckPos.x), double(cCheckPos.y), double(cCheckPos.z));
		return 1;
	}

	std::fprintf(stderr, "  reloaded: %u objects, and one is where it was put\n", after);
	std::fprintf(stderr, "MapEditor: round trip ok\n");
	return 0;
}

/*
 * Every engine reference this file holds, dropped before the world goes.
 *
 * gDoc's selection, the pending placement and the whole library cache are
 * AutoRefs, and they live at file scope -- so without this they are destroyed
 * after main returns, which is after ReleaseWorld has freed what they point at.
 * AutoRef's destructor unhooks itself from the object's back-reference list, so
 * it reads a vtable that is no longer there.
 *
 * That crashed on exit with a corrupt stack, EXC_BAD_ACCESS on an address that
 * is plainly not a pointer, and it did so AFTER all the work had succeeded --
 * so the process reported failure about a run that had gone perfectly. The
 * round-trip check made it visible because its exit status is the whole point
 * of it; the interactive editor had been doing it silently every time it quit.
 */
void ReleaseEditorRefs()
{
	gDoc.selMapObj = edit::IMapObjRef();
	gDoc.selWayPoint = edit::IWayPointRef();
	gPendingObject = edit::IMapObjRef();
	gPendingRecord = edit::IMapObjRecRef();
	gLibrary.clear();
	gLibraryBuilt = false;
}

/* ---------------------------------------------------------------- overlay -- */

/*
 * The engine draws the scene, then calls this, then presents. See IOverlay --
 * the engine had no such seam before, because MFC drew its panes into sibling
 * HWNDs and never needed one.
 */
class EditorOverlay: public game::IOverlay
{
public:
	void OnLostDevice() override
	{
		if (_started)
			ImGui_ImplDX9_InvalidateDeviceObjects();
	}

	void OnResetDevice(IDirect3DDevice9*) override
	{
		if (_started)
			ImGui_ImplDX9_CreateDeviceObjects();
	}

	void OnDraw(IDirect3DDevice9* device) override
	{
		/*
		 * The backend is initialised here rather than at startup because this
		 * is the only place the device appears. IWorld exposes MainProgress,
		 * GetView, GetEdit and GetICamera and nothing that yields a device --
		 * and adding an accessor for it would put Direct3D in the game's public
		 * interface for the sake of one caller.
		 */
		if (!_started)
		{
			if (!device || !ImGui_ImplDX9_Init(device))
				return;
			_started = true;
		}

		ImDrawData* data = ImGui::GetDrawData();
		if (std::getenv("RRR3D_EDITOR_TRACE"))
		{
			static int n = 0;
			if (n < 5)
				std::fprintf(stderr, "overlay draw %d: data=%p lists=%d\n",
					n++, (void*)data, data ? data->CmdListsCount : -1);
		}

		if (data)
			ImGui_ImplDX9_RenderDrawData(data);
	}

	bool started() const { return _started; }

private:
	bool _started = false;
};

EditorOverlay gOverlay;

/* ------------------------------------------------------------------- frame -- */

void BuildUi()
{
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
		ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New"))
			{
				if (edit::IMap* map = gDoc.Map())
					map->ClearMap();
				if (edit::ITrace* trace = gDoc.Trace())
					trace->Clear();
				gDoc.SelectMapObj(edit::IMapObjRef());
				SetStatus("new map");
			}

			if (ImGui::MenuItem("Open..."))
				ShowLevelDialog(pfOpen);

			if (ImGui::MenuItem("Save", NULL, false, !gDoc.path.empty()))
			{
				gPendingFilePath = gDoc.path;
				gPendingFile = pfSave;
			}

			if (ImGui::MenuItem("Save As..."))
				ShowLevelDialog(pfSave);

			ImGui::Separator();
			if (ImGui::MenuItem("Quit"))
				gQuit = true;

			ImGui::EndMenu();
		}

		ImGui::Separator();
		DrawToolbar();
		ImGui::EndMainMenuBar();
	}

	DrawLibraryPane();
	DrawScenePane();
	DrawTracePane();
	DrawInspectorPane();

	ImGui::SetNextWindowPos(ImVec2(340.0f, 850.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(900.0f, 40.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Status"))
		ImGui::TextUnformatted(gDoc.status.c_str());
	ImGui::End();
}

/*
 * A viewport click, once ImGui has declined it.
 *
 * The order is CMapEditorView::OnMouseClickEvent's: offer it to the engine's
 * own view first -- the manipulators live there -- then to a pending library
 * placement, and only then treat it as a pick.
 */
void OnViewportClick(const lsl::Point& coord, bool down, bool shift, bool ctrl)
{
	game::IView* view = gWorld->GetView();
	if (view->OnMouseClickEvent(lsl::mkLeft, down ? lsl::ksDown : lsl::ksUp, coord, shift, ctrl))
		return;

	if (!down)
		return;

	edit::IMap* map = gDoc.Map();
	if (!map)
		return;

	/*
	 * A click commits the object being dragged and starts another at the same
	 * place, so a row of trees is one click each -- CClassView's
	 * OnMapViewMouseClickEvent.
	 */
	if (gPendingObject)
	{
		const D3DXVECTOR3 dropped = gPendingObject->GetPos();
		const std::string name = gPendingObject->GetName();

		/* Committed: forget it without deleting it. */
		gPendingObject = edit::IMapObjRef();

		BeginPlacement(gPendingRecord, dropped);
		SetStatus("placed " + name);
		return;
	}

	gDoc.SelectMapObj(map->PickMapObj(coord));
}

}

int main(int argc, char** argv)
{
	/*
	 * Compile pipelines synchronously.
	 *
	 * d9mt hands a first-seen pipeline state to a background worker and SKIPS
	 * the draw until it is hot, silently -- the same behaviour that made the
	 * phase-7 triangle appear not to draw. ImGui's fixed-function states are
	 * new to this process, and with async on the entire UI was absent from a
	 * frame dumped 150 frames in: not late, absent.
	 *
	 * The game keeps the default, which is the right trade for something that
	 * renders continuously and can afford a frame of missing geometry. An
	 * editor whose panels appear when they feel like it is not usable, and its
	 * frame budget does not matter. Not overwritten, so D9MT_ASYNC=1 still
	 * reproduces the old behaviour.
	 */
	setenv("D9MT_ASYNC", "0", 0);

	/* Assets are resolved relative to the executable, as in the game's shell. */
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}

	gWindow = SDL_CreateWindow("RRR3D Map Editor", cWidth, cHeight,
		SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE);
	if (!gWindow)
	{
		std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return 1;
	}

	gMetalView = SDL_Metal_CreateView(gWindow);
	gLayer = SDL_Metal_GetLayer(gMetalView);

	int pixelW = 0, pixelH = 0;
	SDL_GetWindowSizeInPixels(gWindow, &pixelW, &pixelH);
	RegisterClientSize(static_cast<HWND>(gLayer), pixelW, pixelH);

	game::IView::Desc desc;
	desc.handle = static_cast<HWND>(gLayer);
	desc.resolution = lsl::Point(pixelW, pixelH);
	desc.fullscreen = false;

	/* false, and RunWorldEdit -- the two calls that make this the editor. */
	gWorld = r3d::CreateWorld(desc, false);
	if (!gWorld)
	{
		std::fprintf(stderr, "CreateWorld failed\n");
		return 1;
	}
	gWorld->RunWorldEdit();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	/* Deliberately NOT ViewportsEnable: a detached window would need its own
	   swapchain, and this device is bound to one CAMetalLayer. */

	ImGui::StyleColorsDark();
	ImGui_ImplSDL3_InitForOther(gWindow);
	/* The D3D9 backend starts on the first overlay draw -- see EditorOverlay. */

	gWorld->SetOverlay(&gOverlay);

	if (const char* open = std::getenv("RRR3D_EDITOR_OPEN"))
	{
		gDoc.path = open;
		gWorld->LoadLevel(gDoc.path);
		SetStatus("opened " + gDoc.path);
	}

	if (const char* check = std::getenv("RRR3D_EDITOR_CHECK"))
	{
		int rc = 1;
		if (std::strcmp(check, "roundtrip") == 0)
			rc = RunRoundTripCheck();
		else
			std::fprintf(stderr, "unknown RRR3D_EDITOR_CHECK: %s\n", check);

		/*
		 * The same teardown as the normal exit, in the same order. Written
		 * short the first time -- no Metal view, no window -- and it crashed
		 * after the check had already passed, so the exit status said failure
		 * about work that had succeeded. A test whose status is wrong either
		 * way is worse than no test.
		 */
		gWorld->SetOverlay(NULL);

		if (gOverlay.started())
			ImGui_ImplDX9_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		ReleaseEditorRefs();
		r3d::ReleaseWorld(gWorld);

		SDL_Metal_DestroyView(gMetalView);
		SDL_DestroyWindow(gWindow);
		SDL_Quit();
		return rc;
	}

	const long frameLimit = [] {
		const char* v = std::getenv("RRR3D_EDITOR_FRAMES");
		return v ? std::strtol(v, NULL, 10) : 0L;
	}();

	long frames = 0;
	bool running = true;

	while (running && !gQuit && !gWorld->IsTerminate())
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);

			if (event.type == SDL_EVENT_QUIT)
				running = false;

			/* ImGui gets first refusal. Without this a click on a pane would
			   also pick in the scene behind it. */
			if (io.WantCaptureMouse &&
				(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
				 event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
				 event.type == SDL_EVENT_MOUSE_MOTION))
				continue;

			if (io.WantCaptureKeyboard &&
				(event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP))
				continue;

			switch (event.type)
			{
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				if (event.button.button == SDL_BUTTON_LEFT)
					OnViewportClick(lsl::Point(int(event.button.x), int(event.button.y)),
						event.type == SDL_EVENT_MOUSE_BUTTON_DOWN, false, false);
				break;

			case SDL_EVENT_KEY_DOWN:
				/* Escape abandons a placement, which is the only way out of
				   link mode that does not leave an object behind. */
				if (event.key.key == SDLK_ESCAPE && gPendingObject)
				{
					CancelPlacement();
					SetStatus("placement cancelled");
				}
				break;

			case SDL_EVENT_MOUSE_MOTION:
				gWorld->GetView()->OnMouseMoveEvent(
					lsl::Point(int(event.motion.x), int(event.motion.y)), false, false);
				break;

			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			case SDL_EVENT_WINDOW_RESIZED:
			{
				int w = 0, h = 0;
				SDL_GetWindowSizeInPixels(gWindow, &w, &h);
				RegisterClientSize(static_cast<HWND>(gLayer), w, h);
				gWorld->OnDisplayChange();
				break;
			}

			default:
				break;
			}
		}

		/*
		 * The very first frame runs the engine alone.
		 *
		 * The D3D9 backend can only be initialised from inside the overlay,
		 * because that is the one place the device appears -- and
		 * ImGui_ImplDX9_NewFrame asserts if it has not been. So the first
		 * MainProgress reaches OnDraw, which starts the backend and draws
		 * nothing; every frame after this behaves normally.
		 */
		if (!gOverlay.started())
		{
			gWorld->MainProgress();
			continue;
		}

		ProcessPendingFile();

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		BuildUi();
		ImGui::Render();

		/* The engine's frame, which ends by calling the overlay and presenting. */
		gWorld->MainProgress();

		if (frameLimit && ++frames >= frameLimit)
			running = false;
	}

	gWorld->SetOverlay(NULL);

	if (gOverlay.started())
		ImGui_ImplDX9_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	ReleaseEditorRefs();
	r3d::ReleaseWorld(gWorld);

	SDL_Metal_DestroyView(gMetalView);
	SDL_DestroyWindow(gWindow);
	SDL_Quit();
	return 0;
}
