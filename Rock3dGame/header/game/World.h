#pragma once

#include "IWorld.h"

#include "ControlManager.h"
#include "View.h"
#include "ResourceManager.h"
#include "CameraManager.h"
#include "Logic.h"
#include "DataBase.h"
#include "Map.h"
#include "Environment.h"
#include "GameMode.h"
#include "snd/Audio.h"
#include "video/VideoPlayer.h"

namespace r3d
{
	namespace game
	{
		class World : public IWorld, public Component, IVideoGraphUser, graph::IVideoResource
		{
			friend class View;
		private:
			using FrameEvents = List<IFrameEvent*>;
			using ProgressEvents = List<IProgressEvent*>;
			using FixedStepEvents = List<IFixedStepEvent*>;
			using LateProgressEvents = List<ILateProgressEvent*>;

			struct FrameDT
			{
			public:
				FrameDT(double mDT, float mRenderTime, float mGpuTime, float mDownTimeIter, float mUpTimeIter,
				        int mCurTimeIter): dt(mDT), renderTime(mRenderTime), gpuTime(mGpuTime),
				                           downTimeIter(mDownTimeIter), upTimeIter(mUpTimeIter),
				                           curTimeITer(mCurTimeIter)
				{
				}

				double dt;
				float renderTime;
				float gpuTime;
				float downTimeIter;
				float upTimeIter;
				int curTimeITer;
			};

			struct IterCost
			{
				float gpuTime;
			};

		public:
			static const float cMaxSimStep;
			static const int cMaxSimIter = 6;
		private:
			int _terminateResult;
			bool _terminate;
			bool _pause;

			std::deque<FrameDT> _dtStack;
			IterCost _iterBuf[cMaxSimIter];
			__int64 _cpuFreq;
			int _syncFreq;
			__int64 _firstTick;
			__int64 _lastTick;
			__int64 _lastTick2;
			__int64 _lastSyncTick;
			double _timeAccum;
			float _lastDrawTime;
			int _curTimeIter;
			int _warmIterations;
			unsigned _time;
			float _dTimeReal;
			bool _inputWasReset;
			bool _videoMode;
			unsigned _timeResolution;

			FrameEvents _frameEvents;
			ProgressEvents _progressEvents;
			FixedStepEvents _fixedStepEvents;
			LateProgressEvents _lateProgressEvents;

			Profiler _profiler;
			ControlManager* _control;
			View* _view;

			GraphManager* _graph;
			snd::Engine* _audio;
			video::Player* _videoPlayer;
			px::Manager* _pxManager;
			px::Scene* _pxScene;
			ResourceManager* _resManager;
			CameraManager* _camera;

			Logic* _logic;
			DataBase* _db;
			Map* _map;
			Environment* _env;

			edit::IEdit* _edit;

			GameMode* _game;

			void UpdateLevel();
			void SetTimeResolution(unsigned period);

			void Progress(float deltaTime);
			void FixedStep(float deltaTime);
			void LateProgress(float deltaTime, bool pxStep);
			void FrameStep(float deltaTime, float pxAlpha);

			bool OnMouseClickEvent(const MouseClick& mClick);
			bool OnMouseMoveEvent(const MouseMove& mMove);
			bool OnKeyEvent(unsigned key, KeyState state, bool repeat);
			void OnKeyChar(unsigned key, KeyState state, bool repeat);

			void OnReset(HWND window, Point resolution, bool fullScreen);
			bool OnPaint(HWND handle) override;
			void OnDisplayChange() override;
			void OnWMGraphEvent() override;
			void OnGraphEvent(HWND hwnd, long eventCode, LONG_PTR param1, LONG_PTR param2) override;
			void OnResetDevice() override;
		public:
			World();
			~World() override;

			//Инициализация. Выненсена в функцию из-за возможных исключений. Применяется подход безопасной инициализаци и освобождения ссылки. Защищает от исключений в самом методе инициализации/освобождения, но не защищает от внутренних исключений в конструкторах ссылок(если там нет умных указателей или там не используется аналогичного подхода с внешнией инициализацией)
			void Init(IView::Desc viewDesc);
			void Free();
			void LoadRes();
			void Terminate(int exitResult = EXIT_SUCCESS);
			bool IsTerminate() const override;
			int GetTerminateResult() const override;

			void MainProgress() override;
			//
			void Pause(bool pause);
			bool IsPaused() const;
			//in ms
			unsigned time() const;
			float dTimeReal() const;
			//
			void ResetInput(bool reset = true) override;
			bool InputWasReset() const override;
			void ResetCamera();

			bool GetVideoMode() const;
			void SetVideoMode(bool value);
			bool IsVideoPlaying() const override;

			void RunGame() override;
			void ExitGame() override;

			void RunWorldEdit() override;
			void ExitWorldEdit() override;

			void SaveLevel(const std::string& level) override;
			void LoadLevel(const std::string& level) override;

			void RegFixedStepEvent(IFixedStepEvent* user);
			void UnregFixedStepEvent(IFixedStepEvent* user);

			void RegProgressEvent(IProgressEvent* user);
			void UnregProgressEvent(IProgressEvent* user);

			void RegLateProgressEvent(ILateProgressEvent* user);
			void UnregLateProgressEvent(ILateProgressEvent* user);

			void RegFrameEvent(IFrameEvent* user);
			void UnregFrameEvent(IFrameEvent* user);

			ControlManager* GetControl();
			IView* GetView() override;
			GraphManager* GetGraph();
			snd::Engine* GetAudio();
			video::Player* GetVideo();
			px::Manager* GetPxManager();
			px::Scene* GetPxScene();
			ResourceManager* GetResManager();
			CameraManager* GetCamera();
			ICameraManager* GetICamera() override;

			Logic* GetLogic();
			DataBase* GetDB();
			Map* GetMap();
			Environment* GetEnv();

			//edit mode
			edit::IEdit* GetEdit() override;
			GameMode* GetGame();
		};
	}
}
