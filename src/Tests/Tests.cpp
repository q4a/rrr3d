/*
 * Runtime checks for the two substrate libraries the whole port stands on:
 * XPlatform's Win32 semantics and MathLib's D3DX math.
 *
 * These are PROPERTY checks, not golden values. A golden value copied out of a
 * run of the code it is testing agrees with whatever that code does, including
 * when it is wrong -- so what is asserted here is arithmetic that has to hold
 * regardless of implementation: M times its inverse is the identity, a
 * round-tripped string is the original string, a recursive lock can be taken
 * twice.
 *
 * Dependency-free by design: no window, no device, no game. This runs on a
 * machine with no display and it runs before any of the graphics port exists.
 */

#include <windows.h>

#include "MathCommon.h"
#include "lslMath.h"
/* Header-only, so no link dependency on LexStd comes with it. */
#include "lslUtility.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace
{

int gFailures = 0;
int gChecks = 0;

void Check(bool condition, const char* what)
{
	++gChecks;
	if (!condition)
	{
		++gFailures;
		std::printf("  FAIL  %s\n", what);
	}
}

void CheckNear(float actual, float expected, float tolerance, const char* what)
{
	++gChecks;
	if (!(std::fabs(actual - expected) <= tolerance))
	{
		++gFailures;
		std::printf("  FAIL  %s: expected %g, got %g\n", what, expected, actual);
	}
}

/* ------------------------------------------------------------- XPlatform --- */

/*
 * Win32 critical sections are RECURSIVE. The engine nests locks -- a locked
 * method calling another locked method on the same object -- so a plain
 * std::mutex here would deadlock on the second acquisition rather than fail
 * visibly, which is the worst way for this to be wrong.
 */
void TestCriticalSectionRecurses()
{
	std::printf("critical sections\n");

	CRITICAL_SECTION section;
	InitializeCriticalSection(&section);

	EnterCriticalSection(&section);
	EnterCriticalSection(&section);
	Check(true, "a critical section can be entered twice from one thread");
	LeaveCriticalSection(&section);
	LeaveCriticalSection(&section);

	Check(TryEnterCriticalSection(&section) != FALSE,
	      "and is free again once fully left");
	LeaveCriticalSection(&section);

	DeleteCriticalSection(&section);
}

/*
 * Auto-reset events release one waiter and clear; manual-reset events stay
 * signalled. The engine's loader uses both, and confusing them either wakes
 * everything at once or wakes nothing a second time.
 */
void TestEventResetSemantics()
{
	std::printf("events\n");

	HANDLE autoReset = CreateEventA(NULL, FALSE, TRUE, NULL);
	Check(WaitForSingleObject(autoReset, 0) == WAIT_OBJECT_0,
	      "an auto-reset event starts signalled when asked to");
	Check(WaitForSingleObject(autoReset, 0) == WAIT_TIMEOUT,
	      "and clears itself after one wait");
	CloseHandle(autoReset);

	HANDLE manualReset = CreateEventA(NULL, TRUE, TRUE, NULL);
	Check(WaitForSingleObject(manualReset, 0) == WAIT_OBJECT_0,
	      "a manual-reset event starts signalled");
	Check(WaitForSingleObject(manualReset, 0) == WAIT_OBJECT_0,
	      "and stays signalled until reset");
	ResetEvent(manualReset);
	Check(WaitForSingleObject(manualReset, 0) == WAIT_TIMEOUT,
	      "and is clear after ResetEvent");
	CloseHandle(manualReset);

	/* WAIT_TIMEOUT must be distinguishable from success, and WAIT_OBJECT_0 is
	   zero -- callers test for it in ways that depend on that. */
	Check(WAIT_OBJECT_0 == 0, "WAIT_OBJECT_0 is zero");
	Check(WAIT_TIMEOUT != WAIT_OBJECT_0, "WAIT_TIMEOUT is not success");
}

/* A signal from another thread wakes a blocked waiter. */
void TestEventAcrossThreads()
{
	std::printf("events across threads\n");

	HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);

	std::thread signaller([event]
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		SetEvent(event);
	});

	Check(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0,
	      "a blocked waiter is woken by another thread");

	signaller.join();
	CloseHandle(event);
}

/*
 * UTF-8 round trips, including non-ASCII. This is what the CP1251-to-UTF-8
 * transcode rests on: wchar_t is 32 bits here and 16 on Windows, so the
 * conversion cannot be a cast either way.
 */
void TestStringConversion()
{
	std::printf("string conversion\n");

	const char* samples[] =
	{
		"",
		"plain ascii",
		"\xd0\xa0\xd0\xb0\xd0\xb1\xd0\xbe\xd1\x82\xd0\xb0",   /* Russian */
		"\xc3\xa7\xc3\xa3o",                                   /* Portuguese */
		"\xe2\x82\xac",                                        /* three-byte */
		"\xf0\x9f\x8f\x8e",                                    /* four-byte */
	};

	for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
	{
		wchar_t wide[256];
		char narrow[256];

		const int wideLen = MultiByteToWideChar(CP_UTF8, 0, samples[i], -1, wide,
		                                        sizeof(wide) / sizeof(wide[0]));
		Check(wideLen > 0, "MultiByteToWideChar succeeds");

		const int narrowLen = WideCharToMultiByte(CP_UTF8, 0, wide, -1, narrow,
		                                          sizeof(narrow), NULL, NULL);
		Check(narrowLen > 0, "WideCharToMultiByte succeeds");

		char label[128];
		std::snprintf(label, sizeof(label), "UTF-8 round trip preserves sample %d",
		              static_cast<int>(i));
		Check(std::strcmp(samples[i], narrow) == 0, label);
	}

	/* A four-byte sequence is one code point, not a surrogate pair: wchar_t is
	   32 bits here, and assuming otherwise is what breaks on the other side. */
	wchar_t wide[8];
	MultiByteToWideChar(CP_UTF8, 0, "\xf0\x9f\x8f\x8e", -1, wide, 8);
	Check(wide[0] == 0x1F3CE && wide[1] == 0,
	      "a non-BMP character is one 32-bit wchar_t, not a surrogate pair");
}

/*
 * UTF-16LE decoding, which is a file format and not a compiler property.
 *
 * The game's language files (Data/english.txt and the rest) are UTF-16LE with a
 * BOM. The original code cast the byte buffer to wchar_t* -- correct only where
 * wchar_t is 16 bits, which is Windows and nowhere else. With a 32-bit wchar_t
 * every pair of code units becomes one nonsense character, and the failure is
 * silent: the file still parses, the string table still fills, and the menu
 * shows its internal ids instead of any text.
 */
void TestUtf16Decoding()
{
	std::printf("UTF-16LE decoding\n");

	/* BOM + "svExit" as the files actually store it. */
	const char withBom[] =
		"\xff\xfe" "s\0v\0" "E\0x\0i\0t\0";
	Check(lsl::ConvertUtf16LEToA(withBom, sizeof(withBom) - 1) == "svExit",
	      "a BOM is consumed and ASCII decodes");

	const char noBom[] = "O\0K\0";
	Check(lsl::ConvertUtf16LEToA(noBom, sizeof(noBom) - 1) == "OK",
	      "and a file without a BOM still decodes");

	Check(lsl::ConvertUtf16LEToA("", 0).empty(), "empty input gives empty output");

	/* U+0420 U+0430 -- Russian, two units, and the reason a byte-wise reading
	   cannot be substituted for this. */
	const char cyrillic[] = "\x20\x04\x30\x04";
	Check(lsl::ConvertUtf16LEToA(cyrillic, 4) == "\xd0\xa0\xd0\xb0",
	      "a two-byte code unit becomes its UTF-8 encoding");

	/* U+1F3CE, stored as the surrogate pair D83C DFCE. Combining these is the
	   whole difference between UTF-16 and "an array of 16-bit characters". */
	const char surrogatePair[] = "\x3c\xd8\xce\xdf";
	Check(lsl::ConvertUtf16LEToA(surrogatePair, 4) == "\xf0\x9f\x8f\x8e",
	      "a surrogate pair becomes one code point");

	/* A high surrogate with nothing after it is malformed input, not a
	   character -- it must not be passed through as if it were one. */
	const char loneHigh[] = "\x3c\xd8";
	Check(lsl::ConvertUtf16LEToA(loneHigh, 2) == "\xef\xbf\xbd",
	      "a lone surrogate becomes U+FFFD");

	/* An odd trailing byte is dropped rather than read past the end. */
	const char odd[] = "A\0B";
	Check(lsl::ConvertUtf16LEToA(odd, 3) == "A",
	      "a trailing half code unit is discarded");
}

/*
 * RandomRange must stay inside its range, which is not a formality: callers
 * index containers with the result.
 *
 * RAND_MAX is 32767 on MSVC and 2147483647 here, and at the large value both
 * ends of the original expression overflowed a signed int -- the divisor to
 * INT_MIN, and rand() * span to anywhere at all. AICar.cpp:369 picks a weapon
 * out of a list with this, so an out-of-range draw was a wild pointer and a
 * crash several frames later, inside the AI, with nothing pointing here.
 *
 * A property test rather than fixed values: the point is that no draw escapes,
 * so the check is over many draws and over the small ranges the game actually
 * asks for. A single sample would pass against the broken version most of the
 * time.
 */
void TestRandomRangeStaysInRange()
{
	std::printf("RandomRange bounds\n");

	std::srand(12345);

	bool inRange = true;
	int worst = 0;

	/* Small spans, because that is what indexing a weapon list looks like. */
	for (int to = 0; to <= 8 && inRange; ++to)
	{
		for (int draw = 0; draw < 20000; ++draw)
		{
			const int value = RandomRange(0, to);
			if (value < 0 || value > to)
			{
				inRange = false;
				worst = value;
				break;
			}
		}
	}

	Check(inRange, "RandomRange(0, n) stays within [0, n] over 180000 draws");
	if (!inRange)
		std::printf("        escaped with %d\n", worst);

	/* Negative and offset ranges are used too, and have the same failure. */
	bool offsetInRange = true;
	for (int draw = 0; draw < 20000; ++draw)
	{
		const int value = RandomRange(-5, 5);
		if (value < -5 || value > 5)
		{
			offsetInRange = false;
			break;
		}
	}
	Check(offsetInRange, "and within [-5, 5] for a range spanning zero");

	/* Both ends are reachable -- a generator that never returns `to` would
	   pass the bounds check and still be wrong. */
	bool sawLow = false, sawHigh = false;
	for (int draw = 0; draw < 20000 && !(sawLow && sawHigh); ++draw)
	{
		const int value = RandomRange(0, 3);
		sawLow = sawLow || value == 0;
		sawHigh = sawHigh || value == 3;
	}
	Check(sawLow && sawHigh, "both ends of the range are reachable");

	/* A degenerate range is the one value, not a coin toss. */
	Check(RandomRange(7, 7) == 7, "a single-value range returns that value");
}

/* MulDiv rounds to nearest away from zero, which is not what integer division
   does -- and the difference is a pixel of font height at some DPI values. */
void TestMulDiv()
{
	std::printf("MulDiv\n");

	Check(MulDiv(9, 96, 72) == 12, "MulDiv(9, 96, 72) is 12");
	Check(MulDiv(1, 1, 2) == 1, "a half rounds away from zero");
	Check(MulDiv(-1, 1, 2) == -1, "and does so for negatives too");
	Check(MulDiv(1, 2, 3) == 1, "two thirds rounds down");
	Check(MulDiv(2, 2, 3) == 1, "four thirds rounds down");
	Check(MulDiv(1, 1, 0) == -1, "a zero denominator returns -1 rather than trapping");
}

/*
 * The keyboard table, both directions.
 *
 * Round trip only for keys with one scancode: VK_RETURN maps to two, so it
 * comes back as the main Return rather than the keypad one, and asserting a
 * strict round trip there would be asserting something false.
 */
void TestKeyboardMapping()
{
	std::printf("keyboard mapping\n");

	const int keys[] =
	{
		VK_BACK, VK_ESCAPE, VK_SPACE, VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,
		VK_DELETE, VK_PRIOR, VK_NEXT, VK_F1, VK_F7, VK_ADD, VK_SUBTRACT,
		VK_NUMPAD0, VK_NUMPAD9, VK_OEM_PERIOD,
		'A', 'W', 'S', 'D', 'Z', '0', '1', '9',
	};

	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		const int scancode = ScancodeFromVirtualKey(keys[i]);

		char label[96];
		std::snprintf(label, sizeof(label), "virtual key %d has a scancode", keys[i]);
		Check(scancode != 0, label);

		std::snprintf(label, sizeof(label), "virtual key %d round trips", keys[i]);
		Check(VirtualKeyFromScancode(scancode) == keys[i], label);
	}

	/* Both directions agree that an unmapped key is unmapped. */
	Check(VirtualKeyFromScancode(0) == 0, "an unknown scancode maps to no virtual key");
	Check(ScancodeFromVirtualKey(0xFE) == 0, "an unknown virtual key maps to no scancode");

	/* The letters are contiguous in both alphabets, which is what lets them be
	   handled by range rather than by table. */
	Check(VirtualKeyFromScancode(ScancodeFromVirtualKey('A')) == 'A', "A");
	Check(VirtualKeyFromScancode(ScancodeFromVirtualKey('Z')) == 'Z', "Z");
	Check(ScancodeFromVirtualKey('B') == ScancodeFromVirtualKey('A') + 1,
	      "letters are contiguous scancodes");
}

/*
 * The client-size registry, which is what GetClientRect answers from because
 * the HWND is a CAMetalLayer and cannot be asked.
 */
void TestClientSizeRegistry()
{
	std::printf("client size\n");

	/* A stand-in handle: nothing dereferences it, which is the point. */
	HWND window = reinterpret_cast<HWND>(0x1234);

	RECT rect;
	Check(GetClientRect(window, &rect) == FALSE,
	      "an unregistered window reports failure");
	Check(rect.right == 0 && rect.bottom == 0,
	      "and zeroes the rect rather than leaving stack contents");

	RegisterClientSize(window, 1280, 720);
	Check(GetClientRect(window, &rect) != FALSE, "a registered window succeeds");
	Check(rect.left == 0 && rect.top == 0, "the client rect starts at the origin");
	Check(rect.right == 1280 && rect.bottom == 720, "and carries the published size");

	/* Re-registering updates rather than duplicating -- the shell does this on
	   every resize. */
	RegisterClientSize(window, 1920, 1080);
	GetClientRect(window, &rect);
	Check(rect.right == 1920 && rect.bottom == 1080, "re-registering updates the size");

	/* A different window is independent. */
	HWND other = reinterpret_cast<HWND>(0x5678);
	RegisterClientSize(other, 640, 480);
	GetClientRect(window, &rect);
	Check(rect.right == 1920, "one window's size does not disturb another's");
	GetClientRect(other, &rect);
	Check(rect.right == 640 && rect.bottom == 480, "and the other reads its own");
}

/* --------------------------------------------------------------- D3DX math --- */

/*
 * Matrix properties rather than element-by-element expectations, because the
 * point is that Wine's implementation behaves like Microsoft's, not that it
 * produces particular floats.
 */
void TestMatrixInverse()
{
	std::printf("matrix inverse\n");

	D3DXMATRIX m;
	D3DXMatrixIdentity(&m);

	D3DXMATRIX rotation;
	D3DXMatrixRotationYawPitchRoll(&rotation, 0.3f, -0.7f, 1.1f);

	D3DXMATRIX translation;
	D3DXMatrixTranslation(&translation, 3.0f, -4.0f, 5.0f);

	D3DXMatrixMultiply(&m, &rotation, &translation);

	D3DXMATRIX inverse;
	Check(D3DXMatrixInverse(&inverse, NULL, &m) != NULL, "the matrix inverts");

	D3DXMATRIX product;
	D3DXMatrixMultiply(&product, &m, &inverse);

	for (int row = 0; row < 4; ++row)
		for (int col = 0; col < 4; ++col)
		{
			char label[64];
			std::snprintf(label, sizeof(label), "M*inv(M) identity at [%d][%d]", row, col);
			CheckNear(product.m[row][col], row == col ? 1.0f : 0.0f, 1e-4f, label);
		}
}

/*
 * D3DX is ROW-vector: a point is transformed as v*M, so the translation lives
 * in row 3. Getting this backwards transposes every transform in the game and
 * presents as objects in the wrong places rather than as a maths bug.
 */
void TestRowVectorConvention()
{
	std::printf("row-vector convention\n");

	D3DXMATRIX translation;
	D3DXMatrixTranslation(&translation, 10.0f, 20.0f, 30.0f);

	CheckNear(translation.m[3][0], 10.0f, 1e-6f, "translation x is at m[3][0]");
	CheckNear(translation.m[3][1], 20.0f, 1e-6f, "translation y is at m[3][1]");
	CheckNear(translation.m[3][2], 30.0f, 1e-6f, "translation z is at m[3][2]");

	D3DXVECTOR3 point(1.0f, 2.0f, 3.0f);
	D3DXVECTOR3 moved;
	D3DXVec3TransformCoord(&moved, &point, &translation);

	CheckNear(moved.x, 11.0f, 1e-5f, "TransformCoord applies the translation");
	CheckNear(moved.y, 22.0f, 1e-5f, "TransformCoord applies the translation");
	CheckNear(moved.z, 33.0f, 1e-5f, "TransformCoord applies the translation");

	/* A normal ignores translation, which is the whole difference between the
	   two calls. */
	D3DXVECTOR3 normal;
	D3DXVec3TransformNormal(&normal, &point, &translation);
	CheckNear(normal.x, 1.0f, 1e-5f, "TransformNormal ignores translation");
	CheckNear(normal.y, 2.0f, 1e-5f, "TransformNormal ignores translation");
	CheckNear(normal.z, 3.0f, 1e-5f, "TransformNormal ignores translation");
}

/* A right-handed projection looks down -Z, so a point in front has negative z
   in view space. The sign is what distinguishes RH from LH and picking the
   wrong one renders nothing at all. */
void TestPerspectiveHandedness()
{
	std::printf("projection handedness\n");

	D3DXMATRIX projection;
	D3DXMatrixPerspectiveFovRH(&projection, D3DX_PI / 4.0f, 16.0f / 9.0f, 1.0f, 100.0f);

	Check(projection.m[2][3] == -1.0f,
	      "PerspectiveFovRH puts -1 in m[2][3], which is what makes it right-handed");

	/* A point 10 in front of the camera projects inside the frustum. */
	D3DXVECTOR4 projected;
	D3DXVECTOR3 inFront(0.0f, 0.0f, -10.0f);
	D3DXVec3Transform(&projected, &inFront, &projection);

	Check(projected.w > 0.0f, "a point down -Z has positive w");
	CheckNear(projected.x / projected.w, 0.0f, 1e-4f, "and projects to the centre");
}

/* Slerp hits both endpoints exactly, and stays normalised in between. */
void TestQuaternionSlerp()
{
	std::printf("quaternion slerp\n");

	D3DXQUATERNION from, to;
	const D3DXVECTOR3 zAxis(0, 0, 1);
	D3DXQuaternionRotationAxis(&from, &zAxis, 0.0f);
	D3DXQuaternionRotationAxis(&to, &zAxis, D3DX_PI / 2.0f);

	D3DXQUATERNION result;

	D3DXQuaternionSlerp(&result, &from, &to, 0.0f);
	CheckNear(std::fabs(D3DXQuaternionDot(&result, &from)), 1.0f, 1e-4f,
	          "slerp at t=0 is the start");

	D3DXQuaternionSlerp(&result, &from, &to, 1.0f);
	CheckNear(std::fabs(D3DXQuaternionDot(&result, &to)), 1.0f, 1e-4f,
	          "slerp at t=1 is the end");

	for (int i = 0; i <= 10; ++i)
	{
		D3DXQuaternionSlerp(&result, &from, &to, i / 10.0f);
		CheckNear(D3DXQuaternionLength(&result), 1.0f, 1e-4f,
		          "slerp stays normalised throughout");
	}
}

/* The D3DXVECTOR4(D3DXVECTOR3, w) constructor is one of the five deviations
   the vendor script adds, because Wine lacks it and GrassField uses it. */
void TestVectorConstructors()
{
	std::printf("vector constructors\n");

	const D3DXVECTOR3 xyz(1.0f, 2.0f, 3.0f);
	const D3DXVECTOR4 v(xyz, 4.0f);

	CheckNear(v.x, 1.0f, 1e-6f, "D3DXVECTOR4(D3DXVECTOR3, w) takes x");
	CheckNear(v.y, 2.0f, 1e-6f, "D3DXVECTOR4(D3DXVECTOR3, w) takes y");
	CheckNear(v.z, 3.0f, 1e-6f, "D3DXVECTOR4(D3DXVECTOR3, w) takes z");
	CheckNear(v.w, 4.0f, 1e-6f, "D3DXVECTOR4(D3DXVECTOR3, w) takes w");

	/* PlaneDotCoord takes a D3DXVECTOR3 here, not Wine's D3DXVECTOR4 -- the
	   other deviation, and the game passes vectors. */
	D3DXPLANE plane(0.0f, 0.0f, 1.0f, -5.0f);
	const D3DXVECTOR3 point(0.0f, 0.0f, 8.0f);
	CheckNear(D3DXPlaneDotCoord(&plane, &point), 3.0f, 1e-5f,
	          "PlaneDotCoord takes a D3DXVECTOR3 and measures signed distance");
}

} /* namespace */

int main()
{
	std::printf("Tests\n================================\n");

	TestCriticalSectionRecurses();
	TestEventResetSemantics();
	TestEventAcrossThreads();
	TestStringConversion();
	TestUtf16Decoding();
	TestRandomRangeStaysInRange();
	TestMulDiv();
	TestKeyboardMapping();
	TestClientSizeRegistry();

	TestMatrixInverse();
	TestRowVectorConvention();
	TestPerspectiveHandedness();
	TestQuaternionSlerp();
	TestVectorConstructors();

	std::printf("================================\n");
	if (gFailures == 0)
		std::printf("OK: %d checks, 0 failures\n", gChecks);
	else
		std::printf("FAILED: %d of %d checks\n", gFailures, gChecks);

	return gFailures == 0 ? 0 : 1;
}
