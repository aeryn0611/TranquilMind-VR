# TranquilMind VR — Quest Working Baseline Report

**Date:** 2026-06-21  
**UE Version:** 5.7  
**Platform:** Meta Quest 2 (HorizonOS Android 14 / arm64)  
**Status:** ✅ Confirmed Working on Hardware

---

## 1. Deployed APK

| 항목 | 값 |
|---|---|
| 패키지명 | `com.tranquilmind.vr` |
| 주 APK 경로 | `Binaries/Android/TranquilMind-arm64.apk` |
| 빌드 시각 | 2026-06-21 03:51:05 KST |
| 파일 크기 | 137.6 MB (144,233,745 bytes) |
| 데스크톱 빌드 사본 | `~/Desktop/TranquilMind_AndroidBuild/Android/TranquilMind-arm64.apk` (03:49, 142.9 MB) |
| Gradle debug APK | `Intermediate/Android/arm64/gradle/app/build/outputs/apk/debug/app-debug.apk` |
| 설치 명령 | `adb install -r Binaries/Android/TranquilMind-arm64.apk` |
| 실행 명령 | `adb shell am start -n com.tranquilmind.vr/com.epicgames.unreal.GameActivity` |

---

## 2. 확인된 하드웨어 동작 체인

Quest 런타임 logcat에서 직접 확인된 항목:

```
Hardware : Qualcomm Technologies, Inc KONA (SM8250)
HorizonOS : Android 14, Vulkan 1.1.0
OpenXR   : Oculus Mobile mode (bPackageForMetaQuest=True)
GameMap  : /Game/L_TranquilMind_Void

[CONFIRMED]
✅ App launches → L_TranquilMind_Void 레벨 로드
✅ Environment (M_TheVoid 구체) 렌더링
✅ VRCamera 위치/방향 정상 (CamWorld, Fwd 로그 확인)
✅ SessionManager 자동 시작 → Phase 1A → Phase 1B → Phase II 전환
✅ Target ball 스폰 (VRCamera 200cm 전방)
✅ Target ball 가시성 (Scale=1.0 TEMP로 설정 중)
✅ Right Trigger (OculusTouch_Right_Trigger_Click) 인식
✅ Trigger → HandleTriggerPressed → HandleTriggerPulled → Target 소멸
✅ 다음 Target 스폰 (Trial 연속 진행)
✅ Phase II Core Training 진입 (BaselineRT 계산 완료)
```

로그 예시:
```
[0]   SetupPlayerInputComponent: bound OculusTouch_Right_Trigger_Click -> HandleTriggerPressed
[129] Spawned ONE target | Type=0 | Active=1 | Location=(-404.0, 45.8, 119.9)
[322] DEBUG trigger pulled. Resolving ALL active targets. Count=1
[322] Triggered GO -> Hit | RT=3068.8 ms
[847] Phase II Core Training | BaselineRT=900.0 | ISI=4000 | Noise=0.00
```

---

## 3. 수정된 파일 목록

### 3-A. 영구 변경 (기능 구현)

#### `Source/TranquilMind/Private/TranquilMindVRPawn.cpp`
**수정 시각:** 2026-06-21 03:49  
**변경 내용:**
- Tick() 내 `GetInputAxisKeyValue("OculusTouch_Right_Trigger_Axis")` 폴링 코드 전부 제거
- `SetupPlayerInputComponent()` 추가:
  ```cpp
  PlayerInputComponent->BindKey(
      EKeys::OculusTouch_Right_Trigger_Click,
      IE_Pressed,
      this,
      &ATranquilMindVRPawn::HandleTriggerPressed);
  ```
- Tick() 에 VRCamera 위치 1회 진단 로그 추가 (`bCamLocLogged` 플래그)
- `#include "InputCoreTypes.h"` 추가

**이유:** `GetInputAxisKeyValue`는 `AxisKeyBindings` 배열에 등록된 키만 읽는다. Enhanced Input 모드에서 OpenXR BuildLegacyActions가 생성하는 `OculusTouch_Right_Trigger_Click` boolean 키는 해당 배열에 없기 때문에 항상 0을 반환한다. `BindKey(IE_Pressed)` 방식으로 전환하여 Quest Trigger를 정상 인식.

#### `Source/TranquilMind/Public/TranquilMindVRPawn.h`
**수정 시각:** 2026-06-21 03:49  
**변경 내용:**
- `bRightTriggerWasDown` 멤버 변수 제거 (이전 Tick 폴링 구현의 잔재)

---

### 3-B. 임시 하드웨어 진단 변경 (⚠ 이후 복원 필요)

#### `Source/TranquilMind/Private/Private/Private/TranquilMindTargetActor.cpp`
**수정 시각:** 2026-06-21 03:13  
**⚠ 임시 변경:**

| 위치 | 변경 내용 | 원래 값 | 복원 조건 |
|---|---|---|---|
| Line 151 | `SetActorScale3D(FVector(1.0f))` — 공이 가시적인지 확인용 | `DebugTargetScale (0.25f)` | 공 가시성 확인 완료 후 `DebugTargetScale`로 복원 |
| Lines 102, 123 | TEMP DEBUG MODE — SpawnTarget 후 Timerless 즉시 초기화 | 정식 타이머 로직 | 스케일 복원과 동시에 |
| Line 181 | 로그: `Scale=1.00 (TEMP)` | — | 스케일 복원 시 같이 수정 |

> ⚠ **Note:** 이 파일은 잘못된 경로에 위치함. 아래 "알려진 문제" 참조.

#### `Source/TranquilMind/Private/TargetSpawnerComponent.cpp`
**수정 시각:** 2026-06-21 03:21  
**⚠ 임시 변경:**

| 위치 | 변경 내용 | 복원 조건 |
|---|---|---|
| Lines 419-460 | TEMP: VRCamera 200cm 전방에 스폰 (이전: ISI 기반 정해진 구체 좌표) | 정식 스폰 로직 복원 시 |
| Lines 269-300 | TEMP DEBUG MODE: `HandleTriggerPulled`에서 Tie-break 없이 모든 활성 타겟 일괄 소멸 | 시선 Tie-break 로직 복원 시 |
| Lines 23 | `DebugSpawnX_CM=800, DebugSpawnY_CM=0, DebugSpawnZ_CM=150` 하드코딩 폴백 | 정식 스폰 로직 복원 시 |

---

### 3-C. Config 변경

#### `Config/Android/AndroidEngine.ini`
**수정 시각:** 2026-06-20 07:51  
**내용:**
```ini
[/Script/Engine.RendererSettings]
r.RayTracing=False
r.DynamicGlobalIlluminationMethod=0
r.ReflectionMethod=0
r.Shadow.Virtual.Enable=0
r.Substrate=False
vr.MobileMultiView=1

[/Script/HardwareTargeting.HardwareTargetingSettings]
TargetedHardwareClass=Mobile
DefaultGraphicsPerformance=Scalable

[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]
bPackageForMetaQuest=True
```
**목적:** Quest 2 (모바일 GPU)에 맞는 렌더 프리셋. Ray Tracing/VSM 비활성화, MultiView 활성화.

#### `Config/DefaultGame.ini`
**수정 시각:** 2026-06-20 07:15  
**내용:**
```ini
[/Script/CommonUI.CommonUISettings]
CommonButtonAcceptKeyHandling=TriggerClick

[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=8D8244486D4A6D76DF27B6B8F0DDB127
bStartInVR=True
```
**목적:** VR 자동 시작, CommonUI Trigger 입력 설정.

#### `Config/DefaultEngine.ini`
**수정 시각:** 2026-06-08 06:02 (가장 초기 변경)  
**주요 Quest 관련 설정:**
```ini
GameDefaultMap=/Game/L_TranquilMind_Void.L_TranquilMind_Void
PackageName=com.tranquilmind.vr
bPackageDataInsideApk=True
bBuildForArm64=True
TargetSDKVersion=35
MinSDKVersion=29
```

---

## 4. 파일 경로 이상 (⚠ 미결 문제)

`TranquilMindTargetActor` 소스 파일이 잘못된 중첩 경로에 있다:

| 파일 | 실제 위치 | 올바른 위치 |
|---|---|---|
| TranquilMindTargetActor.cpp | `Source/TranquilMind/Private/Private/Private/TranquilMindTargetActor.cpp` | `Source/TranquilMind/Private/TranquilMindTargetActor.cpp` |
| TranquilMindTargetActor.h | `Source/TranquilMind/Public/Private/Private/TranquilMindTargetActor.h` | `Source/TranquilMind/Public/TranquilMindTargetActor.h` |

UE5 Build Tool이 재귀적으로 .cpp를 탐색하기 때문에 현재 경로에서도 빌드는 성공함. 내부 include 경로도 잘못된 경로에 맞게 조정되어 있음 (`"Private/Private/TranquilMindTargetActor.h"`, `"../../../TranquilMindSessionManager.h"`). 빌드 결과는 정상이지만, 혼란을 줄이기 위해 추후 정위치로 이동 권장.

---

## 5. 알려진 문제 및 진단 완료 항목

### 5-A. 진단 완료 / 기능 영향 없음

| 현상 | 원인 (확인) | 기능 영향 |
|---|---|---|
| `SetupPlayerInputComponent` 2회 호출 | `AutoPossessPlayer = Player0` (배치 Pawn 자동 점유) + GameMode 점유 중첩 | 낮음 — 최종 활성 Pawn만 입력 받음. Trigger 정상 작동 확인 |
| `RegisterSessionManager` 4회 호출 | Level Blueprint(`L_TranquilMind_Void_C`)가 직접 호출 + TargetSpawner.BeginPlay + VRPawn.AutoBindRuntimeReferences | 낮음 — RemoveDynamic + AddUniqueDynamic이 최종 상태 보장 |
| `ApplyStaircaseParams` 2회/Broadcast | Level Blueprint가 `OnStaircaseParamsUpdated`에 별도 구독 + TargetSpawner 자체 구독 | 낮음 — 멱등 함수, 결과 동일 |
| VR 컨트롤러/레이 미표시 | `UXRDeviceVisualizationComponent`, `UWidgetInteractionComponent` 없음 — `UMotionControllerComponent`는 위치 추적만 함 | 무관 — 시선 기반 게임이므로 레이 불필요 |

### 5-B. 미해결 / 조사 필요

| 현상 | 예상 원인 | 우선순위 |
|---|---|---|
| `APP_CMD_LOST_FOCUS` / HardGate at ~14-15초 | 포커스 상실 원인 미상 (XR 런타임 이벤트? 시스템 오버레이?) | 중간 |
| Scale = 1.0 (TEMP) — 미복원 | 공 가시성 확인 후 0.25로 복원 예정이나 아직 미실행 | 높음 (다음 변경) |
| Tie-break 비활성화 (TEMP) | 시선 기반 Tie-break 대신 모든 타겟 일괄 소멸 중 | 높음 (Scale 복원과 동시) |
| 스폰 위치 TEMP 하드코딩 | 200cm 카메라 전방 — 정식 스폰 로직 미복원 | 높음 |
| TargetActor 파일 잘못된 경로 | Private/Private/Private/ 중첩 | 낮음 (기능 무관, 정리 목적) |

---

## 6. 입력 시스템 구성 요약

| 입력 경로 | 상태 | 비고 |
|---|---|---|
| `EKeys::OculusTouch_Right_Trigger_Click` → `BindKey(IE_Pressed)` | ✅ **활성 / 확인됨** | Quest Trigger 인식 |
| `IA_TriggerPressed` + `IMC_VRDefault` (Blueprint Enhanced Input) | ✅ 활성 (SpaceBar 대응) | Blueprint에서 설정됨, VRPawn.cpp 비관여 |
| Level Blueprint SpaceBar 바인딩 | ⚠ 활성 (중복) | Level BP가 별도로 HandleTriggerPulled 직접 호출 |
| `GetInputAxisKeyValue("OculusTouch_Right_Trigger_Axis")` | ❌ 제거됨 | Enhanced Input 모드에서 항상 0 반환하는 잘못된 API |

---

## 7. 권장 다음 변경 순서

1. **Scale 0.25 복원** — `TranquilMindTargetActor.cpp` line 151: `SetActorScale3D(FVector(1.0f))` → `SetActorScale3D(FVector(DebugTargetScale, DebugTargetScale, DebugTargetScale))`
2. **Tie-break 복원** — `TargetSpawnerComponent.cpp` HandleTriggerPulled TEMP DEBUG 블록 제거, 정식 `SelectTargetByTieBreak` 로직 복원
3. **스폰 위치 복원** — `TargetSpawnerComponent.cpp` 200cm 하드코딩 제거, 정식 스폰 로직 복원
4. **`APP_CMD_LOST_FOCUS` 원인 조사** — 14초 경 HardGate 트리거 원인 분석
5. **AutoPossessPlayer 수정** — `SetupPlayerInputComponent` 2회 호출 해소 (승인 후)
6. **Level Blueprint 중복 RegisterSessionManager 정리** (승인 후)
7. **TargetActor 파일 경로 정리** — Private/Private/Private → Private (승인 후)

---

## 8. Git 상태

이 프로젝트는 **git 저장소가 아님** (`fatal: not a git repository`).

### 권장 조치: git 초기화

```bash
cd /Users/aeryn/UnrealProjects/TranquilMind
git init
```

권장 `.gitignore` 제외 항목 (UE5 표준):
```
Binaries/
Intermediate/
Saved/
Build/
DerivedDataCache/
*.VC.db
*.opensdf
*.sdf
*.suo
*.xcworkspace
*.xcuserstate
```

추적할 파일:
- `Source/**` (모든 C++ 소스)
- `Config/**` (모든 .ini)
- `Content/**` (에셋 — 용량 주의, Git LFS 권장)
- `TranquilMind.uproject`

### 현재 상태에 대한 권장 첫 커밋 메시지

```
feat: implement Quest trigger input via BindKey + hardware baseline

- Replace GetInputAxisKeyValue Tick polling with SetupPlayerInputComponent
  BindKey(OculusTouch_Right_Trigger_Click, IE_Pressed) — works on Quest 2
- Configure Android/Quest renderer settings (no RT, Mobile scalable, MultiView)
- TEMP: target scale=1.0, spawn 200cm in front of camera, all-target resolve
- Known: SetupPlayerInputComponent fires 2x (AutoPossessPlayer + GameMode),
  Level Blueprint duplicate RegisterSessionManager, APP_CMD_LOST_FOCUS at ~14s
```

**승인 후 커밋을 진행하겠다고 말씀해 주시면 실행한다.**

---

## 9. 소스 백업

git이 없으므로 타임스탬프 백업 폴더를 생성하고 변경된 파일을 복사했다:

```
Docs/Backup_20260621/
├── Config/
│   ├── Android/AndroidEngine.ini
│   ├── DefaultEngine.ini
│   └── DefaultGame.ini
└── Source/TranquilMind/
    ├── Private/
    │   ├── Private/Private/TranquilMindTargetActor.cpp   (⚠ 잘못된 경로)
    │   ├── TargetSpawnerComponent.cpp
    │   └── TranquilMindVRPawn.cpp
    └── Public/
        ├── Private/Private/TranquilMindTargetActor.h     (⚠ 잘못된 경로)
        └── TranquilMindVRPawn.h
```

이 폴더가 현재 작동하는 Quest 빌드에 대응하는 최소 복원 체크포인트다.

---

*Generated by Claude Code — 2026-06-21*
