/*
 * Runtime tests for the ported layers.
 *
 * Everything in this port so far has only been proven to *compile*. These
 * tests exist to prove the code actually computes the right answers -- the
 * vendored Wine D3DX math, and the XPlatform Win32 substitutes, none of which
 * had ever been executed.
 *
 * Deliberately dependency-free: a failure here should never be ambiguous about
 * whether the test framework or the code under test is at fault.
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "d3d/d3dx9math.h"
#include "xplatform.h"

namespace {

int g_failures = 0;
int g_checks = 0;
const char* g_section = "";

void Section(const char* name)
{
	g_section = name;
	std::printf("\n-- %s\n", name);
}

void Check(bool ok, const char* what)
{
	++g_checks;
	if (!ok)
	{
		++g_failures;
		std::printf("   FAIL  [%s] %s\n", g_section, what);
	}
}

bool Near(float a, float b, float eps = 1e-4f)
{
	return std::fabs(a - b) <= eps;
}

bool NearVec3(const D3DXVECTOR3& v, float x, float y, float z, float eps = 1e-4f)
{
	return Near(v.x, x, eps) && Near(v.y, y, eps) && Near(v.z, z, eps);
}

/* ---------------------------------------------------------------- math --- */

void TestMath()
{
	Section("D3DX math (vendored from Wine)");

	// Normalisation
	D3DXVECTOR3 v(3.0f, 4.0f, 0.0f), out;
	D3DXVec3Normalize(&out, &v);
	Check(NearVec3(out, 0.6f, 0.8f, 0.0f), "D3DXVec3Normalize(3,4,0)");
	Check(Near(D3DXVec3Length(&out), 1.0f), "normalised length is 1");

	// Cross and dot are right-handed and consistent
	D3DXVECTOR3 x(1, 0, 0), y(0, 1, 0), cross;
	D3DXVec3Cross(&cross, &x, &y);
	Check(NearVec3(cross, 0, 0, 1), "D3DXVec3Cross(x,y) == z");
	Check(Near(D3DXVec3Dot(&x, &y), 0.0f), "D3DXVec3Dot(x,y) == 0");

	// Matrix inverse really inverts: M * M^-1 == I
	D3DXMATRIX m, inv, prod;
	D3DXMatrixRotationYawPitchRoll(&m, 0.3f, -0.7f, 1.1f);
	m._41 = 5.0f; m._42 = -2.0f; m._43 = 3.5f;
	float det = 0.0f;
	Check(D3DXMatrixInverse(&inv, &det, &m) != NULL, "D3DXMatrixInverse succeeds");
	D3DXMatrixMultiply(&prod, &m, &inv);
	bool identity = true;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			identity = identity && Near(prod.m[r][c], r == c ? 1.0f : 0.0f, 1e-3f);
	Check(identity, "M * M^-1 == I");

	// Translation applies in the row-vector convention D3DX uses. If the
	// convention were flipped this would land at the origin instead.
	D3DXMATRIX t;
	D3DXMatrixTranslation(&t, 10.0f, 20.0f, 30.0f);
	D3DXVECTOR3 p(1, 2, 3), moved;
	D3DXVec3TransformCoord(&moved, &p, &t);
	Check(NearVec3(moved, 11, 22, 33), "D3DXVec3TransformCoord applies translation");

	// TransformNormal must ignore translation
	D3DXVECTOR3 normal;
	D3DXVec3TransformNormal(&normal, &p, &t);
	Check(NearVec3(normal, 1, 2, 3), "D3DXVec3TransformNormal ignores translation");

	// Transpose is an involution and actually transposes
	D3DXMATRIX tr, trtr;
	D3DXMatrixTranspose(&tr, &m);
	Check(Near(tr._12, m._21) && Near(tr._41, m._14), "D3DXMatrixTranspose swaps");
	D3DXMatrixTranspose(&trtr, &tr);
	Check(Near(trtr._12, m._12) && Near(trtr._41, m._41), "transpose is an involution");

	// Quaternion round trip: axis-angle -> quat -> matrix -> quat
	D3DXQUATERNION q, q2;
	D3DXVECTOR3 axis(0, 1, 0);
	D3DXQuaternionRotationAxis(&q, &axis, 1.2f);
	D3DXMATRIX qm;
	D3DXMatrixRotationQuaternion(&qm, &q);
	D3DXQuaternionRotationMatrix(&q2, &qm);
	// q and -q are the same rotation
	bool same = (Near(q.x, q2.x, 1e-3f) && Near(q.y, q2.y, 1e-3f) &&
	             Near(q.z, q2.z, 1e-3f) && Near(q.w, q2.w, 1e-3f)) ||
	            (Near(q.x, -q2.x, 1e-3f) && Near(q.y, -q2.y, 1e-3f) &&
	             Near(q.z, -q2.z, 1e-3f) && Near(q.w, -q2.w, 1e-3f));
	Check(same, "quat -> matrix -> quat round trip");

	// A quaternion rotation about Y by 90 degrees maps +X to -Z (right-handed)
	D3DXQuaternionRotationAxis(&q, &axis, D3DX_PI / 2.0f);
	D3DXMatrixRotationQuaternion(&qm, &q);
	D3DXVec3TransformCoord(&out, &x, &qm);
	Check(NearVec3(out, 0, 0, -1, 1e-3f), "90deg about Y maps +X to -Z");

	// Slerp endpoints must be exact, and the midpoint must be normalised
	D3DXQUATERNION qa, qb, qs;
	D3DXQuaternionRotationAxis(&qa, &axis, 0.0f);
	D3DXQuaternionRotationAxis(&qb, &axis, 1.0f);
	D3DXQuaternionSlerp(&qs, &qa, &qb, 0.0f);
	Check(Near(qs.w, qa.w, 1e-3f), "slerp(t=0) == a");
	D3DXQuaternionSlerp(&qs, &qa, &qb, 1.0f);
	Check(Near(qs.w, qb.w, 1e-3f), "slerp(t=1) == b");
	D3DXQuaternionSlerp(&qs, &qa, &qb, 0.5f);
	Check(Near(std::sqrt(qs.x * qs.x + qs.y * qs.y + qs.z * qs.z + qs.w * qs.w), 1.0f, 1e-3f),
	      "slerp midpoint stays unit length");

	// Plane maths. This is the signature we patched away from Wine's
	// D3DXVECTOR4* back to the SDK's D3DXVECTOR3*, so exercise it.
	D3DXPLANE plane;
	D3DXVECTOR3 origin(0, 0, 0), up(0, 1, 0);
	D3DXPlaneFromPointNormal(&plane, &origin, &up);
	D3DXVECTOR3 above(0, 5, 0), below(0, -3, 0);
	Check(Near(D3DXPlaneDotCoord(&plane, &above), 5.0f), "D3DXPlaneDotCoord above plane");
	Check(Near(D3DXPlaneDotCoord(&plane, &below), -3.0f), "D3DXPlaneDotCoord below plane");
	Check(Near(D3DXPlaneDotNormal(&plane, &up), 1.0f), "D3DXPlaneDotNormal");

	// Right-handed perspective: looking down -Z, a point in front should end
	// up with positive w. A left-handed matrix would give negative w.
	D3DXMATRIX proj;
	D3DXMatrixPerspectiveFovRH(&proj, D3DX_PI / 4.0f, 16.0f / 9.0f, 1.0f, 100.0f);
	D3DXVECTOR3 front(0, 0, -10);
	D3DXVECTOR4 clip;
	D3DXVec3Transform(&clip, &front, &proj);
	Check(clip.w > 0.0f, "PerspectiveFovRH gives positive w for a point in front");

	// LookAtRH: eye on +Z looking at origin leaves the target in front
	D3DXMATRIX view;
	D3DXVECTOR3 eye(0, 0, 10), at(0, 0, 0);
	D3DXMatrixLookAtRH(&view, &eye, &at, &up);
	D3DXVec3TransformCoord(&out, &at, &view);
	Check(Near(out.z, -10.0f, 1e-3f), "LookAtRH puts the target 10 units in front");

	// Half-float conversion, extracted from Wine's d3dx_helpers.c
	const float samples[] = {0.0f, 1.0f, -1.0f, 0.5f, -2.25f, 100.0f};
	bool halfOk = true;
	for (float f : samples)
	{
		D3DXFLOAT16 h;
		float back = 0.0f;
		D3DXFloat32To16Array(&h, &f, 1);
		D3DXFloat16To32Array(&back, &h, 1);
		halfOk = halfOk && Near(back, f, 0.05f);
	}
	Check(halfOk, "float32 <-> float16 round trip");
}

/* ----------------------------------------------------------- xplatform --- */

void TestTiming()
{
	Section("XPlatform timing");

	LARGE_INTEGER freq, a, b;
	Check(QueryPerformanceFrequency(&freq) && freq.QuadPart > 0, "QueryPerformanceFrequency > 0");

	QueryPerformanceCounter(&a);
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	QueryPerformanceCounter(&b);

	const double elapsed = double(b.QuadPart - a.QuadPart) / double(freq.QuadPart);
	Check(b.QuadPart > a.QuadPart, "QueryPerformanceCounter advances");
	Check(elapsed > 0.02 && elapsed < 0.5, "QPC/QPF elapsed is ~30ms (scaled correctly)");

	const DWORD t0 = GetTickCount();
	Sleep(30);
	const DWORD t1 = GetTickCount();
	Check(t1 >= t0 + 20, "GetTickCount advances across Sleep");
}

void TestCriticalSection()
{
	Section("XPlatform critical sections");

	RTL_CRITICAL_SECTION cs;
	InitializeCriticalSection(&cs);

	// Win32 critical sections are recursive and the engine nests locks.
	// A plain std::mutex here would deadlock and this test would hang.
	EnterCriticalSection(&cs);
	EnterCriticalSection(&cs);
	LeaveCriticalSection(&cs);
	LeaveCriticalSection(&cs);
	Check(true, "recursive acquisition does not deadlock");

	// Mutual exclusion actually excludes: without it this races and the
	// counter lands below the expected total.
	int counter = 0;
	std::vector<std::thread> threads;
	for (int i = 0; i < 4; ++i)
		threads.emplace_back([&cs, &counter] {
			for (int n = 0; n < 20000; ++n)
			{
				EnterCriticalSection(&cs);
				++counter;
				LeaveCriticalSection(&cs);
			}
		});
	for (auto& t : threads)
		t.join();
	Check(counter == 4 * 20000, "mutual exclusion holds under contention");

	DeleteCriticalSection(&cs);
}

void TestEvents()
{
	Section("XPlatform events");

	// Auto-reset: signalling wakes a waiter and clears the signal
	HANDLE autoEv = CreateEvent(NULL, FALSE, FALSE, NULL);
	Check(WaitForSingleObject(autoEv, 10) == WAIT_TIMEOUT, "unsignalled auto event times out");
	SetEvent(autoEv);
	Check(WaitForSingleObject(autoEv, 100) == WAIT_OBJECT_0, "signalled auto event wakes");
	Check(WaitForSingleObject(autoEv, 10) == WAIT_TIMEOUT, "auto event resets after waking");
	CloseHandle(autoEv);

	// Manual-reset: stays signalled until explicitly reset
	HANDLE manualEv = CreateEvent(NULL, TRUE, FALSE, NULL);
	SetEvent(manualEv);
	Check(WaitForSingleObject(manualEv, 100) == WAIT_OBJECT_0, "manual event wakes");
	Check(WaitForSingleObject(manualEv, 10) == WAIT_OBJECT_0, "manual event stays signalled");
	ResetEvent(manualEv);
	Check(WaitForSingleObject(manualEv, 10) == WAIT_TIMEOUT, "manual event clears on reset");
	CloseHandle(manualEv);

	// Cross-thread signalling, which is what the audio streaming path does
	HANDLE ev = CreateEvent(NULL, FALSE, FALSE, NULL);
	std::thread signaller([ev] {
		std::this_thread::sleep_for(std::chrono::milliseconds(40));
		SetEvent(ev);
	});
	Check(WaitForSingleObject(ev, 2000) == WAIT_OBJECT_0, "event signalled from another thread");
	signaller.join();
	CloseHandle(ev);
}

DWORD WorkItem(void* context)
{
	*static_cast<int*>(context) = 42;
	return 0;
}

void TestThreadPool()
{
	Section("XPlatform thread pool");

	int result = 0;
	HANDLE done = CreateEvent(NULL, TRUE, FALSE, NULL);

	struct Ctx { int* out; HANDLE done; } ctx{&result, done};

	Check(QueueUserWorkItem([](void* c) -> DWORD {
		Ctx* k = static_cast<Ctx*>(c);
		*k->out = 42;
		SetEvent(k->done);
		return 0;
	}, &ctx, WT_EXECUTELONGFUNCTION) == TRUE, "QueueUserWorkItem accepts the item");

	Check(WaitForSingleObject(done, 5000) == WAIT_OBJECT_0, "queued work item ran");
	Check(result == 42, "queued work item saw its context");
	CloseHandle(done);

	(void)&WorkItem;
}

void TestEncoding()
{
	Section("XPlatform encoding conversion");

	// ASCII round trip
	const char* ascii = "Data/Shaders/model.fx";
	wchar_t wide[256] = {};
	int n = MultiByteToWideChar(CP_ACP, 0, ascii, -1, wide, 256);
	Check(n > 0, "MultiByteToWideChar returns a length");
	Check(std::wcslen(wide) == std::strlen(ascii), "wide length matches for ASCII");

	char back[256] = {};
	int m = WideCharToMultiByte(CP_ACP, 0, wide, -1, back, 256, NULL, NULL);
	Check(m > 0 && std::strcmp(back, ascii) == 0, "ASCII survives the round trip");

	// Non-ASCII round trip. The game's data paths and log strings are not all
	// ASCII, so a converter that only handled ASCII would pass the test above
	// and still corrupt real input.
	const char* utf8 = "\xD0\x9C\xD0\xBE\xD1\x82\xD0\xBE\xD1\x80";  // "Мотор"
	wchar_t wideU[64] = {};
	Check(MultiByteToWideChar(CP_ACP, 0, utf8, -1, wideU, 64) > 0, "non-ASCII converts to wide");
	char backU[64] = {};
	Check(WideCharToMultiByte(CP_ACP, 0, wideU, -1, backU, 64, NULL, NULL) > 0,
	      "non-ASCII converts back");
	Check(std::strcmp(backU, utf8) == 0, "non-ASCII survives the round trip");

	// Length query with a null buffer, which is how callers size allocations
	Check(MultiByteToWideChar(CP_ACP, 0, ascii, -1, NULL, 0) > 0, "length query returns a size");
}

void TestFilesystem()
{
	Section("XPlatform filesystem");

	wchar_t exePath[1024] = {};
	const DWORD len = GetModuleFileNameW(NULL, exePath, 1024);
	Check(len > 0, "GetModuleFileNameW returns a path");
	Check(exePath[0] == L'/', "executable path is absolute");

	Check(GetFileAttributesW(exePath) != INVALID_FILE_ATTRIBUTES, "our own binary exists");
	Check((GetFileAttributesW(exePath) & FILE_ATTRIBUTE_DIRECTORY) == 0, "and is not a directory");

	const wchar_t* missing = L"/definitely/not/here/rrr3d-missing";
	Check(GetFileAttributesW(missing) == INVALID_FILE_ATTRIBUTES, "missing path reports invalid");

	const wchar_t* tmp = L"/tmp";
	Check((GetFileAttributesW(tmp) & FILE_ATTRIBUTE_DIRECTORY) != 0, "/tmp is a directory");
}

} // namespace

int main()
{
	std::printf("rrr3d port tests\n================");

	TestMath();
	TestTiming();
	TestCriticalSection();
	TestEvents();
	TestThreadPool();
	TestEncoding();
	TestFilesystem();

	std::printf("\n%d checks, %d failure%s\n",
	            g_checks, g_failures, g_failures == 1 ? "" : "s");
	return g_failures == 0 ? 0 : 1;
}
