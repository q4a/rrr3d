/*
 * Behavioural checks for the vehicle model, offscreen and without the game.
 *
 * The vehicle model is the one part of this port that cannot be judged by
 * looking at it. A screenshot of a stationary car says nothing about whether a
 * spring rate is right, and the only readings the game shows -- Speed = 0,
 * wheel0 lat=0 long=0 -- are equally consistent with "the car is parked" and
 * "no tire force exists". So this drops a car on a plane, drives it with a
 * fixed input sequence, and asserts things that must be true of any working
 * vehicle whatever the tuning:
 *
 *   - it settles to a ride height and stays there, without sinking or jittering
 *   - every wheel reports ground contact on the ground, and none of it in air
 *   - motor torque accelerates it, brake torque stops it
 *   - rolling straight produces no lateral slip
 *
 * These are deliberately NOT a parity test against PhysX 2.8. Parity is not
 * available: extern/physx is headers only, 2.8.4 is closed source with no arm64
 * macOS build, and the pre-port tree that could link it only builds under MSVC.
 * Two different solvers would diverge on the third step of any scenario anyway.
 * What is worth asserting is behaviour a driver would notice, which is what
 * these are.
 *
 * Deliberately dependency-free in the same spirit as Tests/TestMain.cpp: a
 * failure here should never be ambiguous about whose fault it is.
 */

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "px/Physx.h"

namespace {

int g_failures = 0;
int g_checks = 0;
const char* g_section = "";

void Section(const char* name)
{
	g_section = name;
	std::printf("\n-- %s\n", name);
}

void Check(bool ok, const std::string& what)
{
	++g_checks;
	if (!ok)
	{
		++g_failures;
		std::printf("   FAIL  [%s] %s\n", g_section, what.c_str());
	}
}

std::string Fmt(const char* fmt, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	return std::string(buffer);
}

/*
 * The ground.
 *
 * Scene::CreateGroundPlane exists but its body is commented out -- it was dead
 * in the 2.8 code too, because the game's ground is the track's triangle mesh.
 * So the harness brings its own: a very wide, very thick static box centred
 * below the origin, with its top face at z = 0.
 */
class Ground: public r3d::px::ActorUser
{
public:
	Ground(r3d::px::Scene* scene)
	{
		_actor = new r3d::px::Actor(this);
		//No body: an actor without one is static, which is the 2.8
		//discriminator this port kept.
		_actor->SetPos(D3DXVECTOR3(0.0f, 0.0f, -5.0f));

		r3d::px::BoxShape& box = _actor->GetShapes().Add<r3d::px::BoxShape>();
		box.SetDimensions(D3DXVECTOR3(500.0f, 500.0f, 5.0f));

		//Scene last: see the note in TestCar.
		_actor->SetScene(scene);
	}

	~Ground() { delete _actor; }

	virtual void OnContact(const r3d::px::Scene::OnContactEvent&) {}
	virtual bool OnContactModify(const r3d::px::Scene::OnContactModifyEvent&) { return true; }

private:
	r3d::px::Actor* _actor;
};

/*
 * A car: one box chassis and four wheels, close enough to the shipped ones that
 * the numbers mean something. The values come from Data/Car/*Wheel.txt as the
 * game's own debug overlay prints them.
 */
class TestCar: public r3d::px::ActorUser
{
public:
	static const int cWheelCount = 4;

	TestCar(r3d::px::Scene* scene, const D3DXVECTOR3& pos)
		: _actor(0)
	{
		_actor = new r3d::px::Actor(this);

		//SetScene comes last, after the body and every shape, and both of those
		//matter:
		//
		//  - SetScene is what creates the PxRigidActor, and static-versus-
		//    dynamic is decided there by whether a body exists. SetBody only
		//    reloads the actor when a body is *removed*, so adding one
		//    afterwards leaves a static actor that silently never moves.
		//  - InitRootNxActor returns early when the actor has no shapes, so
		//    setting the scene first means no actor is created at all and every
		//    later shape attaches to nothing.
		//
		//Both are inherited from the 2.8 code unchanged. Neither reports
		//anything; the symptom is a car that sits exactly where it was put.
		r3d::px::BodyDesc body;
		body.mass = 2000.0f;
		_actor->SetBody(&body);
		_actor->SetPos(pos);

		//Chassis. Half-extents, so a 4 x 2 x 0.6 metre car.
		r3d::px::BoxShape& chassis = _actor->GetShapes().Add<r3d::px::BoxShape>();
		chassis.SetDimensions(D3DXVECTOR3(2.0f, 1.0f, 0.3f));

		//Wheels at the corners, Z up.
		const float halfLength = 1.5f;
		const float halfWidth = 0.9f;
		const float axleHeight = -0.3f;

		for (int i = 0; i < cWheelCount; ++i)
		{
			const float x = (i < 2) ? halfLength : -halfLength;
			const float y = (i % 2 == 0) ? halfWidth : -halfWidth;

			r3d::px::WheelShape& wheel = _actor->GetShapes().Add<r3d::px::WheelShape>();
			wheel.SetPos(D3DXVECTOR3(x, y, axleHeight));
			wheel.SetRadius(0.4f);
			wheel.SetSuspensionTravel(0.1f);

			r3d::px::SpringDesc suspension;
			suspension.spring = 625000.0f;
			suspension.damper = 25000.0f;
			suspension.targetValue = 0.0f;
			wheel.SetSuspension(suspension);

			r3d::px::TireFunctionDesc longitudinal;
			longitudinal.extremumSlip = 0.3f;
			longitudinal.extremumValue = 7.0f;
			longitudinal.asymptoteSlip = 3.0f;
			longitudinal.asymptoteValue = 6.4f;
			wheel.SetLongitudalTireForceFunction(longitudinal);

			r3d::px::TireFunctionDesc lateral;
			lateral.extremumSlip = 0.1f;
			lateral.extremumValue = 6.5f;
			lateral.asymptoteSlip = 0.4f;
			lateral.asymptoteValue = 3.0f;
			wheel.SetLateralTireForceFunction(lateral);

			//1 / 20 kg
			wheel.SetInverseWheelMass(1.0f / 20.0f);

			_wheels.push_back(&wheel);
		}

		_actor->SetScene(scene);
	}

	virtual ~TestCar()
	{
		delete _actor;
	}

	//ActorUser: this harness does not care about contact events.
	virtual void OnContact(const r3d::px::Scene::OnContactEvent&) {}
	virtual bool OnContactModify(const r3d::px::Scene::OnContactModifyEvent&) { return true; }

	void SetMotorTorque(float value)
	{
		for (size_t i = 0; i < _wheels.size(); ++i)
			_wheels[i]->SetMotorTorque(value);
	}

	void SetBrakeTorque(float value)
	{
		for (size_t i = 0; i < _wheels.size(); ++i)
			_wheels[i]->SetBrakeTorque(value);
	}

	D3DXVECTOR3 GetPos() const { return _actor->GetPos(); }

	bool IsDynamic() const { return _actor->GetNxDynamic() != 0; }

	D3DXVECTOR3 GetVelocity() const
	{
		physx::PxRigidDynamic* dynamic = _actor->GetNxDynamic();
		if (!dynamic)
			return D3DXVECTOR3(0.0f, 0.0f, 0.0f);

		const physx::PxVec3 v = dynamic->getLinearVelocity();
		return D3DXVECTOR3(v.x, v.y, v.z);
	}

	int CountWheelsOnGround() const
	{
		int count = 0;
		for (size_t i = 0; i < _wheels.size(); ++i)
		{
			r3d::px::WheelContactData contact;
			if (_wheels[i]->GetContact(contact))
				++count;
		}
		return count;
	}

	float MaxAbsLateralSlip() const
	{
		float worst = 0.0f;
		for (size_t i = 0; i < _wheels.size(); ++i)
		{
			r3d::px::WheelContactData contact;
			if (_wheels[i]->GetContact(contact))
				worst = std::max(worst, std::fabs(contact.lateralSlip));
		}
		return worst;
	}

	r3d::px::WheelShape* GetWheel(int index) { return _wheels[index]; }

private:
	r3d::px::Actor* _actor;
	std::vector<r3d::px::WheelShape*> _wheels;
};

const float cStep = 1.0f / 60.0f;

void Step(r3d::px::Manager& manager, int steps)
{
	for (int i = 0; i < steps; ++i)
		manager.Compute(cStep);
}

/* ------------------------------------------------------------ scenarios --- */

/*
 * Before anything is asserted about a car, establish that the harness itself
 * simulates. Every check below is meaningless if the body never moves, and a
 * stationary body passes most of them by accident -- "does not sink", "does not
 * jitter" and "is at rest" are all true of an actor that is not simulating.
 */
void TestHarnessSimulates(r3d::px::Manager& manager, r3d::px::Scene* scene)
{
	Section("the harness itself simulates");

	Ground ground(scene);
	TestCar car(scene, D3DXVECTOR3(0.0f, 0.0f, 20.0f));

	Check(car.IsDynamic(), "the car body is a dynamic actor");

	const float startZ = car.GetPos().z;
	Step(manager, 10);
	const float afterZ = car.GetPos().z;

	Check(afterZ < startZ - 0.05f,
		Fmt("a body in the air falls (z %.3f -> %.3f over 10 steps)", startZ, afterZ));

	std::printf("   ... z after 10 steps: %.3f, velocity %.3f m/s\n",
		afterZ, D3DXVec3Length(&car.GetVelocity()));
}

void TestRestsOnGround(r3d::px::Manager& manager, r3d::px::Scene* scene)
{
	Section("a car dropped on a plane settles and stays there");

	Ground ground(scene);
	TestCar car(scene, D3DXVECTOR3(0.0f, 0.0f, 1.0f));

	//Two seconds is long enough for a 625 kN/m spring to stop ringing.
	Step(manager, 120);
	const float settled = car.GetPos().z;

	Check(settled > 0.0f, Fmt("does not sink through the ground (z = %.3f)", settled));
	Check(settled < 2.0f, Fmt("does not launch itself (z = %.3f)", settled));

	//A wheel of radius 0.4 with its axle 0.3 below the body centre puts the
	//body around 0.7 up. Generous bounds: this is a sanity check on the
	//suspension existing at all, not on its tuning.
	Check(settled > 0.3f && settled < 1.2f,
		Fmt("rests at a plausible ride height (z = %.3f, expected ~0.7)", settled));

	Check(car.CountWheelsOnGround() == TestCar::cWheelCount,
		Fmt("all four wheels report ground contact (got %d)", car.CountWheelsOnGround()));

	//Jitter: a settled car should barely move over another half second.
	const float before = car.GetPos().z;
	Step(manager, 30);
	const float drift = std::fabs(car.GetPos().z - before);
	Check(drift < 0.02f, Fmt("does not jitter once settled (moved %.4f m)", drift));

	const float speed = D3DXVec3Length(&car.GetVelocity());
	Check(speed < 0.5f, Fmt("is at rest with no input (speed = %.3f m/s)", speed));
}

void TestMotorAccelerates(r3d::px::Manager& manager, r3d::px::Scene* scene)
{
	Section("motor torque accelerates, brake torque stops");

	Ground ground(scene);
	TestCar car(scene, D3DXVECTOR3(0.0f, 0.0f, 1.0f));
	Step(manager, 120);

	const float restX = car.GetPos().x;

	car.SetMotorTorque(1350.0f);
	Step(manager, 120);
	car.SetMotorTorque(0.0f);

	const float movedX = car.GetPos().x - restX;
	const float drivenSpeed = D3DXVec3Length(&car.GetVelocity());

	Check(movedX > 1.0f, Fmt("moves forward under motor torque (moved %.3f m)", movedX));
	Check(drivenSpeed > 1.0f, Fmt("is actually moving (speed = %.3f m/s)", drivenSpeed));

	Check(std::fabs(car.GetWheel(0)->GetAxleSpeed()) > 0.1f,
		Fmt("wheels are turning (axle speed = %.3f rad/s)", car.GetWheel(0)->GetAxleSpeed()));

	car.SetBrakeTorque(20000.0f);
	Step(manager, 180);

	const float brakedSpeed = D3DXVec3Length(&car.GetVelocity());
	Check(brakedSpeed < drivenSpeed * 0.5f,
		Fmt("brake torque slows it (%.3f -> %.3f m/s)", drivenSpeed, brakedSpeed));
}

void TestStraightLineHasNoLateralSlip(r3d::px::Manager& manager, r3d::px::Scene* scene)
{
	Section("rolling straight produces no lateral slip");

	Ground ground(scene);
	TestCar car(scene, D3DXVECTOR3(0.0f, 0.0f, 1.0f));
	Step(manager, 120);

	car.SetMotorTorque(1350.0f);
	Step(manager, 120);

	Check(car.MaxAbsLateralSlip() < 0.1f,
		Fmt("no wheel is sliding sideways (worst |lateral slip| = %.4f)", car.MaxAbsLateralSlip()));

	const D3DXVECTOR3 velocity = car.GetVelocity();
	Check(std::fabs(velocity.y) < std::fabs(velocity.x) * 0.2f + 0.2f,
		Fmt("travels along its own axis (vx = %.3f, vy = %.3f)", velocity.x, velocity.y));
}

void TestWheelsInAirReportNoContact(r3d::px::Manager& manager, r3d::px::Scene* scene)
{
	Section("a car in the air reports no wheel contact");

	Ground ground(scene);
	TestCar car(scene, D3DXVECTOR3(0.0f, 0.0f, 20.0f));

	//One step: still falling, nowhere near the ground.
	Step(manager, 1);

	Check(car.CountWheelsOnGround() == 0,
		Fmt("no wheel is in contact while falling (got %d)", car.CountWheelsOnGround()));

	//And it does fall, which proves the body is dynamic and gravity applies.
	const float startZ = car.GetPos().z;
	Step(manager, 30);
	Check(car.GetPos().z < startZ, "falls under gravity");
}

} // namespace

int main()
{
	std::printf("rrr3d physics harness\n=====================");

	r3d::px::Manager::InitSDK();

	{
		r3d::px::Manager manager;

		//Each scenario gets a fresh scene so one car cannot disturb the next.
		{
			r3d::px::Scene* scene = manager.AddScene();
			TestHarnessSimulates(manager, scene);
			manager.DelScene(scene);
		}
		{
			r3d::px::Scene* scene = manager.AddScene();
			TestRestsOnGround(manager, scene);
			manager.DelScene(scene);
		}
		{
			r3d::px::Scene* scene = manager.AddScene();
			TestMotorAccelerates(manager, scene);
			manager.DelScene(scene);
		}
		{
			r3d::px::Scene* scene = manager.AddScene();
			TestStraightLineHasNoLateralSlip(manager, scene);
			manager.DelScene(scene);
		}
		{
			r3d::px::Scene* scene = manager.AddScene();
			TestWheelsInAirReportNoContact(manager, scene);
			manager.DelScene(scene);
		}
	}

	r3d::px::Manager::ReleaseSDK();

	std::printf("\n%d checks, %d failure%s\n",
	            g_checks, g_failures, g_failures == 1 ? "" : "s");
	return g_failures == 0 ? 0 : 1;
}
