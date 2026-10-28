# CTL-13: Chaos 적분과 제어 주기를 맞추는 다음 단계

작성일: 2026-10-27. **설계·엔진 소스 검토이며 아직 구현하거나 실행 검증한 기능이 아니다.**
근거는 로컬에 설치된 Unreal Engine 5.8.3의 소스다. 아래 `Engine/Source/...`와
`Engine/Plugins/...`는 엔진 설치 폴더를 기준으로 한 상대 경로다. 다른 엔진 버전에서는
구현과 줄 번호를 다시 확인해야 한다. 이 문서 작업에서는 게임 소스·물리 설정을 변경하지 않았다.

## 현재 동작과 목표

[SoccerDronePawn.cpp](../Source/ImperialWingsSimulator/SoccerDronePawn.cpp)의 `PhysicsStep`
및 `BeginPlay`/`EndPlay`는 `OnPhysScenePreTick`에 연결된다. 장면이 제공하는 동기 물리
예산으로 센서·PID·모터·배터리·네트 계산을 한 번 실행한다. `PhysicsStep`의 주석도
**물리 프레임당 한 번이며 Chaos 서브스텝마다 실행하는 PID가 아님**을 명시한다.

[DefaultEngine.ini](../Config/DefaultEngine.ini)의 현재 설정은 `bSubstepping=True`,
`bTickPhysicsAsync=False`, 최대 서브스텝 약1/120초, 최대8개다.
[SimulationTime.cpp](../Source/ImperialWingsSimulator/SimulationTime.cpp)는 게임 시간,
장면에 배정된 물리 시간, 버린 시간을 기록하며 보류 시간은0으로 표시한다.
이 계약은 현재 동기 실행에 관한 것이며 비동기로 그대로 복사할 수 없다.

목표는 **한 번의 실제 물리 적분에 대응하는 최신 상태를 읽고, 그 적분에 적용할 힘과
토크를 같은 단계에서 계산하는 것**이다. 이후 별도 조건으로 정확한120Hz 고정 단계를
검증한다. 프레임 안에서 PID 함수만 여러 번 호출하거나 `MaxSubstepDeltaTime`만 바꾼
것을 고정주기 제어 완료로 표시하지 않는다.

## 엔진에서 확인한 실행 경로

| 후보 | UE5.8.3 소스 근거 | 판단 |
|---|---|---|
| `OnPhysScenePreTick` / `OnPhysSceneStep` | `Engine/Source/Runtime/Engine/Private/PhysicsEngine/Experimental/PhysScene_Chaos.cpp:2313`에서 두 delegate를 같은 `UseDeltaTime`으로 연속 broadcast | 둘 다 장면 시작당 한 번. `Step`으로 이름만 바꿔도 해결되지 않는다. |
| `FBodyInstance::AddCustomPhysics` | `Engine/Source/Runtime/Engine/Private/PhysicsEngine/BodyInstance.cpp:3726` → 위 `PhysScene_Chaos.cpp:1665` | Chaos 구현은 `CalculateCustomPhysics.ExecuteIfBound(MDeltaTime, BodyInstance)`를 즉시 호출한다. 옛 PhysX 서브스텝 설명을 적용하면 안 된다. |
| `TSimCallbackObject`의 `OnPreSimulate_Internal` | `Engine/Source/Runtime/Experimental/Chaos/Public/Chaos/SimCallbackObject.h:322`, `Engine/Source/Runtime/Experimental/Chaos/Private/PBDRigidsSolver.cpp:505` | 실제 solver advance의 적분 전 callback. 별도 내부 적분 루프가 있다면 그 루프의 모든 작은 단계마다 호출되는 API는 아니다. |
| `OnPreIntegrate_Internal` / `OnPostIntegrate_Internal` | 위 `SimCallbackObject.h:332`, `:342`; `PBDRigidsSolver.cpp:769`; `Engine/Source/Runtime/Experimental/Chaos/Private/Chaos/PBDRigidsEvolutionGBF.cpp:572` | 실제 `Integrate(Dt)` 앞·뒤에서 호출되는 지원 지점. 계측으로 solver step과 적분 step의 대응을 확인하기 좋다. |
| `AsyncPhysicsTickActor` / `AsyncPhysicsTickComponent` | `PhysScene_Chaos.cpp:146`, `:217`, `:239`, `:2826` | 엔진이 `TSimCallbackObject<..., Presimulate | RunOnFrozenGameThread>`로 구현한다. 등록만으로 시간 간격이 고정되거나 전용 물리 스레드에서 실행된다고 보장되지 않는다. |

`Engine/Source/Runtime/Experimental/Chaos/Private/Chaos/Framework/PhysicsSolverBase.cpp:581`
이 현재 경로를 구분하는 핵심이다.

- 동기 서브스텝에서는 `ceil(frame_dt / max_dt)`개의 solver 단계로 나누고 각 간격을
  `frame_dt / 단계 수`로 정한다(`:607`). 최대 개수를 넘으면 시간 일부를 버린다.
  따라서45FPS의 약22.22ms는 약7.41ms 세 단계가 될 수 있고,240FPS의 약4.17ms는
  한 단계다. 최대1/120초 설정은 **정확히120Hz로 고정한다는 뜻이 아니다.**
- 이 경로는 `PushPhysicsState(InternalDt, NumSteps, ...)` 이후 각 push data에 대해
  callback과 solver advance task를 만든다(`:646`, `:659`, `:714`). 현재5.8의 동기
  서브스텝 경로에서 `OnPreSimulate_Internal`은 이 분할된 solver 단계에 대응할 수 있다.
  헤더의 “not once per sub-step” 설명은 `PBDRigidsSolver.cpp:565`의 별도 내부 적분
  루프와 구분해야 한다. 먼저 실제 callback 횟수를 계측한다.
- 고정 간격 경로는 `IsUsingFixedDt()`일 때 누적 시간을 `AsyncDt`로 나누고 나머지를
  보관한다(`:590`). `IsUsingFixedDt()`는 비동기 결과 사용과 보간 설정을 함께 확인한다
  (`Engine/Source/Runtime/Experimental/Chaos/Public/Chaos/Framework/PhysicsSolverBase.h:712`).
- `PhysScene_Chaos.cpp:549`는 `bTickPhysicsAsync`와 `AsyncFixedTimeStepSize`를 solver
  생성에 전달한다. 실험에서 `EnableAsyncMode(1/120)`를 사용할 수도 있지만, 설정 이름을
  확인하는 데 그치지 않고 `IsUsingFixedDt()`와 callback의 실제 `dt`를 검사해야 한다.
  `p.ForceDisableAsyncPhysics` 또는 `p.UseAsyncInterpolation` 설정도 결과를 바꾼다.

일반 callback의 `GetDeltaTime_Internal()`은
`PhysicsSolverBase.cpp:456`에서 `MLastDt`를 받는다.
`PBDRigidsSolver.cpp:769`의 `OnEvolutionPreIntegrate(Dt)`는 `Dt`를 인수 없는 callback으로
전달하므로, 다른 경로에서 한 solver advance가 여러 내부 적분으로 쪼개지면 callback의
`GetDeltaTime_Internal()`을 각 내부 적분의 간격이라고 가정할 수 없다. 최초 실험은
**한 solver advance = 한 적분**을 검사하고 불일치 시 실패로 종료한다.

## 스레드와 상태 전달 계약

권장 구조는 기체별 UObject를 그대로 호출하는 방식이 아니라,10대의 제어·물리 상태를
소유하는 전용 `TSimCallbackObject<Input, Output>`와 게임 스레드 어댑터다.

1. **게임 스레드 → 입력 버퍼:** 사용자 입력, ARM/DISARM, 입력 소스, 목표 위치,
   기체 식별자와 세대 번호, 파라미터, 중력·경기장 경계의 복사본을
   `GetProducerInputData_External()`로 전달한다. 입력은 시각과 순번을 포함하고,
   기본값은 DISARM이다. callback에서는 조종기·HUD·컨트롤러·UWorld의 임의 상태를
   읽지 않는다. 게임 프레임 사이 입력은 최신 유효 표본을 유지하며, 설정한 유효 기간을
   넘긴 수동 입력이나 세대가 맞지 않는 명령은 추력을0으로 만든다.
2. **물리 callback:** `GetConsumerInput_Internal()`의 불변 입력과
   `FSingleParticlePhysicsProxy::GetPhysicsThreadAPI()`로 얻은 실제 내부 강체 상태를
   읽는다. PID·모터·센서·배터리 상태는 callback만 변경한다. 새 step의 위치·회전·속도를
   사용해 힘과 토크를 계산하며, 컴포넌트가 게임 스레드에 마지막으로 표시한 위치를
   피드백으로 쓰지 않는다.
3. **힘 적용:** 내부 강체 핸들에 힘과 토크를 적용한다. 위치에서의 힘은 실제 질량중심을
   기준으로 `torque = (point - COM) × force`를 계산한다. Unreal의cm 단위 변환은
   어댑터 한 곳에서 처리한다. 기존 `Body->AddForce...` 경로와 새 callback이 동시에
   같은 추진력을 넣지 못하도록 구현 선택을 하나로 고정한다.
4. **출력 버퍼 → 게임 스레드:** `GetProducerOutputData_Internal()`에 완료 시각,
   실제 step 수·간격, 모터 출력, 센서, 배터리 및 상태를 기록하고 게임 스레드는
   `PopOutputData_External()`로 소비한다. 출력은 렌더/HUD/F6용이며 게임 스레드가
   callback 내부 상태를 직접 읽지 않는다. 한 step의 여러 hook은 같은 출력 객체를
   사용하므로 중복 합산하지 않는다.
5. **수명·초기화:** 등록은 `CreateAndRegisterSimCallbackObject_External`, 해제는
   `UnregisterAndFreeSimCallbackObject_External`로 한다. 기체 reset/파괴/월드 종료를
   세대 번호와 명령으로 전달해 오래된 proxy와 명령을 재사용하지 않는다. 종료 중
   producer·consumer 버퍼를 임의로 해제하지 않는다. callback 소멸은 내부 스레드에서
   수행될 수 있으므로 UObject 접근이나 UI 처리를 소멸자에 넣지 않는다.

관련 엔진 근거:

- 입력·출력 큐와 callback 규약:
  `Engine/Source/Runtime/Experimental/Chaos/Public/Chaos/SimCallbackObject.h:56`,
  `:597`, `:605`, `:614`, `:668`.
- 등록·해제 수명:
  `Engine/Source/Runtime/Experimental/Chaos/Public/Chaos/Framework/PhysicsSolverBase.h:412`,
  `:452`.
- 내부/외부 핸들의 스레드 검사:
  `Engine/Source/Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h:1107`.
- 엔진 자체 사용 예:
  `Engine/Plugins/Experimental/ChaosVehiclesPlugin/Source/ChaosVehicles/Private/ChaosVehicleManagerAsyncCallback.cpp:78`
  는 입력 버퍼·내부 핸들·출력 버퍼를 사용한다. 같은 디렉터리의 `DeferredForces.cpp:32`는
  질량중심 기준 위치 힘을 실제 힘과 토크로 나눈다.
- 내부/외부 힘 경로는 별도다:
  `Engine/Source/Runtime/PhysicsCore/Private/ChaosEngineInterface.cpp:1148`, `:1163`, `:1194`.

`RunOnFrozenGameThread`는 이 작업을 건너뛰는 방법이 아니다.
`PhysicsSolverBase.cpp:148`에서 실제 게임 스레드가 실행하더라도 **물리 스레드 context**로
바꾸며, 외부 물리 데이터를 사용하면 검사를 발생시키도록 한다. 현재 `PhysicsStep`의
`Body->GetComponentQuat()`, 컴포넌트 속도, `GetThrottle()`에서의 조종기 조회,
`ApplyNetForces`·`ApplyFlightForces`의 컴포넌트 힘 적용을 그대로 옮기면 잘못된 상태나
잘못된 thread API를 사용할 수 있다.

## 손상을 피하는 단계별 구현 순서

### 1. 힘을 바꾸지 않는 계측 실험

명시적인 검사 옵션에서만 callback을 등록한다. 기존 비행 계산은 유지하고 새 callback은
카운터·시간·식별자와 읽기 전용 강체 상태만 기록한다. `OnPreSimulate`와
`OnPreIntegrate`를 모두 구현해 발생 순서를 기록하되 제어기는 호출하지 않는다.

동기 설정의20/30/45/60/120/240FPS에서 프레임 수, solver 단계 수, 적분 횟수,
`GetDeltaTime_Internal()` 분포와 완료 시각을 비교한다. 최대 간격 설정의 부동소수점
경계 때문에 특정FPS에서 정확히N회라고 먼저 가정하지 않는다. 계측 전후 궤적과 전력
소모가 허용 오차 내에서 동일해야 한다. 이 단계는 **서브스텝 관찰 완료**로만 기록한다.

### 2. 동기 서브스텝에서 단일 힘 경로로 이관

기체 한 대의 호버·자세 복원부터 시작한다. 입력·출력 버퍼와 내부 핸들 어댑터를 만들고
실제 solver 단계마다 센서→제어기→모터/전력→힘을 계산한다. 전환된 기체에서는 기존
`OnPhysScenePreTick`의 같은 계산을 끈다. 한 단계당 제어 업데이트·모터 업데이트·힘 적용
각1회임을 측정한다. 이후10대, 접촉·네트·골대·후류로 넓힌다.

이 단계는 간격이 변하는 **solver 단계 동기화 제어**다.120Hz 고정 완료가 아니다.
후류는 같은 step 시작의10대 상태를 먼저 스냅숏해 순회 순서에 따라 달라지는 결과를
피한다. 가드 파손 표시·소리·머티리얼 갱신은 물리 결과를 받아 게임 스레드에서 처리한다.

### 3. 정확한120Hz는 별도 선택 실험으로 전환

고정 solver 간격1/120초와 지원되는 비동기 결과 경로를 별도 옵션으로 활성화한다.
동작 확인 전에 기본 실행 설정을 변경하지 않는다. 모든 비영(非零) solver 단계의 실제
`dt`와 제어 호출 수가 일치해야 한다. 잔여 시간이1/120초 미만인 프레임에는 새 물리
step이 없을 수 있음을 허용한다. 렌더240FPS가 PID240Hz를 만들면 실패다.

현재 동기 계약의 최대8단계·약66.67ms 예산은
`Engine/Source/Runtime/PhysicsCore/Private/ChaosScene.cpp:331`에서 먼저 적용된다.
비동기를 켜도 이 전처리 예산을 유지할지 별도로 결정해야 한다. 유지한다면100/250ms
정지의 초과 시간은 여전히 버리며, 작은 잔여 시간은 solver 안에 보류될 수 있다.
비동기 `DoNoBlock`에서는 `PhysicsSolverBase.cpp:629`가 대기 task 수에 따라 추가로
step을 버릴 수 있으므로 **모두 따라잡는다고 표시하면 안 된다.**

새 장부는 게임 시간, solver에 제공한 시간, 실제 완료 시간, 대기 작업 시간, 고정 간격
잔여 시간, 생략 시간을 구분해야 한다. `GetAccumulatedTime()`만으로 아직 실행하지
않은 task까지 모두 계상할 수 없다. 공개F6의 기존 `deferred_physics_s=0`을 그대로
남기지 않는다. 공식 경기 타이머는 기존의 게임 시간 계약을 유지한다.

### 4. 통과 조건을 만족한 뒤 기본 경로 판단

아래 조건이 모두 확인되기 전까지 새 경로는 실험 상태다. 기존 동기 경로를 선택할 수
있게 유지하고, 실행 보고서에 실제 선택 경로와 설정을 남긴다.

| 시험 | 최소 판정 기준 |
|---|---|
| 카운터·시계 | 각각60초,20/30/45/60/120/240 렌더FPS. 완료된 physics step의 합과 제어·센서·모터·배터리 시간 합이 일치. 고정 경로는 실제1/120초 검증. |
| 적분 연결 | 별도 단순 강체의 일정 힘 시험에서 `Δv = F/m × 완료 시간`과 일치. 제어 반복 횟수만 늘고 힘이 중복되거나 지연되지 않는지 검사. |
| 지연 | 실제100/250ms 정지 뒤 완료·보류·생략 시간 모두 설명. 재개 순간 오래된 수동 명령·자동ARM이 발생하지 않음. |
| 비행 | 한 대와10대 모두 ANGLE 중립 복원·호버·스로틀 계단 입력·착륙. 기존 통과 기준과 동일한 기준으로 비교하며 새 오차를 기록. |
| 전력 | callback별 공급 전력 한도와 실제 모터 추력, `∫I dt`와 배터리 소모 일치. 전력 제한 수정도 같은 모델 경로를 사용. |
| 접촉 | 바닥·벽·기체끼리·네트·골대 시험. 물리 단계 이관 후 관통·과도한 반발·접촉력 중복 없음. |
| 안전·수명 | DISARM, 입력 stale, 창 포커스 손실, 조종기 메뉴, 기체 전환, reset, 파괴·월드 종료. 완료된 새 세대에 이전 명령/출력이 적용되지 않음. |
| 경기·자율 | 수동 공식 경기 타이머와 ARM 제한 유지. Practice 실제 APF 출발·HOLD·통신 장애·귀환/착륙 회귀 통과. |

실물 조종기의 지연·비행 감각과 다른 물리PC 성능은 별도 현장 시험이다. 소스 검토나
자동 계측을 그 검증으로 대신 표시하지 않는다.

## 프레임 간 비교의 입력·표본 정렬 계약 — 2026-10-28 상세화

위 시험 표에는 갱신 횟수·시간 합·안전 회귀가 있으나, 서로 다른 렌더 FPS의 비행 기록을 **어떤 입력과 같은 시점으로 비교할지**는 별도로 고정해야 한다. 이 절은 CTL-03/10/13·DAT-13/14·QA-12의 시험 설계 보완이며 구현·계측 결과가 아니다. 사용자 흐름과 전체 우선순위는 [상세 기획](first-flight-detailed-plan-2026-10-28.md)을 따른다.

### 동일 입력의 뜻

- 비교용 입력은 “렌더 N번째 프레임에 스로틀 변경”이 아니라, 선택한 시계의 시각·값·순서를 갖는 시나리오로 정의한다. FPS마다 프레임 번호에 맞춰 다른 시간에 입력하면 제어 주기만의 비교가 아니다.
- 기초 제어 비교는 완료된 물리 시간에 대한 입력 표본을 사용하도록 시험 경계를 설계한다. 입력 수집부터 화면 반응까지의 지연 비교는 별도의 단조 벽시계 시험으로 구분한다. 어느 경계에서 시험 입력을 넣었는지 반드시 기록한다.
- 자동 입력은 fixture다. USB 장치가 실제로 해당 주기로 입력했다고 표시하거나 미보정 실제 장치를 활성화하는 우회 수단으로 사용하지 않는다.
- 현재 경로/계측만 추가한 경로/solver 동기화 경로/고정 간격 경로를 같은 초기 자세·속도·배터리·파라미터·난수 조건에서 비교한다. 경로 차이와 PID 재튜닝을 한 번의 원인으로 묶지 않는다.
- 정상 조건에서 입력 재현성을 먼저 확인한 뒤 지연·손실을 추가한다. 100/250ms 정지 시험에서는 실제 정지 시점과 입력 만료 시점을 남기고, 생략한 시간을 따라잡기 위한 과거 입력 재생을 기본 동작으로 가정하지 않는다.

### 로그 정렬과 비교 가능성

| 상황 | 비교 방법·주의 |
|---|---|
| 렌더 프레임 수가 다름 | 배열의 같은 행 번호가 아니라 완료 물리 시각/step 식별자로 맞춘다. 실제 표본과 표시용 보간값을 구분 |
| 물리 완료·게임·벽시계가 다름 | 각 시계의 원점·단위·관계를 기록. 자세/전력과 경기 타이머·USB 타임아웃을 같은 시계로 강제 합산하지 않음 |
| reset·기체 교체·세대 변경 | 구간을 나눠 비교. 초기화 전후 표본 사이를 보간하거나 이전 입력을 새 기체에 적용하지 않음 |
| 누락 표본·긴 정지 | 누락 구간을 표시하고 관측하지 않은 최대 오차/진동을 없다고 간주하지 않음. 시간 장부와 실제 완료 범위 대조 |
| 출력 지연·화면 보간 | 물리 오차와 화면에 늦게 나타난 오차를 별도 측정. 화면 표시만으로 callback의 실제 상태 시각을 추정하지 않음 |
| 충돌 후 궤적이 갈라짐 | 완전한 비트 단위 일치 대신 사전 정의한 사건/물리 지표와 허용치를 비교. 비결정성을 이유로 관통·힘 중복을 무시하지 않음 |
| 시나리오를 끝내지 못함 | 종료 사유·완료 구간·미실행 항목을 남김. 짧게 끝난 기록을 낮은 오차의 성공 사례로 집계하지 않음 |

### 비교 보고와 전환 판단

보고서는 실행별 경로·설정·실제 FPS 분포·완료 시간·입력 표본 적용 시각·제어/solver 호출·단위·누락/생략을 포함한다. 자세/위치 오차·정착·전력·안전 사건은 정의와 산출 구간을 함께 적는다. 평균만으로 순간 폭주를 숨기지 않는다.

기준 경로를 반복 실행해 원래의 변동 폭과 계측 비용을 먼저 확인한다. 오차 허용치·반복 횟수·통계 요약 방식은 이 기준과 제품 요구로 확정하며, 후보 결과를 본 뒤 유리하게 기준을 바꾸면 변경 사유와 전후 판정을 함께 남긴다. 기존 시험의 수치 기준은 그대로 보존하고 변경이 필요하면 별도 검토한다.

입력/시각 정렬 실패·표본 누락으로 결론이 불가능하면 **비교 불가/재계측 필요**로 보고한다. 물리 시간 합이 맞는다는 사실만으로 조종감 개선을 주장하지 않는다. 기본 경로 전환은 위 전체 시험 표와 실물 조종·지원 PC 검수의 필요한 범위를 충족한 뒤 별도로 판단한다. 미확정 허용치나 미실행 안전 시험이 남으면 실험 경로로 유지한다.
