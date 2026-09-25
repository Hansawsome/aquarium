# M8 — 숙련의 게임(부딪혀 잡기·도장·숫자) 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `docs/scenario.md`가 대상 나이 개정으로 새로 정한 **중심축**을 만든다 — **내 물고기로 직접 부딪혀 잡고, 잡은 놈에 내 도장이 박히고, 구석의 숫자가 하나 오른다.** 곁들여 먹이가 **미끼**가 되고, 무리 갈라짐이 **속도에 비례**하며, 타격의 연출(흔들림·물 밀림·난류 자국·「퍽」)이 붙는다.

**Architecture:** 새 모드도, 새 맵도 얹지 않는다. **주된 동사만 바꾼다** — "클릭한다"에서 "몸으로 들이받는다"로.

- **부딪혀 잡기는 화면 좌표에서 판정한다.** 이 프로젝트의 물고기는 **월드 X가 고정된 평면** 위에서만 산다. 내 물고기는 X=220, 배경 물고기는 X≥330이다(`AAquariumGameMode::PlaneOrigin`, `Scripts/build_reef_m1.py`). **즉 두 물고기는 3차원 공간에서 절대로 닿을 수 없다.** 시나리오의 "몸통으로 들이받는다"를 물리 접촉으로 구현하려면 평면 구조를 깨야 하는데, 그것은 M3의 "어떤 화면 비율에서도 영역을 벗어나지 않는다"와 M4c의 평면별 장애물 사전계산을 통째로 무효로 만든다. **그래서 접촉은 카메라에서 본 겹침으로 정의한다** — 카메라가 고정이고 아이의 머릿속 모형이 2D라는 것은 이미 `Food.h`의 깊이 비대칭에서 이 프로젝트가 내린 결정이고, `Flee.h`의 `PickFrontmostHit`가 **클릭에 대해 똑같은 기하**를 이미 쓰고 있다. 잡기는 그 기하의 재사용이지 새 좌표계가 아니다.
- **규칙 순서에 한 칸을 끼운다.** M4c/M5/M7이 고정한 순서에 **회피(Evade)**가 들어간다:
  `입력/Wander → 먹이 → **회피** → 도망 → 무리 → 장애물 → 경계 → StepMotion → Clamp`
  회피는 "어디로 가고 싶은가"의 답을 **대체**하므로 입력/Wander·먹이와 같은 층이다. **도망보다 앞**에 둔다 — 클릭의 결과는 아이가 읽어야 하는 사건이고(M5·M7이 무리를 도망 뒤로 보낸 것과 같은 이유), 이미 도망 중인 놈은 그것만으로 충분히 어렵다. **경계는 끝까지 마지막이다.** 잡기 판정과 도장은 조향이 아니라 **관측**이므로 규칙 순서 바깥이다(틱 끝에서 읽기만 한다).
- **잡기 판정은 액터가 아니라 서브시스템 하나가 한다.** `UCatchSubsystem`이 틱마다 등록된 물고기 목록을 훑어 판정하고, 결과(도장·숫자·소리·흔들림)를 뿌린다. 액터 틱은 늘어나지 않는다.
- **도장의 그림은 이름표다.** `NameTagComponent`/`NameTagWidget`이 절대 위치·스케일 왜곡·매 틱 갱신을 **이미 다 풀어 놨고**, `AttachNameTag`는 어느 물고기에나 붙는다. 사진은 이 계획의 범위 밖이다(시나리오: 비싼 쪽에 재미를 걸지 않는다).
- **연출은 전부 "부풀리지 말고 가속, 삑삑거리지 말고 쿵"이다.** 크기 배율 코드는 이 계획에 단 한 줄도 없다. 흔들림은 카메라 액터의 오프셋, 물 밀림은 카메라 컴포넌트의 포스트 프로세스 블렌더블, 난류 자국은 기존 기포 서브시스템의 두 번째 종류다. **레벨(`ReefM1.umap`)은 한 바이트도 바뀌지 않는다** — 따라서 `verify_scene.py`의 `SCENE_OK actors=69 fish=36 props=22 ...` 기대값도 불변이다.
- **Niagara·MetaSound·SoundCue를 쓰지 않는다.** 에디터 Python으로 만들 수 없다(M4b의 벽). 해석적 머티리얼과 `USoundWave`+C++만 쓴다.

**Tech Stack:** C++17 규칙 계층 + Catch2(`CMakeLists.txt`가 `CMAKE_CXX_STANDARD 17`이다 — 20으로 올리지 않는다), Unreal 5.8.2(C++20 모듈 + 에디터 Python), Python 3(표준 라이브러리만), ffmpeg.

**근거 문서:** [`docs/scenario.md`](../../scenario.md) — 개정판 결정표·연출 문법·금지 사항·검증표가 구속력을 갖는다. 이 계획은 그 결정을 다시 논의하지 않고 태스크로 옮긴다. 형식과 규약은 [`2026-09-21-m7-fun-first.md`](2026-09-21-m7-fun-first.md)를 따른다.

**공통 명령**:
- 규칙 테스트: `cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j && ctest --test-dir build`
- UE 빌드(**반드시 두 번**): `"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex`
- Automation: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "Test Completed|Tests Failed"`
- 에디터 Python: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/<이름>.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "_OK|Traceback"`

---

## 모든 태스크에 걸리는 규약 (전부 이 프로젝트가 실제로 대가를 치른 것)

1. **이 ctest는 `100% tests passed out of N` 형식을 쓴다.** `"0 tests failed out of"`를 grep하면 **아무것도 검증하지 않은 채 통과한다.**
2. **규칙 테스트 mtime 함정** — 같은 초 안에 다시 빌드하면 `make`가 재컴파일을 건너뛰고 ctest가 **낡은 결과**를 보고한다. RED 확인 전에는 반드시 `touch`로 mtime을 올린다(각 RED 스텝에 명령으로 박아 두었다).
3. **UE 모듈은 자동화 전에 두 번 빌드한다.** UBT가 `UnrealEditor.modules`를 한 빌드 늦게 쓴다.
4. **에디터 Python은 `-FullStdOutLogOutput` 없이 돌리면 아무것도 출력하지 않는다** — `FX_OK`도, `Traceback`도. 그런데 에셋은 덮어쓴다. **종료 코드는 무관한 `GameFeatureData` 오류 때문에 항상 1이다. 판정은 `*_OK` grep으로만 한다.**
5. **머티리얼은 조용히 실패하고 회색 기본 머티리얼로 떨어지면서 에디터 뷰포트에서는 멀쩡해 보인다.** **실제 RHI 실행 로그**에서 `Failed to compile Material` 0건을 grep한다. `-nullrhi` 로그는 이 질문에 답하지 못한다. M7 장면 2가 이 확인을 못 하고 넘긴 상태이므로 **M8이 갚는다**(Task 18).
6. **테스트는 아무것도 검사하지 않으면서 통과할 수 있다.** 실제 전례: `PropMaterialsCompile`이 `checked 0 of 13`을 찍으며 Success, `SchoolMatesPullTogether`가 무리를 끈 채 통과, `UpVectorStaysUpright`가 구조상 빨간불 불가, `FleeDoesNotSlide`가 직선 헤엄 중 초록불, M5 캡처 하네스가 **계획한 클릭 7개 전부 빈 물**인데 경고 0건, M6 스캔이 UTF-16을 못 봐 대조군 0/11, M7의 테스트 헬퍼가 `bPlayerControlled`를 켜 둔 채라 도망 테스트 3개가 **도망하지 않는 물고기**를 볼 뻔했다. **이 계획의 모든 신규 테스트는 변이로 빨간불을 먼저 확인한다. 제안한 변이로 빨간불이 켜지지 않으면 그것은 통과의 증거가 아니다 — 빨간불이 켜지는 변이를 찾거나, 찾지 못했다는 사실을 후속 항목에 적는다.**
7. **변이는 "정말로 동작이 바뀌는 것"이어야 한다.** M7 계획은 `UButton`에 `UTextBlock`을 **추가**하는 변이를 적었는데, `UButton`은 단일 자식 위젯이라 **아무 일도 일어나지 않았다**(테스트는 초록불인 채였다). 추가가 아니라 **대체·삭제·역전**으로 쓴다.
8. **같은 규칙을 두 군데 두면 검증이 결함을 영원히 못 잡는다(파생하고 베끼지 않는다).** 잡기 속도 문턱은 플레이어 `MaxSpeed`에서, 화면 반지름은 `VisibleHalfExtents`에서, 도장 완주 수는 등록된 물고기 수에서, 기포 소멸 높이는 그 기포가 난 깊이에서 **파생**한다. 숫자를 리터럴로 베끼지 않는다.
9. **계획은 모든 마일스톤에서 틀렸다.** M7 계획만 해도 통과 불가능한 `PlayerReaction` 기대값, 존재하지 않는 `unreal.SoundFactory.b_auto_create_cue`(진짜 이름 `auto_create_cue`)와 `UTexture2D.imported_size`(진짜 `blueprint_get_size_x/y`), 자기 테스트를 못 통과하는 `VoiceLimiter` 감쇠식, **평면 하나에서 파생해 화면 절반 높이에서 사라질 뻔한** 기포 소멸 높이가 있었다. **전제는 믿지 말고 확인한다.** 태스크마다 "전제 확인" 스텝이 있고, 어긋나면 진행하지 말고 보고한다. **테스트 개수는 계획의 산수일 뿐이다** — 매번 ctest/Automation이 실제로 찍은 숫자를 근거로 삼고, 누적이 어긋나면 멈추고 보고한다.
10. **난이도는 테스트가 판정할 수 없다.** 잡기가 쉬우면 이 마일스톤의 전제가 통째로 무너지는데 단위 테스트는 전부 초록불일 것이다. Task 16의 측정 프로토콜이 이 계획의 **진짜 합격 판정**이다.
11. **`scripts/check_ambience.sh`는 지금 정당하게 빨간불이다.** CC0 앰비언스 파일이 아직 없기 때문이다. **약화하거나 우회하지 않는다.** M7 Task 6은 사용자가 파일을 줄 때까지 보류이며 M8은 그것을 건드리지 않는다.
12. **이미 내린 사양 변경 두 개는 다시 논의하지 않는다.** **F-11 폐기**(도망 중 재터치 무시 — 연타가 이 나이대의 기본 사용법이다)와 **F-12 실질 폐기**(내 물고기는 도망하지 않으므로 도달 불가능; 테스트는 지우지 않고 뒤집혀 있다). SRS 문구 개정은 **사용자 결정 사항**이라 이 계획은 `docs/TASK.md`에 기록만 한다.

---

### Task 0: 전제 확인과 기준 보관

**Files:** 없음 (읽기 전용)

- [ ] **Step 1: 브랜치·작업 트리·HEAD 확인**

```bash
cd /Users/hans/dev/aquarium && git status --short && git rev-parse --abbrev-ref HEAD && git rev-parse --short HEAD
```
기대: 출력 없는 첫 줄, `feat/m7-fun-first`, `646a2e3`. **HEAD가 다르면 멈추고 보고한다**(계획서가 지어낸 HEAD로 진행한 전례가 있다). 이 계획은 같은 브랜치에서 이어서 작업한다.

- [ ] **Step 2: 규칙 테스트 기준선**

```bash
cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 126`. 숫자가 다르면 **그 숫자를 기준선으로 채택하고** 이후 태스크의 기대 숫자를 전부 그만큼 옮긴다(규약 9).

- [ ] **Step 3: Automation 기준선 (빌드 두 번)**

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `Result: Succeeded` 두 번, 마지막 줄 `72`.

- [ ] **Step 4: 이 계획의 핵심 전제 — 두 물고기는 서로 다른 평면에 있다**

```bash
cd /Users/hans/dev/aquarium && grep -n "PlaneOrigin = FVector(220" unreal/Aquarium/Source/Aquarium/AquariumGameMode.h && grep -n "PLANE_X\|plane_x\|X = 3[0-9][0-9]" unreal/Aquarium/Scripts/build_reef_m1.py | head -5
```
기대: 플레이어 평면 X=220이 확인되고, 배경 물고기 평면 X가 그보다 **크다**. 만약 두 값이 겹친다면 물리 접촉이 가능하다는 뜻이므로 **멈추고 보고한다** — 이 계획의 화면 좌표 판정 근거가 바뀐다.

- [ ] **Step 5: 재사용할 API가 실재하는지 확인**

```bash
cd /Users/hans/dev/aquarium && grep -n "AttachNameTag\|AsClickTarget\|ScreenTopZAt\|VisibleHalfExtents" unreal/Aquarium/Source/Aquarium/*.h && grep -n "PickFrontmostHit" rules/include/aquarium/Flee.h unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.h
```
기대: 여섯 이름이 전부 한 번 이상 나온다. 하나라도 없으면 그 태스크의 설계가 틀린 것이므로 보고한다.

- [ ] **Step 6: 앰비언스 게이트가 여전히 빨간불인지 확인(약화 금지의 근거 보관)**

```bash
cd /Users/hans/dev/aquarium && bash scripts/check_ambience.sh; echo "exit=$?"
```
기대: **실패한다**(`exit=1` 계열). **성공하면** 누군가 파일을 넣었거나 게이트가 약해진 것이므로 보고한다. 이 계획은 이 스크립트를 수정하지 않는다.

- [ ] **Step 7: 커밋 없음.** 읽기 전용 태스크다.

---

### Task 1: 반응음 재판정 — 「꺅」을 버리고 「퍽」·「쿵」으로

**Files:** Modify `assets/audio/make_sounds.py`, Modify `unreal/Aquarium/Scripts/import_audio.py`, Modify `unreal/Aquarium/Source/Aquarium/AquariumAudioSubsystem.h/.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/AudioTests.cpp`, Modify `docs/ASSETS.md`

현행 `startle()`는 **700Hz에서 2200Hz로 올라가는 글라이드**다. 개정 시나리오는 그것을 정확히 이름 붙여 금지한다 — "만화 비명 톤", "삑삑거리지 말고 쿵". 그리고 잡기에는 **새 소리 하나**가 필요하다: 몸이 부딪히는 「퍽」.

- [ ] **Step 1: 지금 무엇이 있는지 확인**

```bash
cd /Users/hans/dev/aquarium && python3 assets/audio/make_sounds.py | tail -2
```
기대: `SOUNDS_OK count=5 rate=44100 [...]`. 이 줄의 sha256 다섯 개를 **적어 둔다**(뒤에서 어떤 파일이 실제로 바뀌었는지 대조한다).

- [ ] **Step 2: `startle`을 낮고 짧게 다시 합성하고 `thud`를 더한다**

`assets/audio/make_sounds.py`에서 `startle` 함수 전체를 아래로 **대체**하고, `thud`를 `nibble` 앞에 **추가**한다.

```python
def startle(rate=RATE):
    """「퍽」 0.13초. **개정 전에는 700→2200Hz로 올라가는 「꺅」이었다.** 대상 나이가
    초등 5~6학년으로 바뀌면서 그 톤이 시나리오의 금지 목록(만화 비명)에 올랐다.
    이제 방향이 반대다: 240Hz에서 90Hz로 **떨어지는** 짧은 몸통 소리 + 물이 밀리는
    잡음 한 겹. 올라가면 '꺅', 내려가면 '퍽'이다 — 이 한 줄이 유치함의 분기점이다."""
    rnd = random.Random(7)
    n = int(0.130 * rate)
    out = [0.0] * n
    phase = 0.0
    lp = 0.0
    for i in range(n):
        t = i / n
        f = 240.0 * math.exp(-5.0 * t) + 90.0
        phase += 2.0 * math.pi * f / rate
        env = math.exp(-13.0 * t)
        body = math.sin(phase) + 0.22 * math.sin(2.0 * phase)
        # 물이 밀리는 저역 잡음. 1극 저역통과라 「치익」이 아니라 「퍽」의 몸통이 된다.
        lp += 0.18 * (rnd.uniform(-1.0, 1.0) - lp)
        out[i] = env * (body * 0.85 + lp * 0.55)
    return fade(out, in_s=0.001, out_s=0.008, rate=rate)


def thud(rate=RATE):
    """「쿵」 0.18초. 잡았을 때만 난다. startle보다 **한 옥타브 아래에서 시작하고
    더 길다** — 아이가 '비켰다'와 '잡았다'를 소리만으로 구분해야 하기 때문이다.
    타격감의 나머지 9할은 화면 흔들림이 만든다(Task 12)."""
    rnd = random.Random(911)
    n = int(0.180 * rate)
    out = [0.0] * n
    phase = 0.0
    lp = 0.0
    for i in range(n):
        t = i / n
        f = 120.0 * math.exp(-4.0 * t) + 52.0
        phase += 2.0 * math.pi * f / rate
        env = math.exp(-9.0 * t)
        lp += 0.10 * (rnd.uniform(-1.0, 1.0) - lp)
        # 첫 6ms의 딱딱한 어택이 '맞았다'를 만든다. 그 뒤는 몸통뿐이다.
        click = math.exp(-260.0 * t) * rnd.uniform(-1.0, 1.0) * 0.7
        out[i] = env * (math.sin(phase) * 1.0 + lp * 0.8) + click
    return fade(out, in_s=0.0005, out_s=0.012, rate=rate)
```

`SOUNDS` 튜플에 한 줄을 더한다(순서는 `EAquariumCue`와 무관하다 — 파일 이름으로 찾는다).

```python
SOUNDS = (
    ("S_Swim.wav", swim),
    ("S_Startle.wav", startle),
    ("S_Thud.wav", thud),
    ("S_Nibble.wav", nibble),
    ("S_Split.wav", split),
    ("S_Bubble.wav", bubble),
)
```

- [ ] **Step 3: 합성하고 "정말로 낮아졌는지"를 **측정**한다**

소리를 헤드리스로 들을 수는 없지만 **스펙트럼 무게중심은 잴 수 있다.** 「꺅」이 「퍽」이 되었다는 주장은 여기서만 증명된다.

```bash
cd /Users/hans/dev/aquarium && python3 assets/audio/make_sounds.py | tail -2 && python3 - << 'EOF'
import wave, array, math, cmath, os
def centroid(path):
    with wave.open(path) as w:
        n = w.getnframes(); rate = w.getframerate()
        d = array.array("h"); d.frombytes(w.readframes(n))
    # 앞 4096 샘플의 DFT 무게중심(Hz). 느리지만 의존성이 없다.
    N = 4096
    xs = [d[i]/32768.0 for i in range(min(N, len(d)))]
    xs += [0.0]*(N-len(xs))
    num = den = 0.0
    for k in range(1, N//2):
        s = sum(xs[t]*cmath.exp(-2j*math.pi*k*t/N) for t in range(0, N, 8))
        m = abs(s)
        f = k*rate/N
        num += m*f; den += m
    return num/den if den else 0.0
for f in ("S_Startle.wav", "S_Thud.wav"):
    p = os.path.join("assets/audio", f)
    print("%-14s centroid=%7.1f Hz" % (f, centroid(p)))
EOF
```
기대: `SOUNDS_OK count=6`, 그리고 **두 소리의 무게중심이 모두 900 Hz 아래**이며 `S_Thud`가 `S_Startle`보다 **더 낮다**. 개정 전 「꺅」은 이 측정에서 1.5 kHz를 훌쩍 넘겼다 — 넘는 값이 나오면 합성이 계획대로 바뀌지 않은 것이다. **실제 수치를 기록해 둔다**(Task 20의 인계 항목).

- [ ] **Step 4: 임포트 스크립트에 새 파일을 더한다**

`unreal/Aquarium/Scripts/import_audio.py`에서 임포트 목록에 `S_Thud.wav`를 더한다. 목록이 어떤 형태인지 먼저 읽는다:

```bash
cd /Users/hans/dev/aquarium && grep -n "S_Startle\|WAVES\|SOUNDS\|for name" unreal/Aquarium/Scripts/import_audio.py
```
기대: 파일 이름 목록이 한 곳에 있다. **그 목록에 `S_Thud`를 같은 형식으로 한 줄 더한다**(자료구조가 계획의 추측과 다르면 실제 형식을 따른다 — 규약 9). 목록 길이를 세어 `AUDIO_OK count=6`이 되도록 마지막 출력 줄의 기대 개수도 함께 고친다.

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/import_audio.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "AUDIO_OK|Traceback"
```
기대: `AUDIO_OK count=6 ...`. `Traceback`이 한 줄이라도 있으면 멈춘다(규약 4 — 종료 코드는 판정에 쓰지 않는다).

- [ ] **Step 5 (RED): `EAquariumCue::Thud`를 요구하는 테스트**

`unreal/Aquarium/Source/Aquarium/Tests/AudioTests.cpp` 끝에 더한다.

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioThudCueExists, "Aquarium.Audio.ThudCueExists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAudioThudCueExists::RunTest(const FString&)
{
	// 잡았을 때의 「쿵」은 놀람의 「퍽」과 **다른 에셋**이어야 한다. 같은 파일을
	// 두 번 꽂아 두면 아이는 '비켰다'와 '잡았다'를 소리로 구분하지 못한다.
	FAquariumTestWorld World;   // AudioTests.cpp가 이미 쓰는 헬퍼
	UAquariumAudioSubsystem* Audio = World.Get()->GetSubsystem<UAquariumAudioSubsystem>();
	TestNotNull(TEXT("audio subsystem"), Audio);
	USoundBase* Thud = Audio->SoundFor(EAquariumCue::Thud);
	USoundBase* Startle = Audio->SoundFor(EAquariumCue::Startle);
	TestNotNull(TEXT("thud wave loaded"), Thud);
	TestNotNull(TEXT("startle wave loaded"), Startle);
	TestTrue(TEXT("thud is a different asset from startle"), Thud != Startle);
	// Count가 개수의 유일한 출처다. 큐를 더했는데 이 값이 안 늘면 어딘가를 빼먹은 것이다.
	TestEqual(TEXT("cue count"), static_cast<int32>(EAquariumCue::Count), 5);
	return true;
}
```

> **전제 확인:** `AudioTests.cpp`가 실제로 쓰는 테스트 월드 헬퍼 이름을 먼저 읽고 맞춘다(`grep -n "struct F.*TestWorld\|FAquariumTestWorld" unreal/Aquarium/Source/Aquarium/Tests/*.cpp`). 이름이 다르면 **그 이름을 쓴다.**

빌드하면 `EAquariumCue::Thud`가 없어 **컴파일 에러**다. RED다.

- [ ] **Step 6 (GREEN): 큐를 더한다**

`AquariumAudioSubsystem.h`의 enum을 고친다(주석도 함께 — 「꺅」이라는 말이 남아 있으면 안 된다):

```cpp
enum class EAquariumCue : uint8
{
	Startle,   // 「퍽」 배경 물고기가 놀람 (개정 전 「꺅」. 만화 비명은 유치함 신호다)
	Thud,      // 「쿵」 몸으로 들이받아 **잡았다**
	Nibble,    // 「뽁」 먹이를 먹음
	Split,     // 「촤악」 무리가 갈라짐
	Bubble,    // 「뽀글」 내 물고기 재롱
	Count UMETA(Hidden)
};
```

`AquariumAudioSubsystem.cpp`의 경로 표에 같은 자리에 한 줄을 넣는다:

```cpp
static const TCHAR* const kCuePaths[] = {
	TEXT("/Game/Audio/S_Startle.S_Startle"),
	TEXT("/Game/Audio/S_Thud.S_Thud"),
	TEXT("/Game/Audio/S_Nibble.S_Nibble"),
	TEXT("/Game/Audio/S_Split.S_Split"),
	TEXT("/Game/Audio/S_Bubble.S_Bubble"),
};
```

> **전제 확인:** 실제 배열 이름과 경로 형식을 `grep -n "S_Startle" unreal/Aquarium/Source/Aquarium/AquariumAudioSubsystem.cpp`로 먼저 읽고 **그 형식에 맞춘다.** 배열과 enum의 길이가 어긋나면 컴파일 타임에 잡히도록 `static_assert(UE_ARRAY_COUNT(kCuePaths) == static_cast<int32>(EAquariumCue::Count), "cue table out of sync");`를 함께 넣는다.

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `73`.

- [ ] **Step 7: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `kCuePaths`의 Thud 경로를 `S_Startle`로 **대체**한다 | `Aquarium.Audio.ThudCueExists` (같은 에셋) |
| B | `make_sounds.py`의 `thud` 본문을 `return startle(rate)`로 **대체**하고 다시 합성·임포트 | Step 3의 무게중심 측정에서 두 값이 **같아진다**(A와 같은 결함을 에셋 쪽에서 낸 것) |
| C | `startle`의 주파수 식을 개정 전 식(`700 + 1500*t**0.7`)으로 **되돌린다** | Step 3의 무게중심이 900 Hz를 넘는다 |

변이 C가 이 태스크의 존재 이유를 지키는 시험이다. 전부 되돌린다.

- [ ] **Step 8: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add assets/audio unreal/Aquarium/Scripts/import_audio.py unreal/Aquarium/Source/Aquarium docs/ASSETS.md && git commit -q -F - << 'EOF'
feat(m8): 「꺅」을 버리고 「퍽」·「쿵」으로 — 만화 비명 톤 재판정

대상 나이 개정으로 만화 비명이 금지 목록에 올랐다. startle을 700→2200Hz
상승 글라이드에서 240→90Hz 하강 + 저역 잡음으로 다시 합성하고, 잡았을 때만
나는 S_Thud를 더했다. 스펙트럼 무게중심을 직접 재서 두 소리 모두 900Hz
아래임을 확인했고, 옛 식으로 되돌리는 변이로 그 측정이 빨간불이 되는 것을
확인했다.

Automation 72 → 73.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 2: 규칙 계층 — 잡기의 기하 (`Catch.h`), 규칙 126 → 141

**Files:** Create `rules/include/aquarium/Catch.h`, Create `tests/test_catch.cpp`, Modify `CMakeLists.txt`

**이 헤더가 이 마일스톤의 심장이다.** 무엇이 잡힌 것인지 여기서 한 번만 정한다.

**결정 1 — 접촉은 화면에서 본 겹침이다.** 평면 구조상 물리 접촉이 불가능하다(Task 0 Step 4). 카메라에서 본 좌표 `(pos - cam) / depth`로 투영해 **내 물고기의 코끝이 상대 타원 안에 들어오면 접촉**이다. `Flee.h::PickFrontmostHit`가 클릭에 대해 쓰는 것과 **같은 기하**이고, 같은 타원이다 — 클릭으로 맞힐 수 있는 놈은 몸으로도 맞힐 수 있다는 뜻이라 아이가 배울 규칙이 하나뿐이다.

**결정 2 — 잡기의 조건은 "접촉 + 접근 속도"뿐이다. 각도도, 접촉 지속 시간도 없다.**
- 각도 조건은 **아이 눈에 보이지 않는다.** "분명히 부딪혔는데 안 잡혔다"가 반복되면 그것은 어려움이 아니라 고장이다.
- 지속 시간 조건은 즉시성(장면 2 요구사항 1)과 정면으로 부딪히고, 관성이 있는 조작에서 "붙어 있기"는 재미가 아니라 성가심이다.
- 남는 것이 **속도**다. 시나리오가 "먹히는 것" 첫 줄에 적은 바로 그것이고, 두 번 해 보면 배우며, 돌진(Task 4)이 정확히 이 값을 올린다. **잘하는 아이와 못하는 아이가 갈리는 자리가 여기 한 줄이다.**

**결정 3 — 접근 속도는 상대 속도다.** `내 속도 - 상대 속도를 내 진행 방향에 투영한 값`. 접촉 순간에는 둘 사이 거리가 0이라 "서로를 향한 방향"이 정의되지 않으므로 **내 진행 방향**을 기준으로 삼는다. 이 정의의 결과가 정확히 시나리오의 그림이다: **달아나는 놈을 같은 속도로 따라가면 안 잡힌다. 더 빨라야 잡힌다.**

**결정 4 — 빗나감에는 벌이 없다.** 결과는 셋뿐이다: `Miss`(아무 일 없음), `Bump`(겹쳤지만 느렸다 → 상대가 놀라 튄다), `Catch`. `Bump`도 아이에게는 사건이다("놈이 튀었다"), 그리고 그것이 다시 붙을 이유가 된다.

- [ ] **Step 1 (RED): `tests/test_catch.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

#include "aquarium/Catch.h"

using namespace aquarium;
using Catch::Approx;

namespace {
const CatchParams kC{};

// 배경 물고기 한 마리. 기본값은 실제 장면에 가깝게: 깊이 400, 길이 25cm급.
RamTarget Fish(float depth, Vec2 centre, Vec2 vel = {0.f, 0.f}) {
    RamTarget t;
    t.depth = depth;
    t.center = centre;
    t.velocity = vel;
    t.halfWidth = 12.5f;
    t.halfHeight = 4.f;
    return t;
}

// 카메라는 원점, 평면들은 +X 앞에 있다(실제 배치와 같은 부호).
const Vec3 kCam{0.f, 0.f, 0.f};
}

TEST_CASE("a fast nose on the target catches it") {
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};              // 화면에서 정확히 겹친다
    me.velocity = {90.f, 0.f};         // 최대 속도로 돌진
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Catch);
    REQUIRE(r.targetIndex == 0);
    REQUIRE(r.closingSpeed > 0.f);
}

TEST_CASE("drifting into a fish does NOT catch it -- it only bumps") {
    // 관성으로 슬금슬금 겹쳐지는 것은 잡기가 아니다. 이 한 줄이 '어려움'의 전부다.
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {6.f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Bump);
    REQUIRE(r.targetIndex == 0);
}

TEST_CASE("chasing a fish at the same speed never catches it") {
    // 시나리오의 그림 그대로: 달아나는 놈을 같은 속도로 따라가면 못 잡는다.
    // 깊이가 달라 화면 속도가 다르므로, 두 속도는 **화면에서** 같아지도록 만든다.
    const float mineDepth = 220.f, itsDepth = 400.f;
    const float myScreenSpeed = 90.f / mineDepth;
    std::vector<RamTarget> targets{Fish(itsDepth, {0.f, 0.f}, {myScreenSpeed * itsDepth, 0.f})};
    Rammer me;
    me.depth = mineDepth;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {90.f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Bump);
    REQUIRE(r.closingSpeed == Approx(0.f).margin(1e-3f));
}

TEST_CASE("catching a fleeing fish needs to be FASTER than it") {
    const float mineDepth = 220.f, itsDepth = 400.f;
    const float itsScreenSpeed = 40.f / itsDepth;
    std::vector<RamTarget> targets{Fish(itsDepth, {0.f, 0.f}, {40.f, 0.f})};
    Rammer me;
    me.depth = mineDepth;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    // 화면 기준으로 상대보다 충분히 빠르게: 문턱 + 상대의 화면 속도.
    const float need = CatchThreshold(90.f, mineDepth, kC) + itsScreenSpeed;
    me.velocity = {need * mineDepth * 1.05f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Catch);
}

TEST_CASE("no overlap is a plain miss, whatever the speed") {
    std::vector<RamTarget> targets{Fish(400.f, {900.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {90.f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Miss);
    REQUIRE(r.targetIndex == -1);
}

TEST_CASE("depth is normalized: two fish that look identical behave identically") {
    // 같은 화면 위치·같은 화면 크기인데 깊이만 다른 두 마리. 하나가 더 잡기 쉬우면
    // 아이는 이유를 알 수 없다 -- 화면에서는 완전히 같아 보이기 때문이다.
    RamTarget near_ = Fish(400.f, {40.f, 0.f});
    RamTarget far_ = near_;
    far_.depth = 800.f;
    far_.center = {80.f, 0.f};          // (pos/depth)가 같도록
    far_.halfWidth = near_.halfWidth * 2.f;
    far_.halfHeight = near_.halfHeight * 2.f;
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {22.f, 0.f};
    me.velocity = {90.f, 0.f};
    std::vector<RamTarget> a{near_};
    std::vector<RamTarget> b{far_};
    const RamResult ra = EvaluateRam(kCam, me, a.data(), a.size(), kC);
    const RamResult rb = EvaluateRam(kCam, me, b.data(), b.size(), kC);
    REQUIRE(ra.outcome == rb.outcome);
    REQUIRE(ra.closingSpeed == Approx(rb.closingSpeed));
}

TEST_CASE("the frontmost overlapping fish is the one that gets hit") {
    // 두 마리가 화면에서 겹쳐 있으면 앞엣놈이다. 클릭(PickFrontmostHit)과 같은 규칙이라
    // 아이가 배울 규칙이 하나뿐이다.
    std::vector<RamTarget> targets{Fish(700.f, {0.f, 0.f}), Fish(380.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {90.f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.targetIndex == 1);
}

TEST_CASE("an already stamped fish can still be bumped but reports as stamped") {
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    targets[0].alreadyStamped = true;
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {90.f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    // 잡히는 것 자체는 막지 않는다. 숫자를 두 번 올리지 않는 일은 StampBook이 한다.
    REQUIRE(r.outcome == RamOutcome::Catch);
    REQUIRE(r.targetIndex == 0);
}

TEST_CASE("the nose sits ahead of the body, not at its centre") {
    // 머리로 받아야 '들이받았다'이다. 꼬리로 스친 것이 잡기가 되면 아이는 무엇을 한 건지 모른다.
    const Vec2 heading{1.f, 0.f};
    const Vec2 nose = NosePoint({0.f, 0.f}, heading, /*halfLength*/ 17.f, kC);
    REQUIRE(nose.x > 0.f);
    REQUIRE(nose.x <= 17.f + 1e-3f);
    // 방향이 0이면(정지) 몸 중심 그대로. 0으로 나누지 않는다.
    const Vec2 still = NosePoint({5.f, 6.f}, {0.f, 0.f}, 17.f, kC);
    REQUIRE(still.x == Approx(5.f));
    REQUIRE(still.y == Approx(6.f));
}

TEST_CASE("the catch threshold is DERIVED from the player's own max speed") {
    // 문턱을 리터럴로 적으면 최대 속도를 조정할 때 난이도가 조용히 어긋난다(규약 8).
    const float t1 = CatchThreshold(90.f, 220.f, kC);
    const float t2 = CatchThreshold(180.f, 220.f, kC);
    REQUIRE(t2 == Approx(t1 * 2.f));
    REQUIRE(t1 > 0.f);
}

TEST_CASE("nothing to ram is a miss, not a crash") {
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.velocity = {90.f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, nullptr, 0, kC);
    REQUIRE(r.outcome == RamOutcome::Miss);
    REQUIRE(r.targetIndex == -1);
}

TEST_CASE("a target behind the camera is never hit") {
    std::vector<RamTarget> targets{Fish(-100.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.velocity = {90.f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Miss);
}

TEST_CASE("a zero-size target is skipped") {
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    targets[0].halfWidth = 0.f;
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.velocity = {90.f, 0.f};
    REQUIRE(EvaluateRam(kCam, me, targets.data(), targets.size(), kC).outcome == RamOutcome::Miss);
}

TEST_CASE("the grace factor makes the ram no harder than the click") {
    // 클릭으로 맞힐 수 있는 놈은 몸으로도 맞힐 수 있어야 한다. graceScale < 1 이면
    // 아이는 '분명히 닿았는데 안 됐다'를 겪는다.
    REQUIRE(kC.graceScale >= 1.f);
}

TEST_CASE("there is no angle gate and no dwell timer in this header") {
    // 구조로 보장한다: EvaluateRam은 '지금 이 순간'만 받는다. 시간 인자도,
    // 각도 인자도 없으므로 그런 조건을 넣는 코드를 쓰는 것 자체가 불가능하다.
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    // 옆에서 들이받는다(진행 방향이 상대의 장축과 수직). 그래도 잡힌다.
    me.velocity = {0.f, 90.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Catch);
}
```

`CMakeLists.txt`의 `rules_tests` 목록에 `tests/test_catch.cpp`를 더한다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_catch.cpp && cmake -S . -B build && cmake --build build -j 2>&1 | tail -4
```
기대: `'aquarium/Catch.h' file not found`. RED다.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Catch.h`**

```cpp
#pragma once
#include <cstddef>

#include "aquarium/SwimPlane.h"   // Vec2, Vec3
#include "aquarium/Vec2.h"

namespace aquarium {

// 이 게임에서 "부딪혔다"가 무엇인지 정하는 유일한 곳.
//
// **왜 화면 좌표인가.** 물고기는 월드 X가 고정된 평면 위에서만 산다. 내 물고기는
// X=220, 배경 물고기는 X>=330이라 3차원에서는 **절대로 닿을 수 없다.** 평면 구조를
// 깨면 M3의 경계 보장과 M4c의 평면별 장애물 사전계산이 함께 무너진다. 그래서 접촉은
// 카메라에서 본 겹침으로 정의한다 -- 카메라가 고정이고 아이의 머릿속 모형이 2D라는
// 것은 Food.h의 깊이 비대칭에서 이미 내린 결정이고, Flee.h의 PickFrontmostHit가
// 클릭에 대해 **같은 기하**를 이미 쓰고 있다.

// 들이받히는 쪽. ClickTarget과 필드가 겹치는 것은 우연이 아니다 -- 클릭으로 맞힐 수
// 있는 놈은 몸으로도 맞힐 수 있어야 하기 때문이고, 엔진은 둘 다 같은 렌더 바운드에서
// 파생해 채운다.
struct RamTarget {
    float depth = 0.f;        // 월드 X
    Vec2 center;              // 공유 유영 프레임(x = 월드 Y, y = 월드 Z)
    Vec2 velocity;            // 같은 프레임, cm/s
    float halfWidth = 0.f;
    float halfHeight = 0.f;
    bool alreadyStamped = false;   // 관측용. 판정을 바꾸지 않는다(벌을 만들지 않는다)
};

// 들이받는 쪽(= 내 물고기).
struct Rammer {
    float depth = 0.f;
    Vec2 nose;                // 몸 중심이 아니라 **코끝**. NosePoint가 만든다
    Vec2 velocity;
    // **반드시 채운다.** 0으로 두면 잡기 문턱이 0이 되어 "느리게 표류해도 잡힌다"가
    // 되고, 그러면 난이도 테스트가 초록불인 채 아무것도 검증하지 않는다.
    float maxSpeed = 0.f;
};

struct CatchParams {
    // 잡기에 필요한 접근 속도. 플레이어 최대 속도의 비율이라 최대 속도를 조정하면
    // 난이도가 **같이** 따라온다(숫자를 두 군데 두지 않는다).
    //
    // 0.55인 이유: 가만히 표류해 겹치는 것(최대 속도의 0.1 미만)은 확실히 걸러내되,
    // 방향키만으로 똑바로 달리면(1.0) 닿는다. 즉 **추격은 방향키로 되고, 달아나는
    // 놈을 따라잡는 데는 돌진이 필요하다.** 이 값이 난이도 손잡이 1번이고,
    // Task 16의 측정 결과로 조정한다 -- 예측이 아니라 측정으로.
    float catchSpeedFraction = 0.55f;
    // 코끝이 몸 반길이의 어디쯤인가. 1.0이면 정확히 주둥이 끝이라 판정이 너무 뾰족하다.
    float noseFraction = 0.9f;
    // 상대 타원을 이만큼 키워서 본다. 코끝은 점이고 실제 몸은 두께가 있기 때문이다.
    // **1.0 아래로 내리지 않는다** -- 클릭보다 어려워지는 순간 "닿았는데 안 됐다"가 된다.
    float graceScale = 1.15f;
};

enum class RamOutcome : int { Miss = 0, Bump = 1, Catch = 2 };

struct RamResult {
    RamOutcome outcome = RamOutcome::Miss;
    int targetIndex = -1;
    float closingSpeed = 0.f;   // 화면 단위/초. Bump/Catch일 때만 의미가 있다
};

// 카메라에서 본 좌표. 깊이로 나누는 원근 투영 하나뿐이고, 화면 비율이나 시야각은
// 들어오지 않는다 -- 겹쳤는지 아닌지는 그것들과 무관하기 때문이다.
inline Vec2 ToScreen(Vec2 shared, float depth, Vec3 camera) {
    const float d = depth - camera.x;
    if (d <= 1e-3f) return {0.f, 0.f};        // 카메라 뒤 또는 렌즈 위
    return {(shared.x - camera.y) / d, (shared.y - camera.z) / d};
}

// 방향 벡터는 깊이로 나누어도 방향이 바뀌지 않는다(균일 스케일). 속도의 **크기**만
// 화면 단위로 줄어든다.
inline Vec2 ToScreenVelocity(Vec2 velocity, float depth, Vec3 camera) {
    const float d = depth - camera.x;
    if (d <= 1e-3f) return {0.f, 0.f};
    return {velocity.x / d, velocity.y / d};
}

// 잡기에 필요한 접근 속도(화면 단위/초). 플레이어의 최대 속도와 깊이에서 **파생**한다.
inline float CatchThreshold(float playerMaxSpeed, float playerDepth, const CatchParams& p) {
    const float d = playerDepth <= 1e-3f ? 1.f : playerDepth;
    return p.catchSpeedFraction * playerMaxSpeed / d;
}

// 몸 중심과 진행 방향에서 코끝을 만든다. 정지 상태(방향 0)에서는 몸 중심 그대로다.
inline Vec2 NosePoint(Vec2 center, Vec2 heading, float halfLength, const CatchParams& p) {
    const Vec2 h = heading.Normalized();
    if (h.Length() <= 0.f) return center;
    return center + h * (halfLength * p.noseFraction);
}

// 판정에 문턱을 적용해 최종 결과를 만든다. EvaluateRam보다 **먼저** 정의해야 한다
// (아래에서 부른다). 겹침 계산과 난이도 손잡이를 분리해 두면 난이도를 조정해도
// 기하 테스트가 흔들리지 않는다.
inline RamResult ResolveRam(RamResult r, float playerMaxSpeed, float playerDepth,
                            const CatchParams& p) {
    if (r.targetIndex < 0) { r.outcome = RamOutcome::Miss; return r; }
    r.outcome = (r.closingSpeed >= CatchThreshold(playerMaxSpeed, playerDepth, p))
        ? RamOutcome::Catch : RamOutcome::Bump;
    return r;
}

// 지금 이 순간의 판정. **시간 인자도 각도 인자도 없다** -- 지속 시간 조건과 각도
// 조건을 넣는 것이 구조적으로 불가능하다는 뜻이고, 그 둘을 넣지 않기로 한 결정이
// 주석이 아니라 시그니처로 지켜진다.
inline RamResult EvaluateRam(Vec3 camera, const Rammer& me, const RamTarget* targets,
                             std::size_t count, const CatchParams& p) {
    RamResult r;
    if (targets == nullptr || count == 0) return r;
    const Vec2 myScreen = ToScreen(me.nose, me.depth, camera);
    const Vec2 myVelScreen = ToScreenVelocity(me.velocity, me.depth, camera);
    const Vec2 myDir = myVelScreen.Normalized();

    int best = -1;
    float bestDepth = 0.f;
    for (std::size_t i = 0; i < count; ++i) {
        const RamTarget& t = targets[i];
        if (t.halfWidth <= 0.f || t.halfHeight <= 0.f) continue;
        const float d = t.depth - camera.x;
        if (d <= 1e-3f) continue;                       // 카메라 뒤
        const Vec2 ts = ToScreen(t.center, t.depth, camera);
        // 화면에서 본 반지름. 깊이로 나뉘므로 멀수록 작아진다 -- 보이는 그대로다.
        const float hw = (t.halfWidth / d) * p.graceScale;
        const float hh = (t.halfHeight / d) * p.graceScale;
        const float u = (myScreen.x - ts.x) / hw;
        const float v = (myScreen.y - ts.y) / hh;
        if (u * u + v * v > 1.f) continue;              // 타원이다: 물고기는 길고 얇다
        if (best < 0 || t.depth < bestDepth) {          // 앞엣놈 = 작은 평면 X
            best = static_cast<int>(i);
            bestDepth = t.depth;
        }
    }
    if (best < 0) return r;

    r.targetIndex = best;
    const RamTarget& hit = targets[static_cast<std::size_t>(best)];
    const Vec2 itsVelScreen = ToScreenVelocity(hit.velocity, hit.depth, camera);
    // **접근 속도 = (내 속도 - 상대 속도)를 내 진행 방향에 투영한 값.**
    // 접촉 순간에는 두 점이 겹쳐 '서로를 향한 방향'이 정의되지 않으므로 내 진행
    // 방향을 기준으로 삼는다. 그 결과가 정확히 시나리오의 그림이다: 달아나는 놈을
    // 같은 속도로 따라가면 0이 되어 안 잡히고, 더 빨라야 잡힌다.
    const Vec2 rel = myVelScreen - itsVelScreen;
    r.closingSpeed = rel.x * myDir.x + rel.y * myDir.y;
    return ResolveRam(r, me.maxSpeed, me.depth, p);
}

} // namespace aquarium
```

> **빠지기 쉬운 함정:** `Rammer::maxSpeed`를 채우지 않은 호출이 하나라도 있으면 그 호출의 잡기 문턱이 **0**이 되어 "느리게 표류해도 잡힌다"가 된다. 그리고 그 경우 `drifting into a fish...` 테스트는 **빨간불이 아니라 그냥 통과하지 못하는 것이 아니라**, 문턱 0을 쓰는 다른 테스트들이 초록불인 채로 아무것도 검증하지 않게 된다(규약 6의 정확한 재발이다). 구현 뒤 `grep -n "Rammer " -A 6` 로 모든 생성 지점이 `maxSpeed`를 채우는지 눈으로 확인한다. 엔진 쪽 유일한 생성 지점은 `UCatchSubsystem::Tick`이다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_catch.cpp rules/include/aquarium/Catch.h && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 141`.

- [ ] **Step 3: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `ResolveRam`의 비교를 `r.closingSpeed >= 0.f`로 **대체**(문턱 제거) | `drifting into a fish does NOT catch it` |
| B | `r.closingSpeed`를 `myVelScreen` 크기만으로 **대체**(상대 속도를 무시) | `chasing a fish at the same speed never catches it` |
| C | `ToScreen`/`ToScreenVelocity`의 `/ d`를 **삭제**(깊이 정규화 제거) | `depth is normalized: two fish that look identical...` |
| D | 앞엣놈 고르기 `t.depth < bestDepth`를 `>`로 **역전** | `the frontmost overlapping fish is the one that gets hit` |
| E | `NosePoint`의 `noseFraction`을 0으로 **대체** | `the nose sits ahead of the body` |
| F | `CatchThreshold`를 상수 `0.25f` 반환으로 **대체** | `the catch threshold is DERIVED from...` |

변이 A·B가 이 마일스톤의 전제(어려움)를 지키는 시험이다. **A로 빨간불이 안 켜지면 문턱이 0으로 들어가고 있다는 뜻이다** — 위 전제 주의를 다시 읽는다. 전부 되돌린다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules tests CMakeLists.txt && git commit -q -F - << 'EOF'
feat(m8): 잡기의 기하 — 접촉은 화면에서 보고, 조건은 속도 하나뿐이다

물고기는 월드 X가 고정된 평면 위에 살고 내 물고기(X=220)와 배경 물고기(X>=330)는
3차원에서 닿을 수 없다. 평면 구조를 깨면 M3의 경계 보장과 M4c의 평면별 장애물
사전계산이 함께 무너지므로, 접촉을 카메라에서 본 겹침으로 정의했다 — 클릭이
이미 쓰는 것과 같은 기하다.

잡기의 조건은 접촉 + 상대 접근 속도뿐이다. 각도 조건은 아이 눈에 보이지 않고
지속 시간 조건은 즉시성과 부딪힌다. EvaluateRam에는 시간 인자도 각도 인자도
없어 그런 조건을 넣는 것이 구조적으로 불가능하다.

문턱을 없애는 변이와 상대 속도를 무시하는 변이로 각각 빨간불을 확인했다.

규칙 126 → 141.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 3: 규칙 계층 — 눈치채고 피하기 (`Evade.h`), 규칙 141 → 152

**Files:** Create `rules/include/aquarium/Evade.h`, Create `tests/test_evade.cpp`, Modify `CMakeLists.txt`

시나리오: "놈이 **먼저 눈치채고** 옆으로 튄다. 빗나간다. 다시 붙는다." **조준이 아니라 추격이 되는 이유가 이 헤더다.**

세 가지를 정한다.
- **언제 눈치채는가**: 화면에서 일정 반경 안에 들어왔고 **나를 향해 다가오는 중**일 때만. 옆으로 지나가는 물고기에 전부 반응하면 바다 전체가 계속 파닥거려 "내가 노린 놈이 반응했다"가 읽히지 않는다.
- **어디로 피하는가**: 접근선의 **수직**. 그것이 "옆으로 튄다"이다. 두 수직 중에서는 **자기가 이미 가던 쪽**을 고른다 — 관성이 자연스럽고, 무엇보다 **예측 가능**해서 아이가 몇 번 해 보면 각을 재게 된다(그것이 숙련이다). 완전히 수직이면 둘 다 0일 때가 있으므로 그때만 해시로 고른다.
- **벽에 몰리면 어떻게 되는가**: 특별 취급하지 않는다. 경계 규칙이 마지막에 벽을 따라 미끄러뜨린다. **구석으로 모는 것이 아이가 발견할 전략**이고, 여기서 벽을 피해 주면 그 전략이 사라진다.

- [ ] **Step 1 (RED): `tests/test_evade.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "aquarium/Evade.h"

using namespace aquarium;
using Catch::Approx;

namespace {
const EvadeParams kE{};

// 화면 좌표에서의 추격자. 깊이 정규화는 Catch.h가 이미 하므로 여기는 화면 단위만 본다.
Approach Toward(Vec2 from, Vec2 to, float speed) {
    Approach a;
    a.screenPos = from;
    a.screenVel = (to - from).Normalized() * speed;
    return a;
}
}

TEST_CASE("a fish notices a fast approach that is aimed at it") {
    const Approach a = Toward({-0.10f, 0.f}, {0.f, 0.f}, 0.5f);
    REQUIRE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("a fish ignores someone passing by, however close") {
    // 옆으로 스쳐 지나가는 것에까지 반응하면 바다 전체가 계속 파닥거려서
    // '내가 노린 놈이 반응했다'가 읽히지 않는다.
    // 추격자는 바로 아래(0.02)에 있고 **가로로** 지나간다 -- 나를 향한 성분이 0이다.
    Approach a;
    a.screenPos = {0.f, -0.02f};
    a.screenVel = {0.5f, 0.f};
    REQUIRE_FALSE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("a fish ignores an approach that is far away") {
    const Approach a = Toward({-2.f, 0.f}, {0.f, 0.f}, 0.5f);
    REQUIRE_FALSE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("a fish ignores a slow approach: it is not being chased") {
    const Approach a = Toward({-0.10f, 0.f}, {0.f, 0.f}, 0.002f);
    REQUIRE_FALSE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("a fish ignores someone moving away from it") {
    const Approach a = Toward({-0.10f, 0.f}, {-1.f, 0.f}, 0.5f);
    REQUIRE_FALSE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("the dodge is perpendicular to the approach -- that is 'jumps aside'") {
    const Vec2 approachDir{1.f, 0.f};
    const Vec2 d = DodgeDirection(approachDir, /*myVel*/ {0.f, 3.f}, /*seed*/ 1u);
    REQUIRE(d.Length() == Approx(1.f));
    REQUIRE(d.x == Approx(0.f).margin(1e-5f));    // 접근선과 수직
}

TEST_CASE("the dodge keeps the side the fish was already going -- so it can be read") {
    const Vec2 approachDir{1.f, 0.f};
    const Vec2 up = DodgeDirection(approachDir, {0.f, 5.f}, 1u);
    const Vec2 down = DodgeDirection(approachDir, {0.f, -5.f}, 1u);
    REQUIRE(up.y > 0.f);
    REQUIRE(down.y < 0.f);
}

TEST_CASE("a fish with no momentum still dodges, deterministically") {
    const Vec2 a = DodgeDirection({1.f, 0.f}, {0.f, 0.f}, 7u);
    const Vec2 b = DodgeDirection({1.f, 0.f}, {0.f, 0.f}, 7u);
    REQUIRE(a.Length() == Approx(1.f));
    REQUIRE(a.y == Approx(b.y));
    // 시드가 다르면 언젠가는 반대쪽도 나온다.
    bool sawOther = false;
    for (unsigned s = 0; s < 64u; ++s) {
        if (DodgeDirection({1.f, 0.f}, {0.f, 0.f}, s).y * a.y < 0.f) { sawOther = true; break; }
    }
    REQUIRE(sawOther);
}

TEST_CASE("a degenerate approach direction never produces a zero dodge") {
    // 0을 돌려주면 엔진은 '입력 없음'으로 읽고 감속한다 -- 놀라서 멈추는 물고기다.
    const Vec2 d = DodgeDirection({0.f, 0.f}, {0.f, 0.f}, 3u);
    REQUIRE(d.Length() == Approx(1.f));
}

TEST_CASE("the evade burst expires on its own and hands control back") {
    EvadeBehavior e;
    e.Notice({1.f, 0.f}, {0.f, 4.f}, 11u, kE);
    REQUIRE(e.Active());
    REQUIRE(e.SpeedScale(kE) > 1.f);
    e.Step(kE.duration * 0.5f);
    REQUIRE(e.Active());
    e.Step(kE.duration * 0.6f);
    REQUIRE_FALSE(e.Active());
    REQUIRE(e.SpeedScale(kE) == Approx(1.f));
}

TEST_CASE("noticing again while dodging re-aims and refills -- never 'ignored'") {
    // F-11에서 배운 것: 무시는 아이에게 '고장났다'로 읽힌다. 회피도 같다.
    EvadeBehavior e;
    e.Notice({1.f, 0.f}, {0.f, 4.f}, 11u, kE);
    e.Step(kE.duration * 0.9f);
    const Vec2 first = e.Direction();
    e.Notice({0.f, 1.f}, {4.f, 0.f}, 11u, kE);
    REQUIRE(e.Active());
    REQUIRE(e.NoticeCount() == 2);
    REQUIRE_FALSE(e.Direction().x == Approx(first.x));
    e.Step(kE.duration * 0.9f);
    REQUIRE(e.Active());          // 타이머가 새로 채워졌다
}

TEST_CASE("the dodge is faster than normal swimming but slower than a full startle") {
    // 회피가 놀람보다 세면 클릭이 의미를 잃는다(클릭은 몰이 도구여야 한다).
    REQUIRE(kE.dodgeSpeedScale > 1.f);
    REQUIRE(kE.dodgeSpeedScale < 2.2f);   // FleeParams::fleeSpeedScale
}
```

`CMakeLists.txt`에 `tests/test_evade.cpp`를 더한다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_evade.cpp && cmake -S . -B build && cmake --build build -j 2>&1 | tail -4
```
기대: `'aquarium/Evade.h' file not found`. RED다.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Evade.h`**

```cpp
#pragma once
#include <cstdint>

#include "aquarium/Reaction.h"   // ReactionHash
#include "aquarium/Vec2.h"

namespace aquarium {

// 시나리오: "놈이 **먼저 눈치채고** 옆으로 튄다." 이 헤더가 잡기를 조준이 아니라
// 추격으로 만든다. Flee(클릭에 놀람)와는 다른 사건이다 -- 회피는 아이가 아무것도
// 누르지 않아도 일어나고, 놀람보다 약하다(클릭은 몰이 도구로 남아야 한다).

// 다가오는 쪽을 화면 좌표로 본 것. 깊이 정규화는 Catch.h가 이미 했다.
struct Approach {
    Vec2 screenPos;
    Vec2 screenVel;
};

struct EvadeParams {
    // 눈치채는 화면 반경. 화면 절반 너비가 대략 tan(75/2) = 0.767 화면 단위이므로
    // 0.13은 **화면 너비의 약 8.5%**다 -- 코앞이지 시야 전체가 아니다.
    float noticeRadius = 0.13f;
    // 이보다 느리게 다가오면 추격으로 치지 않는다. 화면 단위/초.
    // 플레이어 최대 속도 90cm/s를 깊이 220으로 나누면 0.41이므로, 0.06은 그 15%다.
    float minClosing = 0.06f;
    float duration = 0.45f;        // 옆으로 튀는 시간
    float dodgeSpeedScale = 1.7f;  // **놀람(2.2)보다 약하다.** 클릭이 더 센 도구로 남는다
};

// 눈치채는가. 두 조건이 **모두** 필요하다: 가깝고, 나를 향해 오고 있다.
// 옆으로 지나가는 것에까지 반응하면 바다 전체가 계속 파닥거려서 "내가 노린 놈이
// 반응했다"가 읽히지 않는다.
inline bool ShouldNotice(Vec2 myScreenPos, const Approach& a, const EvadeParams& p) {
    const Vec2 toMe = myScreenPos - a.screenPos;
    const float dist = toMe.Length();
    if (dist > p.noticeRadius) return false;
    const Vec2 dir = toMe.Normalized();
    if (dir.Length() <= 0.f) return true;            // 정확히 겹쳤다: 당연히 눈치챈다
    const float closing = a.screenVel.x * dir.x + a.screenVel.y * dir.y;
    return closing >= p.minClosing;
}

// 접근선의 수직 방향. 두 수직 중 **자기가 이미 가던 쪽**을 고른다 -- 관성이
// 자연스럽고, 무엇보다 예측 가능해서 아이가 몇 번 해 보면 각을 재게 된다.
// 그것이 이 마일스톤이 말하는 숙련이다.
inline Vec2 DodgeDirection(Vec2 approachDir, Vec2 myVelocity, std::uint32_t seed) {
    Vec2 a = approachDir.Normalized();
    if (a.Length() <= 0.f) a = {1.f, 0.f};           // 절대 0을 돌려주지 않는다
    const Vec2 perp{-a.y, a.x};
    const float lean = myVelocity.x * perp.x + myVelocity.y * perp.y;
    if (lean > 1e-4f) return perp;
    if (lean < -1e-4f) return perp * -1.f;
    // 관성이 없을 때만 해시로 고른다(결정적이다).
    return (ReactionHash(seed) & 1u) ? perp : perp * -1.f;
}

// 한 마리의 회피 상태. FleeStateMachine과 같은 모양이라 엔진 쪽 사용법이 같다.
class EvadeBehavior {
public:
    // 다시 눈치채면 **다시 겨누고 타이머를 새로 채운다.** 무시하지 않는다 --
    // F-11에서 배운 것과 같다(무시는 아이에게 '고장났다'로 읽힌다).
    void Notice(Vec2 approachDir, Vec2 myVelocity, std::uint32_t seed, const EvadeParams& p) {
        dir_ = DodgeDirection(approachDir, myVelocity, seed);
        timer_ = p.duration;
        ++noticeCount_;
    }

    void Step(float dt) {
        if (dt <= 0.f || timer_ <= 0.f) return;
        timer_ -= dt;
        if (timer_ < 0.f) timer_ = 0.f;
    }

    bool Active() const { return timer_ > 0.f; }
    Vec2 Direction() const { return dir_; }
    int NoticeCount() const { return noticeCount_; }
    float SpeedScale(const EvadeParams& p) const { return Active() ? p.dodgeSpeedScale : 1.f; }

private:
    Vec2 dir_{1.f, 0.f};
    float timer_ = 0.f;
    int noticeCount_ = 0;
};

} // namespace aquarium
```

```bash
cd /Users/hans/dev/aquarium && touch tests/test_evade.cpp rules/include/aquarium/Evade.h && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 152`.

- [ ] **Step 3: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `ShouldNotice`의 접근 속도 검사를 **삭제**하고 `return true`로 | `a fish ignores someone moving away from it`, `...a slow approach` |
| B | `ShouldNotice`의 반경 검사를 **삭제** | `a fish ignores an approach that is far away` |
| C | `DodgeDirection`의 `perp`를 `a`로 **대체**(수직 대신 접근 방향) | `the dodge is perpendicular to the approach` |
| D | `lean` 분기를 **삭제**하고 항상 `perp` | `the dodge keeps the side the fish was already going` |
| E | `Notice`에서 `timer_` 재설정을 `if (timer_ <= 0.f)` 안으로 **가둔다**(옛 F-11 동작) | `noticing again while dodging re-aims and refills` |
| F | `EvadeParams::dodgeSpeedScale`을 `2.6f`로 **대체** | `the dodge is faster than normal swimming but slower than a full startle` |

전부 되돌린다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules tests CMakeLists.txt && git commit -q -F - << 'EOF'
feat(m8): 눈치채고 옆으로 튄다 — 조준이 아니라 추격이 되는 규칙

가깝고 나를 향해 올 때만 눈치챈다. 스쳐 지나가는 것에까지 반응하면 바다 전체가
파닥거려 "내가 노린 놈이 반응했다"가 읽히지 않는다. 피하는 방향은 접근선의
수직이되 자기가 이미 가던 쪽을 고른다 — 예측 가능해야 아이가 각을 재게 되고,
그것이 숙련이다.

회피는 놀람(2.2배)보다 약한 1.7배다. 클릭이 더 센 몰이 도구로 남아야 한다.
재차 눈치채면 다시 겨누고 타이머를 채운다(F-11에서 배운 것과 같다).

벽에 몰린 물고기를 특별 취급하지 않는다. 구석으로 모는 것은 아이가 발견할
전략이고, 여기서 벽을 피해 주면 그 전략이 사라진다.

규칙 141 → 152.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 4: 규칙 계층 — 돌진 (`Dash.h`), 규칙 152 → 161

**Files:** Create `rules/include/aquarium/Dash.h`, Create `tests/test_dash.cpp`, Modify `CMakeLists.txt`

시나리오: "가속/돌진 같은 것 하나만 있어도 잘하는 아이와 못하는 아이가 갈린다." **형태는 이 계획이 정한다.**

**결정: 스페이스바 한 번에 짧은 가속 한 번. 게이지도, 숫자도, 화면 표시도 없다.**
- 방향키는 이미 다 쓰고 있고 이 나이대는 동시 키를 쉽게 쓴다. 스페이스는 왼손 엄지로 닿는다.
- **거절하지 않는다.** 연타하면 점점 약해질 뿐 **항상 무언가는 일어난다.** 이것은 `VoiceLimiter`가 이미 쓰는 원칙 그대로다("시나리오가 금지한 것은 무시이지 작아짐이 아니다"). 쿨다운으로 무시하면 아이는 고장으로 읽고, 화면에 게이지를 띄우면 유치해지며 **잃는 자원**이 생긴다.
- 따라서 회복은 **연속**이다. 완전히 쉬면 최대, 연타하면 최소 배율. 피드백은 속도와 소리뿐이다.

- [ ] **Step 1 (RED): `tests/test_dash.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "aquarium/Dash.h"

using namespace aquarium;
using Catch::Approx;

namespace { const DashParams kD{}; }

TEST_CASE("a rested dash gives the full burst") {
    DashDrive d;
    d.Press(kD);
    REQUIRE(d.SpeedScale(kD) == Approx(kD.burstScale));
}

TEST_CASE("the burst decays back to normal and stays there") {
    DashDrive d;
    d.Press(kD);
    d.Step(kD.burstDuration * 0.5f, kD);
    const float mid = d.SpeedScale(kD);
    REQUIRE(mid > 1.f);
    REQUIRE(mid < kD.burstScale);
    d.Step(kD.burstDuration, kD);
    REQUIRE(d.SpeedScale(kD) == Approx(1.f));
    d.Step(10.f, kD);
    REQUIRE(d.SpeedScale(kD) == Approx(1.f));
}

TEST_CASE("mashing the key is never REFUSED, only weakened") {
    // 무시는 '고장났다'로 읽힌다. 거절을 표현할 반환값이 Press에 아예 없다.
    DashDrive d;
    float previous = 99.f;
    for (int i = 0; i < 8; ++i) {
        d.Press(kD);
        const float s = d.SpeedScale(kD);
        REQUIRE(s > 1.f);                 // 항상 무언가는 일어난다
        REQUIRE(s <= previous + 1e-4f);   // 점점 약해진다
        previous = s;
        d.Step(0.05f, kD);
    }
    REQUIRE(previous >= 1.f + (kD.burstScale - 1.f) * kD.minChargeFraction - 1e-3f);
}

TEST_CASE("resting restores the full burst") {
    DashDrive d;
    for (int i = 0; i < 6; ++i) { d.Press(kD); d.Step(0.05f, kD); }
    REQUIRE(d.SpeedScale(kD) < kD.burstScale);
    d.Step(kD.rechargeDuration * 2.f, kD);
    d.Press(kD);
    REQUIRE(d.SpeedScale(kD) == Approx(kD.burstScale));
}

TEST_CASE("charge recovers linearly and is clamped to [0,1]") {
    DashDrive d;
    REQUIRE(d.Charge() == Approx(1.f));
    d.Press(kD);
    REQUIRE(d.Charge() == Approx(0.f));
    d.Step(kD.rechargeDuration * 0.5f, kD);
    REQUIRE(d.Charge() == Approx(0.5f));
    d.Step(kD.rechargeDuration * 5.f, kD);
    REQUIRE(d.Charge() == Approx(1.f));
}

TEST_CASE("a zero or negative step changes nothing") {
    DashDrive d;
    d.Press(kD);
    const float before = d.SpeedScale(kD);
    d.Step(0.f, kD);
    d.Step(-1.f, kD);
    REQUIRE(d.SpeedScale(kD) == Approx(before));
}

TEST_CASE("the dash is worth pressing: it beats the catch threshold on its own") {
    // 돌진이 잡기 문턱을 넘기지 못하면 아무 의미가 없다. 이 단언이 두 헤더를 묶는다.
    // CatchParams::catchSpeedFraction = 0.55 -- 평속(1.0배)만으로도 넘지만,
    // 달아나는 놈(도망 2.2배)을 따라잡으려면 돌진이 필요하다.
    REQUIRE(kD.burstScale > 2.2f * 0.5f + 0.55f);
}

TEST_CASE("there is no cooldown that refuses, and no gauge value to display") {
    // 구조로 보장한다: Press에 반환값이 없고, '남은 횟수'를 돌려주는 함수도 없다.
    // Charge()는 0..1 연속값이라 '몇 발 남음'으로 그릴 수 없다.
    DashDrive d;
    d.Press(kD);
    d.Press(kD);
    d.Press(kD);
    REQUIRE(d.Charge() >= 0.f);
    REQUIRE(d.Charge() <= 1.f);
    REQUIRE(d.SpeedScale(kD) > 1.f);
}
```

`CMakeLists.txt`에 `tests/test_dash.cpp`를 더한다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_dash.cpp && cmake -S . -B build && cmake --build build -j 2>&1 | tail -4
```
기대: `'aquarium/Dash.h' file not found`. RED다.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Dash.h`**

```cpp
#pragma once

namespace aquarium {

// 돌진. 시나리오: "가속/돌진 같은 것 하나만 있어도 잘하는 아이와 못하는 아이가 갈린다."
//
// **거절하지 않는다.** 연타하면 약해질 뿐 항상 무언가는 일어난다 -- VoiceLimiter와
// 같은 원칙이다(금지된 것은 무시이지 작아짐이 아니다). 그래서 Press에는 실패를
// 표현할 반환값이 없고, '남은 횟수'를 돌려주는 함수도 없다. 화면에 게이지를 띄우지
// 않는 이유도 같다 -- 게이지는 곧 **잃는 자원**이고 그것은 벌이다.
struct DashParams {
    float burstScale = 2.4f;        // 최대 속도 배율의 꼭대기
    float burstDuration = 0.35f;    // 그 꼭대기에서 1로 내려오는 시간
    float rechargeDuration = 1.3f;  // 0에서 1까지 차는 시간(연속)
    // 완전히 방전됐을 때도 이만큼은 나간다. 0으로 두면 그것이 곧 거절이다.
    float minChargeFraction = 0.35f;
};

class DashDrive {
public:
    // 누르면 반드시 무언가 일어난다. 반환값이 없다 -- 구조적으로 거절이 불가능하다.
    void Press(const DashParams& p) {
        const float c = charge_ < p.minChargeFraction ? p.minChargeFraction : charge_;
        peak_ = 1.f + (p.burstScale - 1.f) * c;
        timer_ = p.burstDuration;
        charge_ = 0.f;
    }

    void Step(float dt, const DashParams& p) {
        if (dt <= 0.f) return;
        if (timer_ > 0.f) {
            timer_ -= dt;
            if (timer_ < 0.f) timer_ = 0.f;
        }
        if (p.rechargeDuration > 1e-4f) {
            charge_ += dt / p.rechargeDuration;
            if (charge_ > 1.f) charge_ = 1.f;
        } else {
            charge_ = 1.f;
        }
    }

    // 최대 속도에 곱할 배율. 꼭대기에서 1로 선형으로 내려온다 -- 한 번에 1로
    // 떨어뜨리면 브레이크를 밟은 것처럼 보인다(FleeStateMachine이 같은 이유로
    // 회복 구간을 선형으로 둔다).
    float SpeedScale(const DashParams& p) const {
        if (timer_ <= 0.f || p.burstDuration <= 1e-4f) return 1.f;
        const float t = timer_ / p.burstDuration;    // 1에서 0으로
        return 1.f + (peak_ - 1.f) * t;
    }

    // 0..1 연속값. '몇 발 남음'으로 그릴 수 없는 모양인 것이 의도다.
    float Charge() const { return charge_; }

private:
    float charge_ = 1.f;
    float timer_ = 0.f;
    float peak_ = 1.f;
};

} // namespace aquarium
```

```bash
cd /Users/hans/dev/aquarium && touch tests/test_dash.cpp rules/include/aquarium/Dash.h && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 161`.

- [ ] **Step 3: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `Press` 본문 첫 줄을 `if (charge_ < 1.f) return;`로 **대체**(쿨다운 도입) | `mashing the key is never REFUSED, only weakened` |
| B | `minChargeFraction`을 `0.f`로 **대체** | 같은 테스트의 마지막 단언 |
| C | `SpeedScale`의 선형 감쇠를 `return peak_;`로 **대체**(끝에서 뚝 떨어짐) | `the burst decays back to normal and stays there`의 중간 값 단언 |
| D | `Step`에서 `charge_` 갱신을 **삭제** | `resting restores the full burst`, `charge recovers linearly` |

변이 A가 이 헤더의 존재 이유(무시 금지)를 지키는 시험이다. 전부 되돌린다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules tests CMakeLists.txt && git commit -q -F - << 'EOF'
feat(m8): 돌진 — 거절하지 않고 약해질 뿐

스페이스 한 번에 짧은 가속 하나. 게이지도 남은 횟수도 화면 표시도 없다.
쿨다운으로 무시하면 아이는 고장으로 읽고, 게이지를 띄우면 잃는 자원이 생겨
시나리오가 금지한 벌이 된다. 그래서 Press에는 반환값이 없고 회복은 연속이다 —
VoiceLimiter가 이미 쓰는 원칙 그대로다.

쿨다운을 넣는 변이로 빨간불을 확인했다. 돌진이 잡기 문턱을 넘기는지도 두
헤더를 묶어 단언했다.

규칙 152 → 161.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 5: 규칙 계층 — 도장 (`Stamp.h`), 규칙 161 → 171

**Files:** Create `rules/include/aquarium/Stamp.h`, Create `tests/test_stamp.cpp`, Modify `CMakeLists.txt`

시나리오 결정표 그대로다: **개수 제한 없음, 떼기 기능 없음, 세션 동안 유지, 나가기로 리셋, 다 찍으면 바다가 조용히 달라진다.** 그리고 화면에 나가는 숫자는 여기서 나온다.

**떼기가 없다는 것을 주석이 아니라 타입으로 보장한다** — 한 마리를 지우는 함수가 클래스에 아예 없다. 있는 것은 세션 전체를 버리는 `Reset()`뿐이고, 그것은 "나가기"와 같은 사건이라 실수로 잃는 일이 될 수 없다.

- [ ] **Step 1 (RED): `tests/test_stamp.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "aquarium/Stamp.h"

using namespace aquarium;

TEST_CASE("stamping a fish counts it once") {
    StampBook b;
    REQUIRE(b.Count() == 0);
    REQUIRE(b.Stamp(7));
    REQUIRE(b.Count() == 1);
    REQUIRE(b.Has(7));
}

TEST_CASE("stamping the same fish again does not count twice, and is not an error") {
    // 이미 찍은 놈을 또 들이받는 것은 흔한 일이다. 숫자가 두 번 오르면 자랑거리가
    // 거짓이 되고, 거부당하면 벌처럼 읽힌다. 아무 일도 없는 것이 맞다.
    StampBook b;
    REQUIRE(b.Stamp(7));
    REQUIRE_FALSE(b.Stamp(7));
    REQUIRE(b.Count() == 1);
}

TEST_CASE("there is no limit on how many fish can be stamped") {
    StampBook b;
    for (int i = 0; i < 5000; ++i) REQUIRE(b.Stamp(i));
    REQUIRE(b.Count() == 5000);
}

TEST_CASE("the count only ever goes up within a session") {
    StampBook b;
    int previous = 0;
    for (int i = 0; i < 40; ++i) {
        b.Stamp(i % 13);          // 중복이 잔뜩 섞인 현실적인 흐름
        REQUIRE(b.Count() >= previous);
        previous = b.Count();
    }
    REQUIRE(b.Count() == 13);
}

TEST_CASE("leaving resets everything at once") {
    StampBook b;
    b.Stamp(1); b.Stamp(2);
    b.Reset();
    REQUIRE(b.Count() == 0);
    REQUIRE_FALSE(b.Has(1));
}

TEST_CASE("completion is measured against the number of fish that actually exist") {
    // 36을 리터럴로 박으면 배경 물고기 수를 바꾸는 날 완주가 조용히 깨진다(규약 8).
    StampBook b;
    const int total = 4;
    for (int i = 0; i < total; ++i) {
        REQUIRE_FALSE(b.IsComplete(total));
        b.Stamp(i);
    }
    REQUIRE(b.IsComplete(total));
}

TEST_CASE("completion of an empty sea is not claimed") {
    StampBook b;
    REQUIRE_FALSE(b.IsComplete(0));
}

TEST_CASE("completion fires exactly once, so the sea changes once") {
    StampBook b;
    const int total = 2;
    b.Stamp(0);
    REQUIRE_FALSE(b.ConsumeJustCompleted(total));
    b.Stamp(1);
    REQUIRE(b.ConsumeJustCompleted(total));
    REQUIRE_FALSE(b.ConsumeJustCompleted(total));   // 두 번째부터는 조용하다
}

TEST_CASE("the book has no way to remove a single stamp") {
    // 구조로 보장한다. 한 마리를 지우는 함수가 있으면 그것은 실수로 잃는 길이고,
    // 잃는 것은 이 게임에서 금지다. 아래는 그 사실을 사람이 읽도록 적어 둔 단언이다.
    StampBook b;
    b.Stamp(9);
    b.Stamp(9);
    b.Stamp(9);
    REQUIRE(b.Count() == 1);
    REQUIRE(b.Has(9));
}

TEST_CASE("unknown fish are simply not stamped") {
    StampBook b;
    REQUIRE_FALSE(b.Has(123));
}
```

`CMakeLists.txt`에 `tests/test_stamp.cpp`를 더한다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_stamp.cpp && cmake -S . -B build && cmake --build build -j 2>&1 | tail -4
```
기대: `'aquarium/Stamp.h' file not found`. RED다.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Stamp.h`**

```cpp
#pragma once
#include <algorithm>
#include <vector>

namespace aquarium {

// 아이가 찍은 도장의 장부. 시나리오 결정표: 개수 제한 없음, **떼기 기능 없음**,
// 세션 동안 유지, 나가기로 리셋.
//
// 떼기가 없다는 것을 주석이 아니라 **타입**으로 보장한다 -- 한 마리를 지우는 함수가
// 이 클래스에 아예 없다. 있는 것은 세션 전체를 버리는 Reset()뿐이고 그것은
// "나가기"와 같은 사건이라 실수로 잃는 일이 될 수 없다.
class StampBook {
public:
    // 처음 찍은 것이면 true. 이미 찍은 놈이면 false지만 **실패가 아니다** --
    // 숫자를 두 번 올리지 않을 뿐이고, 호출자는 아무 일도 하지 않으면 된다.
    bool Stamp(int fishId) {
        if (Has(fishId)) return false;
        ids_.push_back(fishId);
        return true;
    }

    bool Has(int fishId) const {
        return std::find(ids_.begin(), ids_.end(), fishId) != ids_.end();
    }

    // 화면 구석의 숫자. 세션 안에서 **오르기만 한다.**
    int Count() const { return static_cast<int>(ids_.size()); }

    // total은 실제로 존재하는 물고기 수에서 **파생**해 넘긴다. 36을 여기 박지 않는다.
    bool IsComplete(int total) const { return total > 0 && Count() >= total; }

    // 완주의 순간을 정확히 한 번만 돌려준다. 바다가 조용히 달라지는 일이 매 틱
    // 다시 일어나면 그것은 연출이 아니라 고장이다.
    bool ConsumeJustCompleted(int total) {
        if (!IsComplete(total) || announced_) return false;
        announced_ = true;
        return true;
    }

    // 나가기. 한 마리씩이 아니라 통째로다.
    void Reset() { ids_.clear(); announced_ = false; }

private:
    std::vector<int> ids_;
    bool announced_ = false;
};

} // namespace aquarium
```

```bash
cd /Users/hans/dev/aquarium && touch tests/test_stamp.cpp rules/include/aquarium/Stamp.h && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 171`.

- [ ] **Step 3: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `Stamp`의 중복 검사를 **삭제**(항상 push_back) | `stamping the same fish again does not count twice` |
| B | `IsComplete`를 `Count() >= 36`으로 **대체** | `completion is measured against the number of fish that actually exist` |
| C | `ConsumeJustCompleted`에서 `announced_ = true`를 **삭제** | `completion fires exactly once` |
| D | `IsComplete`의 `total > 0` 검사를 **삭제** | `completion of an empty sea is not claimed` |

전부 되돌린다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules tests CMakeLists.txt && git commit -q -F - << 'EOF'
feat(m8): 도장 장부 — 떼기가 타입에 없다

한 마리를 지우는 함수가 클래스에 아예 없다. 실수로 떼는 것은 잃는 일이고 잃는
것은 이 게임에서 금지다. 있는 것은 나가기와 같은 사건인 Reset() 하나뿐이다.

개수 제한 없음, 이미 찍은 놈을 또 받아도 거부가 아니라 무변화, 완주 수는
실제 물고기 수에서 파생, 완주 신호는 정확히 한 번. 36을 리터럴로 박는 변이와
중복 검사를 지우는 변이로 각각 빨간불을 확인했다.

규칙 161 → 171.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 6: 규칙 계층 — 타격의 연출값 (`Impact.h`), 규칙 171 → 179

**Files:** Create `rules/include/aquarium/Impact.h`, Create `tests/test_impact.cpp`, Modify `CMakeLists.txt`

시나리오: "**짧은 화면 흔들림.** 부딪힌 순간. **타격감의 9할이 여기서 나온다.**" 그리고 "물이 밀리는 왜곡".

흔들림과 왜곡의 **모양**은 순수 함수다. 엔진은 카메라 오프셋과 머티리얼 파라미터로 옮기기만 한다.

- [ ] **Step 1 (RED): `tests/test_impact.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "aquarium/Impact.h"

using namespace aquarium;
using Catch::Approx;

namespace { const ImpactParams kI{}; }

TEST_CASE("the shake starts immediately at full amplitude") {
    // 램프 인이 있으면 아이 눈에 늦게 시작한 것으로 보인다(장면 2 요구사항 1과 같은 이유).
    ImpactShake s;
    s.Hit(kI);
    const ShakeOffset o = s.Offset(kI);
    REQUIRE(std::abs(o.y) + std::abs(o.z) > kI.amplitudeCm * 0.5f);
}

TEST_CASE("the shake is SHORT and ends exactly at zero") {
    ImpactShake s;
    s.Hit(kI);
    s.Step(kI.duration * 0.999f);
    REQUIRE(s.Active());
    s.Step(kI.duration);
    REQUIRE_FALSE(s.Active());
    const ShakeOffset o = s.Offset(kI);
    REQUIRE(o.y == Approx(0.f));
    REQUIRE(o.z == Approx(0.f));
}

TEST_CASE("the shake decays: later is smaller than earlier") {
    ImpactShake a, b;
    a.Hit(kI); b.Hit(kI);
    b.Step(kI.duration * 0.7f);
    const float ea = std::abs(a.Offset(kI).y) + std::abs(a.Offset(kI).z);
    const float eb = std::abs(b.Offset(kI).y) + std::abs(b.Offset(kI).z);
    REQUIRE(eb < ea);
}

TEST_CASE("the shake never leaves a few centimetres -- it is a bump, not an earthquake") {
    ImpactShake s;
    s.Hit(kI);
    for (int i = 0; i < 200; ++i) {
        const ShakeOffset o = s.Offset(kI);
        REQUIRE(std::abs(o.y) <= kI.amplitudeCm + 1e-3f);
        REQUIRE(std::abs(o.z) <= kI.amplitudeCm + 1e-3f);
        s.Step(kI.duration / 100.f);
    }
}

TEST_CASE("the two axes are not the same wave") {
    // 두 축이 같은 위상이면 대각선으로만 흔들려 '툭 튕겼다'가 아니라 '미끄러졌다'가 된다.
    ImpactShake s;
    s.Hit(kI);
    s.Step(kI.duration * 0.15f);
    const ShakeOffset o = s.Offset(kI);
    REQUIRE_FALSE(o.y == Approx(o.z));
}

TEST_CASE("hitting again while shaking restarts, it does not stack") {
    // 겹쳐 쌓이면 연타할 때 화면이 아이를 멀미하게 만든다.
    ImpactShake s;
    s.Hit(kI);
    s.Step(kI.duration * 0.8f);
    s.Hit(kI);
    s.Step(kI.duration * 0.5f);
    REQUIRE(s.Active());
    const ShakeOffset o = s.Offset(kI);
    REQUIRE(std::abs(o.y) <= kI.amplitudeCm + 1e-3f);
}

TEST_CASE("the water displacement weight follows the same short life") {
    ImpactShake s;
    s.Hit(kI);
    REQUIRE(s.DisplacementWeight(kI) > 0.5f);
    s.Step(kI.duration * 2.f);
    REQUIRE(s.DisplacementWeight(kI) == Approx(0.f));
}

TEST_CASE("displacement decays monotonically -- no flicker") {
    // 왜곡이 사인처럼 깜빡이면 화면이 지글거린다. 흔들림만 진동하고 왜곡은 내려가기만 한다.
    ImpactShake s;
    s.Hit(kI);
    float previous = s.DisplacementWeight(kI);
    for (int i = 0; i < 30; ++i) {
        s.Step(kI.duration / 30.f);
        const float w = s.DisplacementWeight(kI);
        REQUIRE(w <= previous + 1e-5f);
        previous = w;
    }
}
```

`CMakeLists.txt`에 `tests/test_impact.cpp`를 더한다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_impact.cpp && cmake -S . -B build && cmake --build build -j 2>&1 | tail -4
```
기대: `'aquarium/Impact.h' file not found`. RED다.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Impact.h`**

```cpp
#pragma once

#include "aquarium/Reaction.h"   // PlayerReactionSin

namespace aquarium {

// 부딪힌 순간의 연출값. 시나리오: "짧은 화면 흔들림. **타격감의 9할이 여기서 나온다.**"
//
// 여기에는 **크기 배율이 없다.** 개정된 연출 문법이 부풀기를 유치함 신호로 못 박았고,
// Reaction.h가 같은 이유로 speedScale만 가진 것과 같은 결정이다.
struct ImpactParams {
    float duration = 0.20f;     // 짧다. 길면 '흔들림'이 아니라 '지진'이 된다
    float amplitudeCm = 6.0f;   // 카메라가 흔들리는 최대 거리
    float hz = 24.f;            // 빠르게 떨어야 '툭'이지 '흔들흔들'이 아니다
    float displacementMax = 1.f;// 물 밀림 왜곡의 최대 가중치(머티리얼로 간다)
};

struct ShakeOffset {
    float y = 0.f;   // 월드 Y(화면 가로)
    float z = 0.f;   // 월드 Z(화면 세로)
};

class ImpactShake {
public:
    // 다시 맞으면 **새로 시작한다.** 겹쳐 쌓으면 연타할 때 아이가 멀미한다.
    void Hit(const ImpactParams& p) { elapsed_ = 0.f; duration_ = p.duration; active_ = p.duration > 0.f; }

    void Step(float dt) {
        if (!active_ || dt <= 0.f) return;
        elapsed_ += dt;
    }

    // 시간까지 본다. active_만 보면 수명이 끝나도 참이라 Task 12의 카메라 흔들림이
    // 영원히 켜져 있는 것으로 읽힌다.
    bool Active() const { return active_ && elapsed_ < duration_; }

    // 감쇠하는 진동. 코사인이라 **첫 프레임부터 최대 진폭**이다 -- 사인으로 두면
    // t=0에서 0이라 아이 눈에 늦게 시작한 것으로 보인다(PlayerReaction과 같은 이유).
    ShakeOffset Offset(const ImpactParams& p) const {
        ShakeOffset o;
        if (!active_ || p.duration <= 0.f) return o;
        const float t = elapsed_ / p.duration;
        if (t >= 1.f) return o;
        const float fade = 1.f - t;
        const float w = 6.2831853f * p.hz * elapsed_;
        const float quarter = 1.5707963f;
        o.y = p.amplitudeCm * fade * PlayerReactionSin(w + quarter);
        // 세로축은 조금 다른 주파수와 위상이다. 같으면 대각선으로만 흔들려
        // '툭 튕겼다'가 아니라 '미끄러졌다'가 된다.
        o.z = p.amplitudeCm * 0.72f * fade * PlayerReactionSin(w * 1.37f);
        return o;
    }

    // 물 밀림 왜곡의 가중치. **진동하지 않고 내려가기만 한다** -- 깜빡이면 화면이
    // 지글거린다. 흔드는 것은 카메라 하나로 충분하다.
    float DisplacementWeight(const ImpactParams& p) const {
        if (!active_ || p.duration <= 0.f) return 0.f;
        const float t = elapsed_ / p.duration;
        if (t >= 1.f) return 0.f;
        const float fade = 1.f - t;
        return p.displacementMax * fade * fade;
    }

private:
    float elapsed_ = 0.f;
    float duration_ = 0.f;
    bool active_ = false;
};

} // namespace aquarium
```

> **주의:** `Active()`가 `duration_`을 들고 있는 이유는 `Hit`을 부른 뒤 시간이 다 지나도 참이면 카메라 흔들림이 영원히 켜진 것으로 읽히기 때문이다. `Offset`은 `t >= 1`에서 0을 돌려주므로 화면은 멀쩡해 보이고, **그래서 이 결함은 눈으로 안 잡힌다** — 테스트가 유일한 방어선이다. 빨간불이 나면 테스트를 완화하지 말고 구현을 맞춘다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_impact.cpp rules/include/aquarium/Impact.h && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 179`.

- [ ] **Step 3: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `Offset`의 코사인 위상 `+ quarter`를 **삭제**(사인으로) | `the shake starts immediately at full amplitude` |
| B | `o.z`의 배율·주파수를 `o.y`와 같게 **대체** | `the two axes are not the same wave` |
| C | `fade`를 **삭제**(감쇠 없음) | `the shake decays`, `the shake is SHORT and ends exactly at zero` |
| D | `DisplacementWeight`를 `PlayerReactionSin(w)`을 곱하도록 **대체** | `displacement decays monotonically -- no flicker` |
| E | `Hit`에서 `elapsed_ = 0.f`를 **삭제**(누적) | `hitting again while shaking restarts` |

전부 되돌린다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules tests CMakeLists.txt && git commit -q -F - << 'EOF'
feat(m8): 타격의 연출값 — 부풀리지 않고 흔든다

크기 배율 코드가 한 줄도 없다. 0.2초짜리 감쇠 진동 하나와, 진동하지 않고
내려가기만 하는 물 밀림 가중치 하나뿐이다. 두 축의 위상과 주파수를 다르게 둔
이유는 같으면 대각선으로만 흔들려 '툭 튕겼다'가 아니라 '미끄러졌다'가 되기
때문이다. 다시 맞으면 겹쳐 쌓이지 않고 새로 시작한다 — 연타에서 아이가
멀미하지 않아야 한다.

사인으로 되돌리는 변이(늦게 시작하는 흔들림)와 두 축을 같게 만드는 변이로
각각 빨간불을 확인했다.

규칙 171 → 179.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 7: 엔진 — 돌진 키와 코끝, Automation 73 → 77

**Files:** Modify `.../DiverPlayerController.h/.cpp`, Modify `.../FishActor.h/.cpp`, Modify `.../Tests/ControllerTests.cpp`

- [ ] **Step 1: 전제 확인 — 입력 바인딩이 어디서 일어나는지 본다**

```bash
cd /Users/hans/dev/aquarium && grep -n "BindKey\|EKeys::" unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp | head -20
```
기대: 방향키 네 개가 `BindKey`로 직접 묶여 있다. **다르면 그 방식을 따른다**(Enhanced Input을 새로 들이지 않는다 — 에셋이 필요해지고 그것은 스크립트로 못 만든다).

- [ ] **Step 2 (RED): 돌진이 속도를 올리고, 연타해도 거절당하지 않는다**

`unreal/Aquarium/Source/Aquarium/Tests/ControllerTests.cpp` 끝에 더한다. (이 파일이 쓰는 테스트 월드·세션 헬퍼 이름을 먼저 읽고 맞춘다.)

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishDashRaisesSpeed, "Aquarium.Fish.DashRaisesSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFishDashRaisesSpeed::RunTest(const FString&)
{
	FAquariumTestWorld World;
	AFishActor* Fish = /* 기존 헬퍼로 플레이어 물고기를 만든다 */ SpawnPlayerFishForTest(World);
	TestNotNull(TEXT("player fish"), Fish);
	Fish->SetInputDirection(FVector2D(1.f, 0.f));
	// 평속이 될 때까지 달린다.
	for (int32 i = 0; i < 240; ++i) { Fish->StepSwim(1.f / 60.f); }
	const float Cruise = Fish->CurrentSpeed();
	TestTrue(TEXT("cruising"), Cruise > 1.f);
	Fish->PressDash();
	Fish->StepSwim(1.f / 60.f);
	Fish->StepSwim(1.f / 60.f);
	TestTrue(TEXT("dash is faster than cruising"), Fish->CurrentSpeed() > Cruise * 1.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishDashNeverRefuses, "Aquarium.Fish.DashNeverRefuses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFishDashNeverRefuses::RunTest(const FString&)
{
	// 연타는 이 나이대의 기본 사용법이다(F-11에서 배운 것). 여덟 번 연타해도
	// 매번 평속보다는 빨라야 한다.
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnPlayerFishForTest(World);
	Fish->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Fish->StepSwim(1.f / 60.f); }
	const float Cruise = Fish->CurrentSpeed();
	for (int32 i = 0; i < 8; ++i)
	{
		Fish->PressDash();
		Fish->StepSwim(1.f / 60.f);
		TestTrue(*FString::Printf(TEXT("press %d still does something"), i), Fish->CurrentSpeed() > Cruise);
		for (int32 k = 0; k < 3; ++k) { Fish->StepSwim(1.f / 60.f); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishNoseIsAheadOfBody, "Aquarium.Fish.NoseIsAheadOfBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFishNoseIsAheadOfBody::RunTest(const FString&)
{
	// 코끝은 몸 중심보다 진행 방향 쪽에 있고, 몸 반길이를 넘지 않는다.
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnPlayerFishForTest(World);
	Fish->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 120; ++i) { Fish->StepSwim(1.f / 60.f); }
	const aquarium::ClickTarget Body = Fish->AsClickTarget();
	const aquarium::Vec2 Nose = Fish->NosePoint();
	TestTrue(TEXT("nose leads the centre"), Nose.x > Body.center.x);
	TestTrue(TEXT("nose is on the body"), Nose.x - Body.center.x <= Body.halfWidth + 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerDashKeyIsBound, "Aquarium.Controller.DashKeyIsBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FControllerDashKeyIsBound::RunTest(const FString&)
{
	// 스페이스가 실제로 물고기까지 도달하는지. 컨트롤러의 공개 진입점을 직접 부른다
	// (-nullrhi에는 실제 키 이벤트가 없다 -- 클릭과 같은 사정이다).
	FAquariumTestWorld World;
	ADiverPlayerController* PC = /* 기존 헬퍼 */ SpawnControllerForTest(World);
	AFishActor* Fish = World.GameMode()->PlayerFish();
	TestNotNull(TEXT("player fish"), Fish);
	const int32 Before = Fish->DashPressCount();
	PC->HandleDashPressed();
	TestEqual(TEXT("the press reached the fish"), Fish->DashPressCount(), Before + 1);
	return true;
}
```

빌드하면 `PressDash`/`NosePoint`/`DashPressCount`/`HandleDashPressed`가 없어 **컴파일 에러**다. RED다.

- [ ] **Step 3 (GREEN): `AFishActor`에 돌진과 코끝을 더한다**

`FishActor.h`에 포함과 선언을 더한다.

```cpp
#include "aquarium/Catch.h"
#include "aquarium/Dash.h"
#include "aquarium/Evade.h"
```

public 구역에:

```cpp
	// 돌진 한 번. **거절하지 않는다**(aquarium::DashDrive에 실패를 표현할 반환값이 없다).
	void PressDash();
	int32 DashPressCount() const { return DashPressCountValue; }
	// 공유 유영 프레임에서 본 코끝. 몸 중심이 아니라 여기가 들이받는 점이다.
	aquarium::Vec2 NosePoint() const;
	// 이 물고기를 들이받히는 쪽으로 본 것. AsClickTarget과 같은 렌더 바운드에서 파생한다.
	aquarium::RamTarget AsRamTarget() const;
	// 다가오는 추격자를 눈치챘다. 화면 좌표는 호출자(UCatchSubsystem)가 계산해 넘긴다.
	void NoticeApproach(const aquarium::Vec2& ApproachDirShared);
	int32 EvadeNoticeCount() const { return EvadeValue.NoticeCount(); }
	// 도장이 박혔는지. 세션 리셋으로만 풀린다.
	bool IsStamped() const { return bStamped; }
```

private 구역에:

```cpp
	aquarium::DashDrive Dash;
	aquarium::DashParams DashParamsValue;
	aquarium::EvadeBehavior EvadeValue;
	aquarium::EvadeParams EvadeParamsValue;
	aquarium::CatchParams CatchParamsValue;
	int32 DashPressCountValue = 0;
	bool bStamped = false;
```

`FishActor.cpp`:

```cpp
void AFishActor::PressDash()
{
	// 내 물고기만 돌진한다. 배경 물고기에 걸리면 바다 전체가 튀어 나간다.
	if (!bPlayerControlled && !bIsPlayerFish) return;
	Dash.Press(DashParamsValue);
	++DashPressCountValue;
}

aquarium::Vec2 AFishActor::NosePoint() const
{
	const aquarium::ClickTarget T = AsClickTarget();
	// 진행 방향이 0이면(정지) 몸 중심 그대로. NosePoint가 0으로 나누지 않는다.
	return aquarium::NosePoint(T.center, Motion.velocity, T.halfWidth, CatchParamsValue);
}

aquarium::RamTarget AFishActor::AsRamTarget() const
{
	const aquarium::ClickTarget T = AsClickTarget();
	aquarium::RamTarget R;
	R.depth = T.depth;
	R.center = T.center;
	R.halfWidth = T.halfWidth;
	R.halfHeight = T.halfHeight;
	R.velocity = Motion.velocity;
	R.alreadyStamped = bStamped;
	return R;
}

void AFishActor::NoticeApproach(const aquarium::Vec2& ApproachDirShared)
{
	// 내 물고기는 자기 자신을 피하지 않는다.
	if (bPlayerControlled || bIsPlayerFish) return;
	EvadeValue.Notice(ApproachDirShared, Motion.velocity, Seed + static_cast<uint32>(EvadeValue.NoticeCount()), EvadeParamsValue);
}
```

`InitializeSwim()` 안에서 상태를 초기화한다(`Flee = aquarium::FleeStateMachine();` 옆):

```cpp
	Dash = aquarium::DashDrive();
	EvadeValue = aquarium::EvadeBehavior();
	DashPressCountValue = 0;
	bStamped = false;
```

`StepSwim`의 최대 속도 계산 줄을 고친다. **돌진은 기존 배율에 곱한다** — 도망·스타일 배율과 같은 자리이고, 그래야 속도 상수가 여전히 규칙 계층에만 있다.

```cpp
	Dash.Step(DeltaSeconds, DashParamsValue);
	MotionParamsValue.maxSpeed = MaxSpeed * Flee.SpeedScale(FleeParamsValue) * StyleScale * Dash.SpeedScale(DashParamsValue);
```

`DiverPlayerController.h` public에:

```cpp
	// 스페이스. 공개인 이유는 클릭과 같다: -nullrhi에는 실제 키 이벤트가 없어
	// 자동화가 여기서 시작해야 한다.
	void HandleDashPressed();
```

`DiverPlayerController.cpp`의 `SetupInputComponent()` 안, 방향키 바인딩 옆에:

```cpp
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ADiverPlayerController::HandleDashPressed);
```

그리고:

```cpp
void ADiverPlayerController::HandleDashPressed()
{
	if (AAquariumGameMode* GM = GameMode())
	{
		if (AFishActor* Fish = GM->PlayerFish())
		{
			Fish->PressDash();
		}
	}
}
```

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `77`.

- [ ] **Step 4: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `StepSwim`의 `* Dash.SpeedScale(...)`를 **삭제** | `Aquarium.Fish.DashRaisesSpeed` |
| B | `PressDash` 첫 줄을 `if (Dash.Charge() < 1.f) return;`로 **대체** | `Aquarium.Fish.DashNeverRefuses` |
| C | `NosePoint`의 `Motion.velocity`를 `aquarium::Vec2{}`로 **대체** | `Aquarium.Fish.NoseIsAheadOfBody` |
| D | `HandleDashPressed` 본문을 **삭제** | `Aquarium.Controller.DashKeyIsBound` |

**주의:** 변이 C가 빨간불이 안 되면 테스트가 정지 상태의 물고기를 보고 있는 것이다(속도가 0이면 코끝 = 몸 중심이라 `>`가 거짓이 되므로 실제로는 빨간불이 맞다). **빨간불이 안 켜지면 워밍업 스텝이 실제로 물고기를 움직였는지 먼저 확인한다** — M7에서 정확히 이 종류의 빈 테스트가 있었다. 전부 되돌린다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 돌진 키와 코끝 — 잘하는 아이와 못하는 아이가 갈리는 한 가지

스페이스 한 번에 짧은 가속. 돌진 배율은 도망·스타일 배율과 같은 자리에서
곱해지므로 속도 상수는 여전히 규칙 계층에만 있다. 배율을 빼는 변이와 쿨다운을
넣는 변이로 각각 빨간불을 확인했다.

들이받는 점은 몸 중심이 아니라 코끝이다. 머리로 받아야 '들이받았다'이고,
꼬리로 스친 것이 잡기가 되면 아이는 자기가 무엇을 한 건지 모른다.

Automation 73 → 77.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 8: 엔진 — 회피 층을 규칙 순서에 끼운다, Automation 77 → 81

**Files:** Modify `.../FishActor.cpp`, Modify `.../Tests/FishActorTests.cpp`

**규칙 순서는 이 프로젝트에서 가장 비싸게 얻은 불변식이다.** 회피가 들어갈 자리는 정확히 한 곳이다:

`입력/Wander → 먹이 → **회피** → 도망 → 무리 → 장애물 → 경계 → StepMotion → Clamp`

- **회피는 "어디로 가고 싶은가"의 답을 대체**하므로 입력/Wander와 같은 층이다(보정이 아니다).
- **도망보다 앞**이다. 클릭의 결과는 아이가 읽어야 하는 사건이고(M5가 무리를 도망 뒤로 보낸 것과 같은 이유), 이미 도망 중인 놈은 그것만으로 충분히 어렵다. 즉 **도망이 회피를 이긴다.**
- **경계는 끝까지 마지막이다.** M3의 "어떤 화면 비율에서도 영역을 벗어나지 않는다"가 여기 걸려 있다. 회피는 경계를 볼 수 없고, 봐서도 안 된다 — 구석으로 모는 것이 아이가 발견할 전략이기 때문이다.

- [ ] **Step 1 (RED): `Tests/FishActorTests.cpp`에 네 개를 더한다**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishEvadesSideways, "Aquarium.Fish.EvadesSideways",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFishEvadesSideways::RunTest(const FString&)
{
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnAimedFish(World, FVector2D(1.f, 0.f));   // 기존 헬퍼
	for (int32 i = 0; i < 120; ++i) { Fish->StepSwim(1.f / 60.f); }
	const aquarium::Vec2 Before = Fish->AsClickTarget().center;
	// 왼쪽에서 다가온다 -> 위나 아래로 튀어야 한다.
	Fish->NoticeApproach(aquarium::Vec2{1.f, 0.f});
	for (int32 i = 0; i < 20; ++i) { Fish->StepSwim(1.f / 60.f); }
	const aquarium::Vec2 After = Fish->AsClickTarget().center;
	TestTrue(TEXT("moved sideways"), FMath::Abs(After.y - Before.y) > 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishFleeBeatsEvade, "Aquarium.Fish.FleeBeatsEvade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFishFleeBeatsEvade::RunTest(const FString&)
{
	// 클릭의 결과는 아이가 읽어야 한다. 도망 중에 회피가 방향을 빼앗으면 못 읽는다.
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnAimedFish(World, FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 120; ++i) { Fish->StepSwim(1.f / 60.f); }
	const FVector Touch = Fish->GetActorLocation() + FVector(0.f, -40.f, 0.f);
	Fish->ApplyFleeFrom(Touch);
	Fish->NoticeApproach(aquarium::Vec2{0.f, 1.f});
	Fish->StepSwim(1.f / 60.f);
	const aquarium::Vec2 Before = Fish->AsClickTarget().center;
	for (int32 i = 0; i < 10; ++i) { Fish->StepSwim(1.f / 60.f); }
	const aquarium::Vec2 After = Fish->AsClickTarget().center;
	// 터치에서 멀어지는 쪽(+Y)으로 간다. 회피가 이겼다면 세로로 갔을 것이다.
	TestTrue(TEXT("flee direction wins"), After.x - Before.x > FMath::Abs(After.y - Before.y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishEvadeCannotLeaveTheArea, "Aquarium.Fish.EvadeCannotLeaveTheArea",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFishEvadeCannotLeaveTheArea::RunTest(const FString&)
{
	// M3의 보장: 경계가 언제나 마지막에 이긴다. 벽에 붙은 물고기를 벽 쪽으로
	// 계속 피하게 만들어도 영역을 벗어나지 못해야 한다.
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnAimedFish(World, FVector2D(0.f, 1.f));
	for (int32 i = 0; i < 600; ++i) { Fish->StepSwim(1.f / 60.f); }   // 위쪽 벽으로 붙는다
	for (int32 i = 0; i < 600; ++i)
	{
		Fish->NoticeApproach(aquarium::Vec2{1.f, 0.f});               // 계속 위로 튀게 만든다
		Fish->StepSwim(1.f / 60.f);
		const aquarium::Vec2 P = Fish->AsClickTarget().center;
		const float LocalY = P.y - static_cast<float>(Fish->PlaneOrigin.Z);
		TestTrue(TEXT("inside the area"), LocalY <= Fish->PlaneHalfHeight + 0.01f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishEvadeIsFasterThanCruise, "Aquarium.Fish.EvadeIsFasterThanCruise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFishEvadeIsFasterThanCruise::RunTest(const FString&)
{
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnAimedFish(World, FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 300; ++i) { Fish->StepSwim(1.f / 60.f); }
	const float Cruise = Fish->CurrentSpeed();
	Fish->NoticeApproach(aquarium::Vec2{1.f, 0.f});
	for (int32 i = 0; i < 12; ++i) { Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("dodging is faster"), Fish->CurrentSpeed() > Cruise * 1.05f);
	return true;
}
```

> **전제 확인(M7이 당한 함정의 재발 방지):** `SpawnAimedFish`는 M7에서 **워밍업 뒤 `bPlayerControlled`를 끄도록 고쳐졌다.** 끄지 않으면 회피가 입력에 덮여 위 테스트 넷이 전부 빈 테스트가 된다. 실제 헬퍼 본문을 읽고 그 동작을 **확인한 뒤** 진행한다:
> `grep -n "SpawnAimedFish" -A 20 unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 2 (GREEN): `StepSwim`에 한 칸을 끼운다**

`FishActor.cpp`의 `Flee.Step(DeltaSeconds);` **바로 앞**에 넣는다:

```cpp
	// 회피 층. 입력/Wander와 같은 층이다 -- "어디로 가고 싶은가"의 답을 **대체**한다.
	// 도망보다 **앞**인 이유: 클릭의 결과는 아이가 읽어야 하는 사건이고(M5가 무리를
	// 도망 뒤로 보낸 것과 같은 이유), 이미 도망 중인 놈은 그것만으로 충분히 어렵다.
	// 즉 아래의 Flee 블록이 이 값을 덮어쓰는 것이 의도된 우선순위다.
	EvadeValue.Step(DeltaSeconds);
	const bool bEvading = EvadeValue.Active();
	if (bEvading)
	{
		Desired = EvadeValue.Direction();
	}
```

그리고 최대 속도 계산에 회피 배율을 곱한다(돌진과 같은 자리):

```cpp
	MotionParamsValue.maxSpeed = MaxSpeed * Flee.SpeedScale(FleeParamsValue) * StyleScale
		* Dash.SpeedScale(DashParamsValue) * EvadeValue.SpeedScale(EvadeParamsValue);
```

무리 조건에도 회피를 더한다 — 피하는 중에 무리가 방향을 섞으면 "옆으로 튄다"가 흐려진다:

```cpp
	if (!bPlayerControlled && !bIsPlayerFish && !bFleeing && !bEvading)
```

**장애물·경계·StepMotion·Clamp는 한 줄도 건드리지 않는다.** 그것이 이 태스크의 핵심이다.

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `81`. **기존 테스트가 하나라도 빨간불이 되면 멈춘다** — 규칙 순서를 깬 것이다.

- [ ] **Step 3: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | 회피 블록을 `Flee.Step` **뒤로 옮긴다**(회피가 도망을 이긴다) | `Aquarium.Fish.FleeBeatsEvade` |
| B | 회피 블록을 경계 계산 **뒤로 옮긴다**(`Dir` 계산 다음에 `Dir = EvadeValue.Direction()`) | `Aquarium.Fish.EvadeCannotLeaveTheArea` |
| C | `Desired = EvadeValue.Direction();`을 **삭제** | `Aquarium.Fish.EvadesSideways` |
| D | 속도 배율의 `* EvadeValue.SpeedScale(...)`를 **삭제** | `Aquarium.Fish.EvadeIsFasterThanCruise` |

**변이 B가 이 계획 전체에서 가장 중요한 변이다.** 경계가 마지막이라는 보장이 실제로 테스트에 걸려 있는지를 증명한다. 빨간불이 안 켜지면 M3의 보장은 지금 **아무것도 지키지 않는 상태**라는 뜻이므로 멈추고 보고한다. 전부 되돌린다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 회피를 규칙 순서에 끼운다 — 도망 앞, 경계보다 한참 앞

입력/Wander → 먹이 → 회피 → 도망 → 무리 → 장애물 → 경계 → StepMotion → Clamp.
회피는 "어디로 가고 싶은가"의 답을 대체하므로 입력과 같은 층이고, 도망보다
앞이다(클릭의 결과는 아이가 읽어야 한다). 경계는 끝까지 마지막이다.

회피를 경계 뒤로 옮기는 변이로 M3의 "영역을 벗어나지 않는다" 보장이 실제로
빨간불이 되는 것을 확인했다.

Automation 77 → 81.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 9: 엔진 — 잡기 판정 서브시스템, Automation 81 → 88

**Files:** Create `.../CatchSubsystem.h/.cpp`, Modify `.../FishSchoolSubsystem.h/.cpp`, Modify `.../FishActor.h/.cpp`, Create `.../Tests/CatchTests.cpp`, Modify `.../Aquarium.Build.cs`(필요 시)

- [ ] **Step 1: 전제 확인 — 등록된 물고기를 훑을 수 있는가**

```bash
cd /Users/hans/dev/aquarium && grep -n "Fishes" unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.h
```
기대: `TArray<TWeakObjectPtr<AFishActor>> Fishes;`가 **private**이다. 그러면 접근자를 더한다(아래 Step 3).

- [ ] **Step 2 (RED): `Tests/CatchTests.cpp`**

```cpp
#include "Misc/AutomationTest.h"

#include "AquariumGameMode.h"
#include "CatchSubsystem.h"
#include "FishActor.h"
#include "FishSchoolSubsystem.h"

// 이 파일의 테스트 월드 헬퍼는 기존 Tests/*.cpp가 쓰는 것과 같은 것을 쓴다.
// (실제 이름을 먼저 확인하고 맞춘다 -- 계획이 이름을 지어내지 않는다.)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchFastRamStamps, "Aquarium.Catch.FastRamStamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchFastRamStamps::RunTest(const FString&)
{
	FAquariumTestWorld World;
	UCatchSubsystem* Catch = World.Get()->GetSubsystem<UCatchSubsystem>();
	TestNotNull(TEXT("catch subsystem"), Catch);
	// 내 물고기를 빠르게 달리게 하고, 그 코끝의 화면 위치에 배경 물고기를 놓는다.
	AFishActor* Mine = World.GameMode()->PlayerFish();
	AFishActor* Target = Catch->SpawnTargetUnderNoseForTest(Mine);
	TestNotNull(TEXT("target"), Target);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	Catch->SetTargetUnderNoseForTest(Target, Mine);      // 코끝 화면 위치에 다시 맞춘다
	TestEqual(TEXT("nothing stamped yet"), Catch->StampCount(), 0);
	Catch->Tick(1.f / 60.f);
	TestEqual(TEXT("one stamp"), Catch->StampCount(), 1);
	TestTrue(TEXT("that fish carries the mark"), Target->IsStamped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchSlowDriftDoesNotStamp, "Aquarium.Catch.SlowDriftDoesNotStamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchSlowDriftDoesNotStamp::RunTest(const FString&)
{
	// 이 게임이 어렵다는 주장이 걸려 있는 테스트다. 느리게 겹쳐지는 것은 잡기가 아니다.
	FAquariumTestWorld World;
	UCatchSubsystem* Catch = World.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	AFishActor* Target = Catch->SpawnTargetUnderNoseForTest(Mine);
	Mine->SetInputDirection(FVector2D(0.f, 0.f));        // 표류
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	Catch->SetTargetUnderNoseForTest(Target, Mine);
	Catch->Tick(1.f / 60.f);
	TestEqual(TEXT("no stamp"), Catch->StampCount(), 0);
	TestFalse(TEXT("not stamped"), Target->IsStamped());
	// 다만 아무 일도 없지는 않다: 놈은 놀라서 튄다. 그것이 다시 붙을 이유가 된다.
	TestTrue(TEXT("the target was startled"), Target->FleeState() != aquarium::BehaviorState::Normal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchCountsEachFishOnce, "Aquarium.Catch.CountsEachFishOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchCountsEachFishOnce::RunTest(const FString&)
{
	FAquariumTestWorld World;
	UCatchSubsystem* Catch = World.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	AFishActor* Target = Catch->SpawnTargetUnderNoseForTest(Mine);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	for (int32 i = 0; i < 5; ++i)
	{
		Catch->SetTargetUnderNoseForTest(Target, Mine);
		Catch->Tick(1.f / 60.f);
	}
	TestEqual(TEXT("still one"), Catch->StampCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchNeverStampsMyOwnFish, "Aquarium.Catch.NeverStampsMyOwnFish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchNeverStampsMyOwnFish::RunTest(const FString&)
{
	FAquariumTestWorld World;
	UCatchSubsystem* Catch = World.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	for (int32 i = 0; i < 30; ++i) { Catch->Tick(1.f / 60.f); }
	TestEqual(TEXT("no stamps"), Catch->StampCount(), 0);
	TestFalse(TEXT("my fish is not stamped"), Mine->IsStamped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchNoticesBeforeContact, "Aquarium.Catch.NoticesBeforeContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchNoticesBeforeContact::RunTest(const FString&)
{
	// "놈이 먼저 눈치채고" -- 닿기 전에 회피가 걸려야 한다. 걸리지 않으면 이 게임은
	// 추격이 아니라 조준이 되고, 그러면 어렵지 않다.
	FAquariumTestWorld World;
	UCatchSubsystem* Catch = World.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	AFishActor* Target = Catch->SpawnTargetAheadForTest(Mine, /*screenGap*/ 0.08f);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	Catch->Tick(1.f / 60.f);
	TestTrue(TEXT("noticed"), Target->EvadeNoticeCount() > 0);
	TestFalse(TEXT("not caught yet"), Target->IsStamped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchPlaysThudOnlyOnACatch, "Aquarium.Catch.PlaysThudOnlyOnACatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchPlaysThudOnlyOnACatch::RunTest(const FString&)
{
	FAquariumTestWorld World;
	UCatchSubsystem* Catch = World.Get()->GetSubsystem<UCatchSubsystem>();
	UAquariumAudioSubsystem* Audio = World.Get()->GetSubsystem<UAquariumAudioSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	AFishActor* Target = Catch->SpawnTargetUnderNoseForTest(Mine);
	Mine->SetInputDirection(FVector2D(0.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	Catch->SetTargetUnderNoseForTest(Target, Mine);
	Catch->Tick(1.f / 60.f);
	TestEqual(TEXT("no thud on a bump"), Audio->CuePlayCount(EAquariumCue::Thud), 0);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	Catch->SetTargetUnderNoseForTest(Target, Mine);
	Catch->Tick(1.f / 60.f);
	TestEqual(TEXT("thud on a catch"), Audio->CuePlayCount(EAquariumCue::Thud), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchResetsOnLeaving, "Aquarium.Catch.ResetsOnLeaving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchResetsOnLeaving::RunTest(const FString&)
{
	// 시나리오: 세션 동안 유지, 나가기로 리셋.
	FAquariumTestWorld World;
	UCatchSubsystem* Catch = World.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	AFishActor* Target = Catch->SpawnTargetUnderNoseForTest(Mine);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	Catch->SetTargetUnderNoseForTest(Target, Mine);
	Catch->Tick(1.f / 60.f);
	TestEqual(TEXT("one stamp"), Catch->StampCount(), 1);
	World.GameMode()->EndSession();
	TestEqual(TEXT("reset to zero"), Catch->StampCount(), 0);
	TestFalse(TEXT("the mark is gone from the fish too"), Target->IsStamped());
	return true;
}
```

빌드하면 `CatchSubsystem.h`가 없어 **컴파일 에러**다. RED다.

- [ ] **Step 3 (GREEN): 서브시스템**

`FishSchoolSubsystem.h` public에 접근자를 더한다:

```cpp
	// 등록된 물고기 자체. 잡기 판정은 BoidNeighbor 스냅샷으로는 안 된다 --
	// 액터에 도장을 찍고 회피를 걸어야 하기 때문이다.
	const TArray<TWeakObjectPtr<AFishActor>>& RegisteredFish() const { return Fishes; }
```

`unreal/Aquarium/Source/Aquarium/CatchSubsystem.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <vector>

#include "aquarium/Catch.h"
#include "aquarium/Evade.h"
#include "aquarium/Impact.h"
#include "aquarium/Stamp.h"

#include "CatchSubsystem.generated.h"

class AFishActor;
class ACameraActor;

// 부딪혀 잡기의 판정과 결과를 모두 가진 곳. **판정은 조향이 아니라 관측이므로
// 규칙 순서 바깥이다** -- 모든 물고기가 한 틱을 다 움직인 뒤에 읽기만 한다.
// 액터 틱은 하나도 늘지 않는다(서브시스템 하나가 전부 훑는다).
UCLASS()
class AQUARIUM_API UCatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCatchSubsystem, STATGROUP_Tickables); }

	// 화면 구석의 숫자. 오르기만 한다.
	int32 StampCount() const { return Book.Count(); }
	// 이 바다에 있는 잡을 수 있는 물고기 수. 36을 리터럴로 쓰지 않기 위한 파생값이다.
	int32 CatchableCount() const;
	// 나가기. 장부를 비우고 모든 도장을 떼어 낸다(한 마리씩이 아니라 통째로).
	void ResetSession();

	// 부딪힌 순간의 화면 흔들림. 카메라를 실제로 흔드는 일은 Task 12가 한다.
	const aquarium::ImpactShake& Shake() const { return ShakeValue; }
	const aquarium::ImpactParams& ShakeParams() const { return ImpactParamsValue; }

	// 개발 전용: 판정을 통째로 끈다(성능 귀속). 값을 받지 않는 불리언이다.
	bool bCatchEnabled = true;

	// --- 테스트 전용 ---
	// 내 물고기 코끝의 화면 위치에 배경 물고기 한 마리를 놓는다.
	AFishActor* SpawnTargetUnderNoseForTest(AFishActor* Mine);
	void SetTargetUnderNoseForTest(AFishActor* Target, AFishActor* Mine);
	// 코끝 앞쪽 ScreenGap만큼 떨어진 자리에 놓는다(회피 시험용).
	AFishActor* SpawnTargetAheadForTest(AFishActor* Mine, float ScreenGap);

private:
	aquarium::StampBook Book;
	aquarium::CatchParams CatchParamsValue;
	aquarium::EvadeParams EvadeParamsValue;
	aquarium::ImpactParams ImpactParamsValue;
	aquarium::ImpactShake ShakeValue;
	std::vector<aquarium::RamTarget> Targets;
	TArray<TWeakObjectPtr<AFishActor>> TargetActors;
	UPROPERTY() TObjectPtr<ACameraActor> Camera = nullptr;
	// 카메라 위치. DiverCamera 태그로 한 번만 찾는다(게임 모드와 같은 방식).
	FVector CameraLocation();
	void OnCaught(AFishActor* Fish);
	void OnBumped(AFishActor* Fish, AFishActor* Mine);
	void NoticeNearby(AFishActor* Mine, const aquarium::Vec2& MyScreen, const aquarium::Vec2& MyScreenVel);
};
```

`CatchSubsystem.cpp`의 `Tick` 본문(나머지 보조 함수는 위 선언 그대로 구현한다):

```cpp
void UCatchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ShakeValue.Step(DeltaTime);
	if (!bCatchEnabled || DeltaTime <= 0.f) return;

	UWorld* W = GetWorld();
	AAquariumGameMode* GM = W ? W->GetAuthGameMode<AAquariumGameMode>() : nullptr;
	AFishActor* Mine = GM ? GM->PlayerFish() : nullptr;
	UFishSchoolSubsystem* School = W ? W->GetSubsystem<UFishSchoolSubsystem>() : nullptr;
	if (Mine == nullptr || School == nullptr || Mine->IsPaused()) return;

	const FVector Cam = CameraLocation();
	const aquarium::Vec3 Camera{static_cast<float>(Cam.X), static_cast<float>(Cam.Y), static_cast<float>(Cam.Z)};

	Targets.clear();
	TargetActors.Reset();
	for (const TWeakObjectPtr<AFishActor>& Weak : School->RegisteredFish())
	{
		AFishActor* Fish = Weak.Get();
		// 내 물고기는 자기를 잡지 않는다. 이것이 없으면 첫 틱에 스스로를 찍는다.
		if (Fish == nullptr || Fish == Mine || Fish->bIsPlayerFish) continue;
		Targets.push_back(Fish->AsRamTarget());
		TargetActors.Add(Fish);
	}
	if (Targets.empty()) return;

	aquarium::Rammer Me;
	const aquarium::ClickTarget MyBody = Mine->AsClickTarget();
	Me.depth = MyBody.depth;
	Me.nose = Mine->NosePoint();
	Me.velocity = Mine->SwimVelocity();          // 공유 프레임 속도(아래에서 더한다)
	Me.maxSpeed = Mine->MaxSpeed;

	// 1) 먼저 눈치채게 한다. 판정보다 **앞**이라야 "놈이 먼저 눈치챘다"가 성립한다.
	NoticeNearby(Mine, aquarium::ToScreen(Me.nose, Me.depth, Camera),
	             aquarium::ToScreenVelocity(Me.velocity, Me.depth, Camera));

	// 2) 판정.
	const aquarium::RamResult R = aquarium::EvaluateRam(Camera, Me, Targets.data(), Targets.size(), CatchParamsValue);
	if (R.targetIndex < 0) return;
	AFishActor* Hit = TargetActors[R.targetIndex].Get();
	if (Hit == nullptr) return;
	if (R.outcome == aquarium::RamOutcome::Catch) { OnCaught(Hit); }
	else { OnBumped(Hit, Mine); }
}
```

`OnCaught`가 하는 일 넷: `Book.Stamp(FishId)`가 **처음일 때만** 숫자를 올리고, `Hit->ApplyStamp(...)`(Task 10), 「쿵」 재생, `ShakeValue.Hit(ImpactParamsValue)`. 그리고 `Book.ConsumeJustCompleted(CatchableCount())`가 참이면 Task 13의 "바다가 조용히 달라진다"를 발화한다. `OnBumped`는 `Hit->ApplyFleeFrom(Mine->GetActorLocation())`과 `EAquariumCue::Startle` 재생만 한다 — **벌은 없다.**

`AFishActor`에 두 개를 더한다(`SwimVelocity`는 이미 있는 `CurrentSpeed`의 형제다):

```cpp
	// 공유 유영 프레임에서의 속도. CurrentSpeed()는 크기만 주므로 방향이 필요하다.
	aquarium::Vec2 SwimVelocity() const { return Motion.velocity; }
```

물고기의 **동일성 키**는 `int32 FishId()`가 필요하다. `Seed`는 배경 물고기마다 다르게 배정되므로 그것을 쓴다:

```cpp
	// 장부의 열쇠. Seed는 build_reef_m1.py가 물고기마다 다르게 준다.
	int32 FishId() const { return static_cast<int32>(Seed); }
```

> **전제 확인:** `Seed`가 정말로 36마리 전부 서로 다른지 확인한다. 같은 값이 둘 있으면 **한 마리를 잡았는데 둘이 찍히거나** 완주가 영원히 안 된다.
> `grep -n "seed" unreal/Aquarium/Scripts/build_reef_m1.py | head -10`
> 중복이 있으면 `FishId()`를 `GetUniqueID()`나 액터 이름 해시로 바꾸고 후속 항목에 적는다.

`AAquariumGameMode::EndSession()` 끝에서 `UCatchSubsystem::ResetSession()`을 부른다.

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `88`.

- [ ] **Step 4: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `Tick`의 `Fish == Mine \|\| Fish->bIsPlayerFish` 검사를 **삭제** | `Aquarium.Catch.NeverStampsMyOwnFish` |
| B | `OnBumped`를 `OnCaught`로 **대체**(느려도 잡힌다) | `Aquarium.Catch.SlowDriftDoesNotStamp` |
| C | `Book.Stamp`의 반환값 검사를 **무시**하고 항상 숫자를 올린다 | `Aquarium.Catch.CountsEachFishOnce` |
| D | `NoticeNearby` 호출을 판정 **뒤로 옮긴다** | `Aquarium.Catch.NoticesBeforeContact`(닿는 틱에만 눈치채게 되므로 회피가 늦는다) |
| E | `OnBumped`에서도 「쿵」을 울린다 | `Aquarium.Catch.PlaysThudOnlyOnACatch` |
| F | `ResetSession`에서 `Book.Reset()`을 **삭제** | `Aquarium.Catch.ResetsOnLeaving` |

변이 B가 이 마일스톤의 전제를 지키는 시험이다. **변이 D로 빨간불이 안 켜지면** 테스트가 회피를 실제로 관측하지 못하고 있다는 뜻이니 배치를 다시 본다. 전부 되돌린다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 부딪혀 잡기 — 판정은 조향이 아니라 관측이다

UCatchSubsystem이 틱 끝에서 한 번 훑는다. 규칙 순서 바깥이라 경계 보장에
손대지 않고, 액터 틱은 하나도 늘지 않는다. 먼저 눈치채게 하고 그다음에
판정한다 — 순서가 반대면 "놈이 먼저 눈치챘다"가 성립하지 않는다.

빗나감에는 벌이 없다. 느리게 겹치면 놈이 놀라 튀고, 그것이 다시 붙을 이유가
된다. 느려도 잡히게 만드는 변이와 내 물고기를 제외하지 않는 변이로 각각
빨간불을 확인했다.

Automation 81 → 88.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 10: 엔진 — 도장의 그림은 이름표다, Automation 88 → 92

**Files:** Modify `.../FishActor.h/.cpp`, Modify `.../NameTagWidget.h/.cpp`, Modify `.../NameTagComponent.h`, Create `.../Tests/StampVisualTests.cpp`

시나리오: "**기본은 이름표.** 사진은 선택지." 그리고 사진은 이 계획의 범위 밖이다.

**거의 공짜인 이유**: 어려운 부분(절대 위치·스케일 왜곡·매 틱 갱신)은 `NameTagComponent`와 `AFishActor`가 **이미 다 풀어 놨고 위젯 내용물과 무관하다.** `AttachNameTag`는 어느 물고기에나 붙는다.

**다만 내 이름표와 도장은 달라 보여야 한다.** 내 물고기 위의 것은 "나"이고, 잡힌 놈 위의 것은 "내가 잡았다"이다. 둘이 똑같으면 화면이 같은 글자로 뒤덮인다.

- [ ] **Step 1 (RED): `Tests/StampVisualTests.cpp`**

```cpp
#include "Misc/AutomationTest.h"

#include "FishActor.h"
#include "NameTagComponent.h"
#include "NameTagWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStampAttachesTheName, "Aquarium.Stamp.AttachesTheName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStampAttachesTheName::RunTest(const FString&)
{
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnBackgroundFishForTest(World);
	TestFalse(TEXT("not stamped"), Fish->IsStamped());
	UNameTagComponent* Tag = Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	TestNotNull(TEXT("tag"), Tag);
	TestTrue(TEXT("stamped"), Fish->IsStamped());
	TestEqual(TEXT("the child's name"), Tag->DisplayedName().ToString(), FString(TEXT("민지")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStampIsQuieterThanMyOwnTag, "Aquarium.Stamp.IsQuieterThanMyOwnTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStampIsQuieterThanMyOwnTag::RunTest(const FString&)
{
	// 36마리가 전부 같은 크기의 흰 글자를 이고 다니면 화면이 글자밭이 된다.
	// 도장은 내 이름표보다 작고 옅다.
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnBackgroundFishForTest(World);
	UNameTagComponent* Tag = Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	UNameTagWidget* Widget = Cast<UNameTagWidget>(Tag->GetUserWidgetObject());
	TestNotNull(TEXT("widget"), Widget);
	TestTrue(TEXT("stamp style is on"), Widget->IsStampStyle());
	TestTrue(TEXT("smaller than the owner tag"), Widget->FontSize() < UNameTagWidget::OwnerFontSize());
	TestTrue(TEXT("dimmer than the owner tag"), Widget->Opacity() < 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStampCannotBeRemovedOneByOne, "Aquarium.Stamp.CannotBeRemovedOneByOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStampCannotBeRemovedOneByOne::RunTest(const FString&)
{
	// 두 번 찍어도 하나다. 그리고 떼는 공개 함수는 존재하지 않는다 -- 있는 것은
	// 세션 전체를 되돌리는 ClearStampForSessionReset()뿐이다.
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnBackgroundFishForTest(World);
	Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	TestTrue(TEXT("still stamped"), Fish->IsStamped());
	Fish->ClearStampForSessionReset();
	TestFalse(TEXT("gone after leaving"), Fish->IsStamped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStampFollowsTheFish, "Aquarium.Stamp.FollowsTheFish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStampFollowsTheFish::RunTest(const FString&)
{
	// 도장이 제자리에 남으면 '박혔다'가 아니라 '떨어졌다'로 보인다.
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnBackgroundFishForTest(World);
	UNameTagComponent* Tag = Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	const FVector Before = Tag->GetComponentLocation();
	for (int32 i = 0; i < 120; ++i) { Fish->StepSwim(1.f / 60.f); }
	const FVector After = Tag->GetComponentLocation();
	TestTrue(TEXT("the mark moved with the fish"), !After.Equals(Before, 1.f));
	return true;
}
```

빌드하면 `ApplyStamp`/`IsStampStyle`/`FontSize`/`Opacity`/`OwnerFontSize`/`ClearStampForSessionReset`이 없어 **컴파일 에러**다. RED다.

- [ ] **Step 2 (GREEN): 위젯에 도장 모양을 더한다**

`NameTagWidget.h`:

```cpp
public:
	void SetName(const FText& InName);
	FText DisplayedName() const { return PendingName; }
	// 도장 모양으로 바꾼다: 더 작고 더 옅다. 36마리가 같은 크기의 흰 글자를 이고
	// 다니면 화면이 글자밭이 되고, 그러면 내 물고기가 어느 것인지도 안 보인다.
	void SetStampStyle(bool bInStamp);
	bool IsStampStyle() const { return bStampStyle; }
	int32 FontSize() const { return bStampStyle ? StampFontSize() : OwnerFontSize(); }
	float Opacity() const { return bStampStyle ? 0.62f : 1.f; }
	static int32 OwnerFontSize() { return 22; }
	static int32 StampFontSize() { return 14; }

private:
	bool bStampStyle = false;
```

`NameTagWidget.cpp`에서 `RebuildWidget`의 하드코딩된 `22`를 `OwnerFontSize()`로 바꾸고(숫자를 두 군데 두지 않는다), 아래를 더한다:

```cpp
void UNameTagWidget::SetStampStyle(bool bInStamp)
{
	bStampStyle = bInStamp;
	if (Label)
	{
		Label->SetFont(FUiFont::Get(FontSize()));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, Opacity())));
	}
}
```

`RebuildWidget` 끝(라벨 구성 직후)에도 `SetStampStyle(bStampStyle);`를 부른다 — 위젯이 나중에 만들어져도 모양이 적용되도록(`PendingName`이 같은 이유로 존재한다).

`FishActor.h` public:

```cpp
	// 잡힌 물고기에 아이의 표시를 박는다. **떼는 함수는 없다** -- 실수로 잃는 것은
	// 벌이 되기 때문이다. 되돌리는 길은 나가기(ClearStampForSessionReset) 하나뿐이다.
	UNameTagComponent* ApplyStamp(const FText& ChildName);
	// 세션 리셋 전용. 이름이 길고 못생긴 것은 의도다 -- 게임 코드에서 부르면 안 된다.
	void ClearStampForSessionReset();
```

`FishActor.cpp`:

```cpp
UNameTagComponent* AFishActor::ApplyStamp(const FText& ChildName)
{
	UNameTagComponent* Tag = AttachNameTag(ChildName);
	if (Tag)
	{
		if (UNameTagWidget* Widget = Cast<UNameTagWidget>(Tag->GetUserWidgetObject()))
		{
			Widget->SetStampStyle(true);
		}
		bStamped = true;
	}
	return Tag;
}

void AFishActor::ClearStampForSessionReset()
{
	bStamped = false;
	if (NameTag)
	{
		NameTag->DestroyComponent();
		NameTag = nullptr;
	}
}
```

`UCatchSubsystem::OnCaught`에서 아이 별명을 넘긴다: `Hit->ApplyStamp(FText::FromString(GM->CurrentNickname()))`. **별명은 여전히 로그에 찍히지 않는다** — `ApplyStamp`에도 `UE_LOG`가 한 줄도 없어야 한다. `ResetSession()`은 등록된 모든 물고기에 `ClearStampForSessionReset()`을 부른다.

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `92`.

- [ ] **Step 3: 별명이 새지 않는지 확인(M6의 규칙 유지)**

```bash
cd /Users/hans/dev/aquarium && grep -rn "UE_LOG" unreal/Aquarium/Source/Aquarium/FishActor.cpp unreal/Aquarium/Source/Aquarium/CatchSubsystem.cpp unreal/Aquarium/Source/Aquarium/NameTagWidget.cpp | grep -i "name\|nick\|stamp"
```
기대: **출력 없음.** 한 줄이라도 나오면 지운다.

- [ ] **Step 4: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `ApplyStamp`에서 `Widget->SetStampStyle(true)`를 **삭제** | `Aquarium.Stamp.IsQuieterThanMyOwnTag` |
| B | `StampFontSize()`를 `OwnerFontSize()`와 같은 값으로 **대체** | 같은 테스트의 크기 단언 |
| C | `ApplyStamp`에서 `bStamped = true`를 **삭제** | `Aquarium.Stamp.AttachesTheName` |
| D | `NameTagComponent`의 `SetUsingAbsoluteLocation(true)`를 **삭제**하고 매 틱 갱신을 끈다 | `Aquarium.Stamp.FollowsTheFish` |
| E | `SetStampStyle`의 `Label->SetFont(...)` 갱신을 **삭제**(상태 변수만 바뀌고 실제 글꼴은 그대로) | `Aquarium.Stamp.IsQuieterThanMyOwnTag` — **이 변이로 빨간불이 안 켜지면 테스트가 상태 변수만 보고 있는 것이다.** M7의 뮤트 버튼에서 정확히 이 일이 있었다. 실제 `Label`의 글꼴 크기를 읽는 단언을 더한다 |

전부 되돌린다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 도장 — 잡은 놈이 내 표시를 영구히 달고 다닌다

기본 그림은 이름표다. 사진은 선택지이고 이 계획의 범위 밖이다. 어려운 부분은
NameTagComponent가 이미 다 풀어 놓아서 거의 공짜였다.

도장은 내 이름표보다 작고 옅다. 36마리가 같은 크기의 흰 글자를 이고 다니면
화면이 글자밭이 되고 내 물고기가 어느 것인지도 안 보인다.

떼는 공개 함수가 없다. 되돌리는 길은 나가기 하나뿐이고 그 함수 이름은 일부러
길고 못생기게 두었다. 별명은 ApplyStamp 경로 어디에도 로그로 나가지 않는다.

Automation 88 → 92.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 11: 엔진 — 화면 구석의 숫자 하나, Automation 92 → 95

**Files:** Modify `.../HudWidget.h/.cpp`, Modify `.../DiverPlayerController.cpp`, Modify `.../Tests/MuteButtonTests.cpp`(또는 새 `Tests/HudCountTests.cpp`)

시나리오: "**화면 구석에 작게 하나.** 점수판·축하 화면·별 세 개 없음." 그리고 "쌓이기만 하는 숫자는 벌이 아니고, **10분과 40분을 가른다.**"

**결정: 숫자만 띄운다. `/36`도, 라벨도, 아이콘도 없다.** 분모를 띄우는 순간 그것은 목표 선언이 되고, 시나리오가 완주 보상에 대해 "선언하는 순간 유치해진다"고 못 박았다. 아이는 세다가 스스로 알아낸다.

- [ ] **Step 1 (RED): 새 파일 `Tests/HudCountTests.cpp`**

```cpp
#include "Misc/AutomationTest.h"

#include "Components/TextBlock.h"
#include "HudWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudShowsTheCount, "Aquarium.Hud.ShowsTheCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHudShowsTheCount::RunTest(const FString&)
{
	FAquariumTestWorld World;
	UHudWidget* Hud = CreateWidget<UHudWidget>(World.Get(), UHudWidget::StaticClass());
	Hud->TakeWidget();                       // RebuildWidget을 실제로 돌린다
	Hud->SetCatchCount(0);
	TestEqual(TEXT("starts at zero"), Hud->CountText(), FString(TEXT("0")));
	Hud->SetCatchCount(8);
	TestEqual(TEXT("shows eight"), Hud->CountText(), FString(TEXT("8")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudCountHasNoDenominatorOrLabel, "Aquarium.Hud.CountHasNoDenominatorOrLabel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHudCountHasNoDenominatorOrLabel::RunTest(const FString&)
{
	// "8 / 36"도 "잡은 수: 8"도 아니다. 분모는 목표 선언이고, 선언하는 순간
	// 유치해진다(시나리오: 완주 보상은 조용해야 한다).
	FAquariumTestWorld World;
	UHudWidget* Hud = CreateWidget<UHudWidget>(World.Get(), UHudWidget::StaticClass());
	Hud->TakeWidget();
	Hud->SetCatchCount(8);
	const FString Text = Hud->CountText();
	TestFalse(TEXT("no slash"), Text.Contains(TEXT("/")));
	TestFalse(TEXT("no words"), Text.Contains(TEXT("마리")));
	TestEqual(TEXT("digits only"), Text, FString(TEXT("8")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudCountDoesNotStealClicks, "Aquarium.Hud.CountDoesNotStealClicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHudCountDoesNotStealClicks::RunTest(const FString&)
{
	// 숫자가 클릭을 먹으면 그 자리의 물고기를 놀래킬 수 없다(F-09의 규칙).
	FAquariumTestWorld World;
	UHudWidget* Hud = CreateWidget<UHudWidget>(World.Get(), UHudWidget::StaticClass());
	Hud->TakeWidget();
	TestFalse(TEXT("the count is not hit-testable"), Hud->CountIsHitTestable());
	return true;
}
```

- [ ] **Step 2 (GREEN): HUD에 숫자를 더한다**

`HudWidget.h` public:

```cpp
	// 잡은 수. 오르기만 한다 -- 내리는 경로가 이 클래스에 없다.
	void SetCatchCount(int32 Count);
	FString CountText() const;
	bool CountIsHitTestable() const;

private:
	UPROPERTY() TObjectPtr<class UTextBlock> CountLabel = nullptr;
```

`HudWidget.cpp`의 `RebuildWidget` 안, 캔버스에 붙인다. **왼쪽 아래**다 — 나가기·뮤트가 오른쪽 위에 있고, 물고기가 가장 적게 지나가는 구석이다.

```cpp
		CountLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CatchCount"));
		CountLabel->SetFont(FUiFont::Get(30));
		CountLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.82f)));
		CountLabel->SetShadowOffset(FVector2D(1.f, 1.f));
		CountLabel->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		CountLabel->SetText(FText::FromString(TEXT("0")));
		// 글자가 클릭을 먹으면 그 자리의 물고기를 놀래킬 수 없다(F-09).
		CountLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* CountSlot = Root->AddChildToCanvas(CountLabel))
		{
			CountSlot->SetAnchors(FAnchors(0.f, 1.f));
			CountSlot->SetAlignment(FVector2D(0.f, 1.f));
			CountSlot->SetPosition(FVector2D(kEdgeMargin, -kEdgeMargin));
			CountSlot->SetAutoSize(true);
		}
```

```cpp
void UHudWidget::SetCatchCount(int32 Count)
{
	if (CountLabel)
	{
		CountLabel->SetText(FText::AsNumber(Count));
	}
}

FString UHudWidget::CountText() const
{
	return CountLabel ? CountLabel->GetText().ToString() : FString();
}

bool UHudWidget::CountIsHitTestable() const
{
	return CountLabel != nullptr && CountLabel->GetVisibility() == ESlateVisibility::Visible;
}
```

> **전제 주의:** `FText::AsNumber(8)`는 로캘에 따라 천 단위 구분자를 넣는다(1,234). 여기서는 최대 두 자리라 문제가 없지만, **테스트가 `"8"`을 기대하므로 로캘이 다른 기계에서 깨질 여지가 있다면** `FText::FromString(FString::FromInt(Count))`로 바꾼다. 실제로 어느 쪽이 통과하는지 보고 고른다.

`ADiverPlayerController::Tick`에서 매 틱 숫자를 밀어 넣는다:

```cpp
	if (Hud)
	{
		if (UCatchSubsystem* CatchSub = GetWorld()->GetSubsystem<UCatchSubsystem>())
		{
			Hud->SetCatchCount(CatchSub->StampCount());
		}
	}
```

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `95`.

- [ ] **Step 3: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `SetCatchCount`가 `"%d / 36"`을 쓰도록 **대체** | `Aquarium.Hud.CountHasNoDenominatorOrLabel` |
| B | `SetCatchCount` 본문을 **삭제** | `Aquarium.Hud.ShowsTheCount` |
| C | `ESlateVisibility::HitTestInvisible`을 `Visible`로 **대체** | `Aquarium.Hud.CountDoesNotStealClicks` |

전부 되돌린다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 화면 구석의 숫자 하나 — 분모도 라벨도 없다

"8 / 36"도 "잡은 수: 8"도 아니다. 분모는 목표 선언이고 선언하는 순간
유치해진다. 아이는 세다가 스스로 알아낸다.

왼쪽 아래 구석, 클릭을 먹지 않는 글자다(HitTestInvisible). 초판의 "점수 금지"는
잃는 점수를 말한 것이고, 쌓이기만 하는 숫자는 10분과 40분을 가른다.

분모를 붙이는 변이와 클릭을 먹게 만드는 변이로 각각 빨간불을 확인했다.

Automation 92 → 95.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 12: 연출 — 화면 흔들림·물 밀림·난류 자국, Automation 95 → 99

**Files:** Modify `.../DiverPlayerController.h/.cpp`, Modify `.../BubbleSubsystem.h/.cpp`, Create `unreal/Aquarium/Scripts/build_fx_m8.py`, Create `.../Tests/ImpactFxTests.cpp`

M7 장면 2가 **범위 밖으로 남긴 세 가지**다. 시나리오의 "먹히는 것" 목록 그대로다.

**결정 셋:**
1. **흔들림은 카메라 액터를 직접 움직인다.** `UCameraShakeBase`는 에셋이나 CDO 설정이 필요해 스크립트 재현성과 충돌하고, `UMatineeCameraShake`의 파라미터는 C++에서 인스턴스마다 설정하기 번거롭다. 카메라는 우리가 스폰한 평범한 `ACameraActor` 하나이므로 **기준 위치를 기억해 두고 오프셋을 더했다 뺀다.** 헤드리스에서도 검증된다.
2. **물 밀림은 카메라 컴포넌트의 포스트 프로세스 블렌더블이다.** 레벨에 `PostProcessVolume`을 넣지 않는다 — 넣으면 `ReefM1.umap`이 바뀌어 `verify_scene.py` 기대값이 흔들린다. 머티리얼은 `build_fx_m8.py`가 만든다.
3. **난류 자국은 기존 기포 서브시스템의 두 번째 종류다.** 새 서브시스템을 만들지 않는다. 빠르게 지나간 자리에 **하얗고 옆으로 늘어진** 흔적을 남기고, 위로 뜨지 않고 그 자리에서 사라진다(기포와 반대다 — 그래서 귀여운 방울로 안 보인다).

- [ ] **Step 1 (RED): `Tests/ImpactFxTests.cpp`**

```cpp
#include "Misc/AutomationTest.h"

#include "BubbleSubsystem.h"
#include "CatchSubsystem.h"
#include "Camera/CameraActor.h"
#include "DiverPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactShakesTheCamera, "Aquarium.Impact.ShakesTheCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImpactShakesTheCamera::RunTest(const FString&)
{
	FAquariumTestWorld World;
	ADiverPlayerController* PC = SpawnControllerForTest(World);
	ACameraActor* Cam = FindDiverCameraForTest(World);
	TestNotNull(TEXT("camera"), Cam);
	const FVector Base = Cam->GetActorLocation();
	UCatchSubsystem* CatchSub = World.Get()->GetSubsystem<UCatchSubsystem>();
	CatchSub->TriggerShakeForTest();
	PC->TickCameraShakeForTest(1.f / 120.f);
	TestTrue(TEXT("camera moved"), !Cam->GetActorLocation().Equals(Base, 0.05f));
	// 그리고 짧다. 끝나면 **정확히** 제자리다 -- 누적 오차가 남으면 카메라가
	// 한 판 내내 조금씩 흘러간다(P-06: 카메라는 절대 물고기를 따라가지 않는다).
	for (int32 i = 0; i < 200; ++i) { PC->TickCameraShakeForTest(1.f / 120.f); }
	TestTrue(TEXT("exactly back"), Cam->GetActorLocation().Equals(Base, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactShakeIsShort, "Aquarium.Impact.ShakeIsShort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImpactShakeIsShort::RunTest(const FString&)
{
	// 0.35초를 넘으면 '흔들림'이 아니라 '지진'이고, 연타하는 아이가 멀미한다.
	FAquariumTestWorld World;
	UCatchSubsystem* CatchSub = World.Get()->GetSubsystem<UCatchSubsystem>();
	TestTrue(TEXT("short"), CatchSub->ShakeParams().duration <= 0.35f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWakeLeavesATrailWhenFast, "Aquarium.Impact.WakeLeavesATrailWhenFast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWakeLeavesATrailWhenFast::RunTest(const FString&)
{
	FAquariumTestWorld World;
	UBubbleSubsystem* Bubbles = World.Get()->GetSubsystem<UBubbleSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	Mine->SetInputDirection(FVector2D(0.f, 0.f));
	for (int32 i = 0; i < 120; ++i) { Mine->StepSwim(1.f / 60.f); Bubbles->Tick(1.f / 60.f); }
	TestEqual(TEXT("drifting leaves nothing"), Bubbles->WakeSpawnedTotal(), 0);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 120; ++i) { Mine->StepSwim(1.f / 60.f); Bubbles->Tick(1.f / 60.f); }
	TestTrue(TEXT("running leaves a trail"), Bubbles->WakeSpawnedTotal() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWakeDoesNotRise, "Aquarium.Impact.WakeDoesNotRise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWakeDoesNotRise::RunTest(const FString&)
{
	// 난류 자국은 기포가 아니다. 위로 뜨면 그것은 '귀여운 방울'이고, 시나리오가
	// 이름 붙여 금지한 것이다.
	FAquariumTestWorld World;
	UBubbleSubsystem* Bubbles = World.Get()->GetSubsystem<UBubbleSubsystem>();
	Bubbles->SpawnWake(FVector(300.f, 0.f, 100.f), FVector2D(1.f, 0.f), 1u);
	const float Z0 = Bubbles->HighestWakeZ();
	for (int32 i = 0; i < 30; ++i) { Bubbles->Tick(1.f / 60.f); }
	TestTrue(TEXT("did not rise"), Bubbles->HighestWakeZ() <= Z0 + 0.01f);
	// 그리고 제 수명으로 사라진다(화면 위까지 갈 일이 없으므로 기포의 규칙을 쓰지 않는다).
	for (int32 i = 0; i < 600; ++i) { Bubbles->Tick(1.f / 60.f); }
	TestEqual(TEXT("gone"), Bubbles->ActiveWakeCount(), 0);
	return true;
}
```

- [ ] **Step 2 (GREEN): 카메라 흔들림**

`DiverPlayerController.h` private에:

```cpp
	// 흔들리기 전의 카메라 자리. 오프셋을 **더했다 빼는** 것이 아니라 언제나
	// 기준 + 오프셋으로 다시 놓는다. 누적 오차가 남으면 카메라가 한 판 내내
	// 조금씩 흘러가고, 그것은 P-06(카메라는 물고기를 따라가지 않는다) 위반이다.
	FVector CameraBaseLocation = FVector::ZeroVector;
	bool bHasCameraBase = false;
	UPROPERTY() TObjectPtr<class ACameraActor> ShakeCamera = nullptr;
public:
	// 테스트 진입점. Tick에서 부르는 것과 같은 함수다.
	void TickCameraShakeForTest(float DeltaSeconds) { ApplyCameraShake(DeltaSeconds); }
private:
	void ApplyCameraShake(float DeltaSeconds);
```

`DiverPlayerController.cpp`:

```cpp
void ADiverPlayerController::ApplyCameraShake(float DeltaSeconds)
{
	UWorld* W = GetWorld();
	UCatchSubsystem* CatchSub = W ? W->GetSubsystem<UCatchSubsystem>() : nullptr;
	if (CatchSub == nullptr || ShakeCamera == nullptr) return;
	if (!bHasCameraBase)
	{
		CameraBaseLocation = ShakeCamera->GetActorLocation();
		bHasCameraBase = true;
	}
	const aquarium::ShakeOffset O = CatchSub->Shake().Offset(CatchSub->ShakeParams());
	ShakeCamera->SetActorLocation(CameraBaseLocation + FVector(0.f, O.y, O.z));

	// 물 밀림 왜곡. 같은 수명을 따르되 진동하지 않는다.
	if (UCameraComponent* Cc = ShakeCamera->GetCameraComponent())
	{
		if (DisplacementMid)
		{
			const float Weight = CatchSub->Shake().DisplacementWeight(CatchSub->ShakeParams());
			DisplacementMid->SetScalarParameterValue(TEXT("Strength"), Weight);
			// 블렌더블은 한 번만 넣는다. 매 틱 AddBlendable을 부르면 배열이 자란다.
		}
	}
}
```

`BeginPlay`에서 `ShakeCamera`를 `DiverCamera` 태그로 찾고(기존 코드가 이미 같은 질의를 한다 — **그 결과를 재사용한다**), 왜곡 머티리얼을 만들어 한 번만 붙인다:

```cpp
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fx/M_WaterPush.M_WaterPush")))
	{
		DisplacementMid = UMaterialInstanceDynamic::Create(Base, this);
		DisplacementMid->SetScalarParameterValue(TEXT("Strength"), 0.f);
		if (UCameraComponent* Cc = ShakeCamera ? ShakeCamera->GetCameraComponent() : nullptr)
		{
			Cc->PostProcessSettings.AddBlendable(DisplacementMid, 1.f);
			Cc->PostProcessBlendWeight = 1.f;
		}
	}
```

그리고 `Tick` 끝에서 `ApplyCameraShake(DeltaSeconds);`를 부른다.

- [ ] **Step 3 (GREEN): 난류 자국**

`BubbleSubsystem`에 두 번째 배열을 더한다. **기포와 같은 구조체를 쓰되 위로 뜨지 않고 제 수명으로 사라진다** — 그 차이가 "난류의 흔적"과 "귀여운 방울"을 가른다.

`BubbleSubsystem.h`:

```cpp
	// 빠르게 지나간 자리에 남는 하얀 자국. **위로 뜨지 않는다.** 기포와 반대인 것이
	// 요점이다 -- 뜨면 그것은 시나리오가 이름 붙여 금지한 '귀여운 방울'이 된다.
	void SpawnWake(const FVector& WorldPoint, const FVector2D& HeadingShared, uint32 Seed);
	int32 ActiveWakeCount() const { return static_cast<int32>(Wakes.size()); }
	int32 WakeSpawnedTotal() const { return WakeSpawnedTotalValue; }
	float HighestWakeZ() const;
	// 내 물고기가 이 속도 비율을 넘으면 자국을 남긴다. 최대 속도에서 **파생**한다.
	float WakeSpeedFraction = 0.55f;
```

`Tick`에서: 플레이어 물고기 속도가 `MaxSpeed * WakeSpeedFraction`을 넘으면 `WakeInterval`(0.06초)마다 한 점을 남긴다. 각 자국은 `lifetime`(0.5초) 동안 투명해지며 **제자리에서** 사라진다.

`build_fx_m8.py`가 만드는 머티리얼 둘:
- `M_Wake` — 흰색 반투명 `Unlit`. 알파가 시간이 아니라 **인스턴스 커스텀 데이터**로 온다(`PerInstanceCustomData`). 가늘고 길게 늘어난 형태는 **인스턴스 스케일**로 만든다(진행 방향으로 3배, 수직으로 0.4배).
- `M_WaterPush` — 포스트 프로세스 머티리얼(`material_domain = MD_POST_PROCESS`). `SceneTexture:PostProcessInput0`의 UV를 화면 중심에서 바깥으로 `Strength`만큼 민다. **`Strength` 스칼라 파라미터 이름이 C++과 정확히 같아야 한다**(틀리면 조용히 아무 일도 일어나지 않는다).

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/build_fx_m8.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "FX8_OK|Traceback"
```
기대: `FX8_OK count=2 materials=[M_Wake M_WaterPush] ...`.

> **전제 확인:** 포스트 프로세스 머티리얼의 도메인 열거값과 `MaterialExpressionSceneTexture`의 파이썬 이름을 먼저 확인한다. **계획이 지어낸 이름일 수 있다**(M7에서 `b_auto_create_cue`가 정확히 그랬다):
> `"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" ... -run=pythonscript` 로 `print([x for x in dir(unreal) if "SceneTexture" in x]); print(list(unreal.MaterialDomain.__members__ if hasattr(unreal.MaterialDomain,"__members__") else dir(unreal.MaterialDomain)))`를 찍어 **실제 이름을 확인한 뒤** 스크립트를 쓴다.

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `99`.

- [ ] **Step 4: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `ApplyCameraShake`의 `SetActorLocation`을 `AddActorWorldOffset`으로 **대체**(누적) | `Aquarium.Impact.ShakesTheCamera`의 "exactly back" |
| B | `ApplyCameraShake` 본문을 **삭제** | 같은 테스트의 "camera moved" |
| C | `ImpactParams::duration`을 `1.2f`로 **대체** | `Aquarium.Impact.ShakeIsShort` |
| D | 자국의 위치 갱신에 `riseSpeed`를 더한다(위로 뜨게) | `Aquarium.Impact.WakeDoesNotRise` |
| E | 속도 문턱 검사를 **삭제**(항상 자국) | `Aquarium.Impact.WakeLeavesATrailWhenFast`의 "drifting leaves nothing" |

변이 A가 P-06(카메라 고정)을 지키는 시험이다. 전부 되돌린다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 타격감 — 짧은 흔들림, 밀리는 물, 지나간 자리의 하얀 자국

M7 장면 2가 범위 밖으로 남긴 셋을 갚았다. 흔들림은 카메라 액터를 기준 위치
+ 오프셋으로 다시 놓는다 — 더했다 빼면 누적 오차가 남아 카메라가 한 판 내내
흘러가고 그것은 P-06 위반이다. 누적으로 되돌리는 변이로 빨간불을 확인했다.

물 밀림은 카메라 컴포넌트의 블렌더블이라 레벨이 한 바이트도 바뀌지 않는다.
난류 자국은 기포와 달리 위로 뜨지 않고 제자리에서 사라진다 — 뜨는 순간
그것은 시나리오가 이름 붙여 금지한 '귀여운 방울'이 된다.

Automation 95 → 99.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 13: 36마리를 다 찍으면 — 바다가 조용히 달라진다, Automation 99 → 102

**Files:** Modify `.../CatchSubsystem.h/.cpp`, Modify `.../BubbleSubsystem.h/.cpp`, Create `.../Tests/SeaChangeTests.cpp`

시나리오: "**바다가 조용히 달라지는 정도. 축하 화면 금지.**" 그리고 "선언하는 순간 유치해진다."

**결정: 바다 전체가 천천히 숨을 쉬기 시작한다 — 아무 데서나 기포가 느리게 올라온다.**
- 이미 가진 것(`UBubbleSubsystem`)만 쓴다. 새 에셋도, 새 서브시스템도, 레벨 변경도 없다.
- 글자도 소리 신호도 없다. **아이는 화면을 보다가 "어? 뭔가 달라졌는데"를 스스로 알아챈다.**
- 축하가 아니라 **상태 변화**다. 되돌아가지 않고, 나가기 전까지 계속된다.

- [ ] **Step 1 (RED): `Tests/SeaChangeTests.cpp`**

```cpp
#include "Misc/AutomationTest.h"

#include "BubbleSubsystem.h"
#include "CatchSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaIsQuietBeforeCompletion, "Aquarium.SeaChange.QuietBeforeCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSeaIsQuietBeforeCompletion::RunTest(const FString&)
{
	FAquariumTestWorld World;
	UBubbleSubsystem* Bubbles = World.Get()->GetSubsystem<UBubbleSubsystem>();
	UCatchSubsystem* CatchSub = World.Get()->GetSubsystem<UCatchSubsystem>();
	CatchSub->StampAllButOneForTest();
	const int32 Before = Bubbles->SpawnedTotal();
	for (int32 i = 0; i < 300; ++i) { Bubbles->Tick(1.f / 60.f); CatchSub->Tick(1.f / 60.f); }
	TestEqual(TEXT("no ambient bubbles yet"), Bubbles->SpawnedTotal(), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaBreathesAfterCompletion, "Aquarium.SeaChange.BreathesAfterCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSeaBreathesAfterCompletion::RunTest(const FString&)
{
	FAquariumTestWorld World;
	UBubbleSubsystem* Bubbles = World.Get()->GetSubsystem<UBubbleSubsystem>();
	UCatchSubsystem* CatchSub = World.Get()->GetSubsystem<UCatchSubsystem>();
	CatchSub->StampAllForTest();
	const int32 Before = Bubbles->SpawnedTotal();
	for (int32 i = 0; i < 300; ++i) { Bubbles->Tick(1.f / 60.f); CatchSub->Tick(1.f / 60.f); }
	TestTrue(TEXT("the sea started breathing"), Bubbles->SpawnedTotal() > Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompletionSaysNothing, "Aquarium.SeaChange.SaysNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCompletionSaysNothing::RunTest(const FString&)
{
	// 축하 화면도, 별 세 개도, 소리 신호도 없다. 완주는 조용해야 한다.
	FAquariumTestWorld World;
	UCatchSubsystem* CatchSub = World.Get()->GetSubsystem<UCatchSubsystem>();
	UAquariumAudioSubsystem* Audio = World.Get()->GetSubsystem<UAquariumAudioSubsystem>();
	ADiverPlayerController* PC = SpawnControllerForTest(World);
	const int32 ThudBefore = Audio->CuePlayCount(EAquariumCue::Thud);
	CatchSub->StampAllForTest();
	for (int32 i = 0; i < 300; ++i) { CatchSub->Tick(1.f / 60.f); }
	TestEqual(TEXT("no fanfare"), Audio->CuePlayCount(EAquariumCue::Thud), ThudBefore);
	// HUD에는 여전히 숫자 하나뿐이다.
	TestEqual(TEXT("still just a number"), PC->HudForTest()->CountText(), FString::FromInt(CatchSub->StampCount()));
	return true;
}
```

- [ ] **Step 2 (GREEN)**

`UBubbleSubsystem`에:

```cpp
	// 완주 뒤의 바다. 아무 데서나 느리게 기포가 올라온다. 축하가 아니라 상태 변화다.
	void SetSeaBreathing(bool bInBreathing) { bSeaBreathing = bInBreathing; }
	bool IsSeaBreathing() const { return bSeaBreathing; }
private:
	bool bSeaBreathing = false;
	float BreathTimer = 0.f;
	float BreathInterval = 0.8f;   // 초. 이보다 잦으면 '달라졌다'가 아니라 '고장났다'다
```

`Tick`에서 `bSeaBreathing`이면 `BreathInterval`마다 무작위 평면·무작위 위치에 기포 **한두 개**를 띄운다. 소멸 높이는 기존대로 그 깊이에서 파생한다(`ScreenTopZAt`).

`UCatchSubsystem::OnCaught` 끝에:

```cpp
	if (Book.ConsumeJustCompleted(CatchableCount()))
	{
		// 축하하지 않는다. 바다가 달라질 뿐이다.
		if (UBubbleSubsystem* Bubbles = GetWorld()->GetSubsystem<UBubbleSubsystem>())
		{
			Bubbles->SetSeaBreathing(true);
		}
	}
```

`ResetSession()`에서 `SetSeaBreathing(false)`도 함께 되돌린다.

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `102`.

- [ ] **Step 3: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `SetSeaBreathing(true)` 호출을 **삭제** | `Aquarium.SeaChange.BreathesAfterCompletion` |
| B | `ConsumeJustCompleted`를 `IsComplete`로 **대체**하고 조건 없이 매 틱 켠다 | 걸리지 않을 수 있다 — 그 경우 "완주 전에는 조용하다"를 보는 A안(`StampAllButOneForTest`)이 빨간불이 되는지 확인한다 |
| C | 완주 시 `PlayCue(EAquariumCue::Thud, ...)`를 **추가**(축하 소리) | `Aquarium.SeaChange.SaysNothing` |
| D | `CatchableCount()`를 `36`으로 **대체**하고 테스트 월드의 물고기 수를 3으로 둔다 | `Aquarium.SeaChange.BreathesAfterCompletion`(영원히 완주 안 됨) |

전부 되돌린다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 다 찍으면 바다가 조용히 달라진다 — 선언은 없다

완주 뒤에는 아무 데서나 느리게 기포가 올라온다. 글자도, 축하 화면도, 소리
신호도 없다. 아이는 화면을 보다가 스스로 알아챈다.

이미 가진 기포 서브시스템만 쓴다 — 새 에셋도 레벨 변경도 없다. 완주 수는
실제 물고기 수에서 파생하므로 36을 리터럴로 쓰지 않는다. 축하 소리를 넣는
변이로 "조용하다"가 빨간불이 되는 것을 확인했다.

Automation 99 → 102.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 14: 먹이 = 미끼, 규칙 179 → 189, Automation 102 → 108

**Files:** Create `rules/include/aquarium/Food.h`, Create `tests/test_food.cpp`, Modify `CMakeLists.txt`, Create `.../FoodSubsystem.h/.cpp`, Modify `.../FishActor.h/.cpp`, Modify `.../DiverPlayerController.cpp`, Create `.../Tests/FoodTests.cpp`, Modify `unreal/Aquarium/Scripts/build_fx_m7.py` 실행

**먹이는 M7에서 계획됐지만 아직 구현되지 않았다.** `rules/include/aquarium/Food.h`는 존재하지 않는다(Task 0에서 확인된다). 그리고 **의미가 바뀌었다**: "몰려오는 걸 구경한다"가 아니라 **"모아 놓고 친다"**이다. 좋은 자리에 뿌리는 것과 아무 데나 뿌리는 것이 달라져 **실력이 갈린다.**

- [ ] **Step 1: 전제 확인 — 먹이가 정말 아직 없는가**

```bash
cd /Users/hans/dev/aquarium && ls rules/include/aquarium/Food.h unreal/Aquarium/Source/Aquarium/FoodSubsystem.h 2>&1 | head -2
```
기대: 둘 다 `No such file`. **하나라도 있으면** 이미 구현된 것이므로 그 상태를 읽고 이 태스크를 델타로 줄인다.

- [ ] **Step 2: M7 계획의 Task 11과 Task 12를 그대로 실행한다**

`docs/superpowers/plans/2026-09-21-m7-fun-first.md`의 **Task 11(규칙 계층 `Food.h`, 3251행부터)과 Task 12(장면 3, 3521행부터)**에 완전한 코드·테스트·변이표·커밋이 들어 있고, **개정된 시나리오가 그 구현을 한 줄도 바꾸지 않았다**(바뀐 것은 먹이의 *의미*이지 동작이 아니다). 그것을 **그대로** 실행한다 — 여기 옮겨 적으면 같은 규칙이 두 문서에 생겨 규약 8을 어긴다.

두 가지만 M8의 판단으로 바꾼다:
1. **커밋 메시지의 `feat(m7)`을 `feat(m8)`로** 바꾸고 첫 줄을 "먹이 = 미끼"로 쓴다.
2. **기대 테스트 수**는 M7 계획의 숫자(124 → 134)가 아니라 **지금의 실제 숫자**를 쓴다(179 → 189, Automation 102 → 108). 실제로 찍힌 값과 다르면 멈추고 보고한다(규약 9).

```bash
cd /Users/hans/dev/aquarium && sed -n '3251,3990p' docs/superpowers/plans/2026-09-21-m7-fun-first.md
```

- [ ] **Step 3: 미끼의 델타 — 뭉친 무리를 들이받으면 한 번에 여러 마리가 잡힌다**

이것이 M7에 없던 부분이고, "구경한다"를 "모아 놓고 친다"로 바꾸는 **유일한 코드**다.

`UCatchSubsystem::Tick`의 판정 부분을 고친다. 지금은 앞엣놈 하나만 본다. **겹쳐 있는 놈들은 전부 판정한다**:

```cpp
	// 먹이로 뭉쳐 놓은 무리를 들이받으면 한 번에 여럿이 잡힌다. 이것이 먹이를
	// "구경거리"에서 "미끼"로 바꾸는 한 곳이다 -- 좋은 자리에 뿌린 아이와 아무 데나
	// 뿌린 아이의 결과가 여기서 갈린다.
	//
	// 앞엣놈만 잡던 것을 겹친 놈 전부로 넓힌다. 소리는 **한 번만** 운다(두 마리를
	// 잡았다고 「쿵」이 두 번 겹치면 그것은 타격이 아니라 잡음이다).
	std::vector<aquarium::RamTarget> Remaining = Targets;
	int32 CaughtThisTick = 0;
	for (int32 Pass = 0; Pass < static_cast<int32>(Targets.size()); ++Pass)
	{
		const aquarium::RamResult R = aquarium::EvaluateRam(Camera, Me, Remaining.data(), Remaining.size(), CatchParamsValue);
		if (R.targetIndex < 0) break;
		AFishActor* Hit = TargetActors[R.targetIndex].Get();
		// 이번 패스에서 다시 뽑히지 않도록 크기를 0으로 만든다(EvaluateRam이 건너뛴다).
		Remaining[R.targetIndex].halfWidth = 0.f;
		if (Hit == nullptr) continue;
		if (R.outcome == aquarium::RamOutcome::Catch) { OnCaught(Hit, /*bPlaySound*/ CaughtThisTick == 0); ++CaughtThisTick; }
		else { OnBumped(Hit, Mine); }
	}
```

`OnCaught`에 `bool bPlaySound` 인자를 더하고 「쿵」 재생을 그 조건 아래로 옮긴다. **흔들림은 첫 한 번만** 건다(`ImpactShake::Hit`는 덮어쓰므로 여러 번 불러도 안전하지만, 의도를 코드로 적어 둔다).

- [ ] **Step 4 (RED → GREEN): 미끼 테스트**

`Tests/CatchTests.cpp`에 더한다.

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchTakesTheWholeCluster, "Aquarium.Catch.TakesTheWholeCluster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchTakesTheWholeCluster::RunTest(const FString&)
{
	// 먹이로 모아 놓은 무리를 치면 한 번에 여럿이다. 좋은 자리에 뿌린 보람이 여기 있다.
	FAquariumTestWorld World;
	UCatchSubsystem* CatchSub = World.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	AFishActor* A = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	AFishActor* B = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	AFishActor* C = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	for (AFishActor* F : {A, B, C}) { CatchSub->SetTargetUnderNoseForTest(F, Mine); }
	CatchSub->Tick(1.f / 60.f);
	TestEqual(TEXT("all three"), CatchSub->StampCount(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchThudsOnceForACluster, "Aquarium.Catch.ThudsOnceForACluster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchThudsOnceForACluster::RunTest(const FString&)
{
	// 「쿵」이 세 번 겹치면 타격이 아니라 잡음이다.
	FAquariumTestWorld World;
	UCatchSubsystem* CatchSub = World.Get()->GetSubsystem<UCatchSubsystem>();
	UAquariumAudioSubsystem* Audio = World.Get()->GetSubsystem<UAquariumAudioSubsystem>();
	AFishActor* Mine = World.GameMode()->PlayerFish();
	AFishActor* A = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	AFishActor* B = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 240; ++i) { Mine->StepSwim(1.f / 60.f); }
	CatchSub->SetTargetUnderNoseForTest(A, Mine);
	CatchSub->SetTargetUnderNoseForTest(B, Mine);
	const int32 Before = Audio->CuePlayCount(EAquariumCue::Thud);
	CatchSub->Tick(1.f / 60.f);
	TestEqual(TEXT("two caught"), CatchSub->StampCount(), 2);
	TestEqual(TEXT("one thud"), Audio->CuePlayCount(EAquariumCue::Thud), Before + 1);
	return true;
}
```

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `108`.

- [ ] **Step 5: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | 반복 패스를 **삭제**하고 앞엣놈 하나만 판정 | `Aquarium.Catch.TakesTheWholeCluster` |
| B | `bPlaySound` 인자를 **무시**하고 매번 울린다 | `Aquarium.Catch.ThudsOnceForACluster` |
| C | `Remaining[...].halfWidth = 0.f;`를 **삭제** | `TakesTheWholeCluster`가 무한 루프 대신 같은 놈을 반복 판정한다 — 테스트가 **멈추지 않으면** 그것도 빨간불이다 |
| D | M7 Task 11의 변이표(깊이 게이트·사정거리·상한)를 그대로 수행 | 그 표의 테스트들 |

전부 되돌린다.

- [ ] **Step 6: 커밋** (M7 Task 11·12의 커밋 두 개 + 이 델타 커밋 하나, 총 세 개)

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 먹이 = 미끼 — 모아 놓고 치면 한 번에 여럿이다

먹이의 동작은 M7 계획 Task 11·12 그대로이고 바뀐 것은 의미다. 그 의미를
코드로 만드는 곳은 여기 한 군데다: 겹쳐 있는 놈을 전부 판정한다. 좋은 자리에
뿌린 아이와 아무 데나 뿌린 아이의 결과가 이 루프에서 갈린다.

「쿵」은 한 번만 운다 — 세 번 겹치면 타격이 아니라 잡음이다.

규칙 179 → 189, Automation 102 → 108.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -3
```

---

### Task 15: 무리 갈라짐을 속도에 비례시킨다 (`Split.h`), 규칙 189 → 197, Automation 108 → 111

**Files:** Create `rules/include/aquarium/Split.h`, Create `tests/test_split.cpp`, Modify `CMakeLists.txt`, Modify `.../FishActor.cpp`, Modify `.../Tests/FishActorTests.cpp`

시나리오 개정: "**속도 계기판** — 속도에 비례해 갈라진다. 일률적으로 세게 갈라지면 그냥 연출이다. **비례해야 속도를 다루는 재미가 생긴다.**"

배경 물고기는 이미 내 물고기를 `avoidOnly` 이웃으로 피한다(`AsNeighbor()`). **없는 것은 비례뿐이다.**

- [ ] **Step 1 (RED): `tests/test_split.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "aquarium/Split.h"

using namespace aquarium;
using Catch::Approx;

namespace { const SplitParams kS{}; }

TEST_CASE("standing still parts nothing") {
    REQUIRE(SplitStrength(0.f, 90.f, kS) == Approx(0.f));
}

TEST_CASE("full speed parts hardest") {
    REQUIRE(SplitStrength(90.f, 90.f, kS) == Approx(kS.maxStrength));
}

TEST_CASE("it is PROPORTIONAL, not a switch") {
    // 문턱 하나로 켜고 끄면 그것은 그냥 연출이고, 속도를 다루는 재미가 생기지 않는다.
    const float a = SplitStrength(20.f, 90.f, kS);
    const float b = SplitStrength(45.f, 90.f, kS);
    const float c = SplitStrength(70.f, 90.f, kS);
    REQUIRE(a > 0.f);
    REQUIRE(b > a);
    REQUIRE(c > b);
    // 중간이 양 끝의 중간쯤이다(계단이 아니다).
    REQUIRE(b == Approx(0.5f * (SplitStrength(0.f, 90.f, kS) + SplitStrength(90.f, 90.f, kS))).margin(kS.maxStrength * 0.15f));
}

TEST_CASE("the reference speed is the player's own max, not a literal") {
    // 최대 속도를 조정하면 계기판도 같이 따라와야 한다(규약 8).
    REQUIRE(SplitStrength(45.f, 90.f, kS) == Approx(SplitStrength(90.f, 180.f, kS)));
}

TEST_CASE("going faster than max does not part harder than max") {
    REQUIRE(SplitStrength(400.f, 90.f, kS) == Approx(kS.maxStrength));
}

TEST_CASE("a zero reference speed is not a divide by zero") {
    REQUIRE(SplitStrength(40.f, 0.f, kS) == Approx(0.f));
}

TEST_CASE("the parting radius is DERIVED from the avoid radius, not copied") {
    // 두 군데에 적으면 한쪽만 고쳐지는 날 검증이 영원히 못 잡는다.
    REQUIRE(SplitRadius(60.f, 1.f, kS) > 60.f);
    REQUIRE(SplitRadius(60.f, 0.f, kS) == Approx(60.f));
    REQUIRE(SplitRadius(120.f, 1.f, kS) == Approx(2.f * SplitRadius(60.f, 1.f, kS)));
}

TEST_CASE("negative speed is treated as standing still, not as reverse parting") {
    REQUIRE(SplitStrength(-50.f, 90.f, kS) == Approx(0.f));
}
```

`CMakeLists.txt`에 `tests/test_split.cpp`를 더한다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_split.cpp && cmake -S . -B build && cmake --build build -j 2>&1 | tail -4
```
기대: `'aquarium/Split.h' file not found`. RED다.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Split.h`**

```cpp
#pragma once

namespace aquarium {

// 시나리오 개정: 무리 갈라짐은 **속도 계기판**이다. 일률적으로 세게 갈라지면
// 그냥 연출이고, 비례해야 속도를 다루는 재미가 생긴다.
//
// 배경 물고기가 내 물고기를 피하는 일 자체는 이미 Boids.h의 avoidOnly가 한다.
// 없던 것은 **비례**뿐이라 이 헤더에는 두 함수밖에 없다.
struct SplitParams {
    float maxStrength = 1.0f;   // 최대 속도에서의 밀어내는 세기 배수
    float radiusGain = 0.8f;    // 최대 속도에서 회피 반경이 이만큼 더 커진다
};

inline float SplitFraction(float speed, float referenceSpeed) {
    if (referenceSpeed <= 1e-6f || speed <= 0.f) return 0.f;
    const float f = speed / referenceSpeed;
    return f > 1.f ? 1.f : f;
}

// 속도에 **선형**으로 비례한다. 문턱으로 켜고 끄지 않는다 -- 그러면 아이는
// 자기 속도가 아니라 스위치를 다루게 된다.
inline float SplitStrength(float speed, float referenceSpeed, const SplitParams& p) {
    return p.maxStrength * SplitFraction(speed, referenceSpeed);
}

// 회피 반경. BoidsParams::avoidOnlyRadius에서 **파생**한다 -- 숫자를 베끼지 않는다.
inline float SplitRadius(float avoidOnlyRadius, float fraction, const SplitParams& p) {
    const float f = fraction < 0.f ? 0.f : (fraction > 1.f ? 1.f : fraction);
    return avoidOnlyRadius * (1.f + p.radiusGain * f);
}

} // namespace aquarium
```

- [ ] **Step 3 (GREEN): 엔진에 연결한다**

`FishActor.cpp`의 무리 블록에서, 이웃을 만들기 **전에** 플레이어의 회피 반경과 세기를 속도에서 파생해 `BoidsParamsValue`에 넣는다.

```cpp
			// 내 물고기가 지나갈 때 갈라지는 정도는 **내 속도에 비례**한다.
			// 참조 속도는 그 물고기의 MaxSpeed에서 파생하므로 숫자가 두 군데에 없다.
			if (AFishActor* PlayerFish = School->PlayerFishForSplit())
			{
				const float Fraction = aquarium::SplitFraction(PlayerFish->CurrentSpeed(), PlayerFish->MaxSpeed);
				BoidsParamsValue.avoidOnlyRadius = aquarium::SplitRadius(BaseAvoidOnlyRadius, Fraction, SplitParamsValue);
				BoidsParamsValue.avoidOnlyWeight = BaseAvoidOnlyWeight * (1.f + aquarium::SplitStrength(PlayerFish->CurrentSpeed(), PlayerFish->MaxSpeed, SplitParamsValue));
			}
```

`BaseAvoidOnlyRadius`/`BaseAvoidOnlyWeight`는 `InitializeSwim`에서 **기본 `BoidsParams`의 값으로부터** 한 번만 채운다. 매 틱 자기가 키운 값을 다시 키우면 반경이 폭주한다(M7의 방향 제한기가 자기 연출 회전을 되먹인 것과 같은 결함이다).

> **전제 확인:** `aquarium::BoidsParams`에 `avoidOnlyRadius`/`avoidOnlyWeight`라는 이름의 필드가 실제로 있는지 먼저 본다: `grep -n "avoidOnly" rules/include/aquarium/Boids.h`. **이름이 다르면 그 이름을 쓴다** — 계획이 이름을 지어내지 않는다.

- [ ] **Step 4 (RED → GREEN): Automation 세 개**

`Tests/FishActorTests.cpp`에 더한다.

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchoolPartsMoreWhenFast, "Aquarium.Fish.SchoolPartsMoreWhenFast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSchoolPartsMoreWhenFast::RunTest(const FString&)
{
	// 같은 배치에서 천천히 지나갈 때와 빠르게 지나갈 때의 밀려난 거리를 비교한다.
	FAquariumTestWorld World;
	const float SlowDisplacement = MeasurePartingForTest(World, /*speedFraction*/ 0.15f);
	const float FastDisplacement = MeasurePartingForTest(World, /*speedFraction*/ 1.0f);
	TestTrue(TEXT("faster parts more"), FastDisplacement > SlowDisplacement * 1.3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchoolBarelyPartsWhenStill, "Aquarium.Fish.SchoolBarelyPartsWhenStill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSchoolBarelyPartsWhenStill::RunTest(const FString&)
{
	FAquariumTestWorld World;
	const float StillDisplacement = MeasurePartingForTest(World, /*speedFraction*/ 0.f);
	const float FastDisplacement = MeasurePartingForTest(World, /*speedFraction*/ 1.0f);
	TestTrue(TEXT("still parts far less"), StillDisplacement < FastDisplacement * 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSplitRadiusDoesNotRunAway, "Aquarium.Fish.SplitRadiusDoesNotRunAway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSplitRadiusDoesNotRunAway::RunTest(const FString&)
{
	// 매 틱 자기가 키운 값을 다시 키우면 반경이 폭주한다(M7의 방향 제한기와 같은 결함).
	FAquariumTestWorld World;
	AFishActor* Fish = SpawnBackgroundFishForTest(World);
	AFishActor* Mine = World.GameMode()->PlayerFish();
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 600; ++i) { Mine->StepSwim(1.f / 60.f); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("radius is bounded"), Fish->AvoidOnlyRadiusForTest() <= Fish->BaseAvoidOnlyRadiusForTest() * 2.f);
	return true;
}
```

`MeasurePartingForTest`는 이 파일에 새로 쓰는 헬퍼다: 배경 물고기 한 마리를 정해진 자리에 놓고, 내 물고기를 주어진 속도 비율로 그 옆을 지나가게 한 뒤 **세로로 밀려난 거리**를 돌려준다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_split.cpp rules/include/aquarium/Split.h && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 197`.

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `111`.

- [ ] **Step 5: 변이 확인**

| 변이 | 바꿀 곳 | 빨간불이 켜져야 하는 테스트 |
|---|---|---|
| A | `SplitStrength`를 상수 `maxStrength` 반환으로 **대체**(일률적으로 세게) | `it is PROPORTIONAL, not a switch`, `Aquarium.Fish.SchoolBarelyPartsWhenStill` |
| B | `SplitFraction`을 `speed > 30.f ? 1.f : 0.f`로 **대체**(문턱 스위치) | `it is PROPORTIONAL, not a switch` |
| C | `SplitRadius`의 `avoidOnlyRadius` 인자를 상수 `60.f`로 **대체** | `the parting radius is DERIVED from the avoid radius` |
| D | 엔진에서 `BaseAvoidOnlyRadius` 대신 **현재 값**을 다시 키운다 | `Aquarium.Fish.SplitRadiusDoesNotRunAway` |
| E | 엔진의 비례 블록 전체를 **삭제** | `Aquarium.Fish.SchoolPartsMoreWhenFast` |

**변이 E가 M4c의 함정(`SchoolMatesPullTogether`가 무리를 끈 채 통과)의 재발 시험이다.** 빨간불이 안 켜지면 `MeasurePartingForTest`가 무리를 관측하지 못하고 있다는 뜻이다. 전부 되돌린다.

- [ ] **Step 6: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules tests CMakeLists.txt unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 무리 갈라짐을 속도 계기판으로

일률적으로 세게 갈라지면 그냥 연출이다. 비례해야 속도를 다루는 재미가 생긴다.
회피 반경과 세기를 내 물고기의 속도 비율에서 선형으로 파생하고, 기준값은
매 틱 다시 키우지 않도록 초기화 때 한 번만 보관한다(M7의 방향 제한기가 자기
연출 회전을 되먹인 것과 같은 결함을 피한다).

문턱 스위치로 바꾸는 변이와 비례 블록을 통째로 지우는 변이로 각각 빨간불을
확인했다 — 후자는 M4c에서 무리를 끈 채 통과한 테스트의 재발 시험이다.

규칙 189 → 197, Automation 108 → 111.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 16: **난이도를 실제로 잰다** — 이 마일스톤의 진짜 합격 판정

**Files:** Modify `.../DiverPlayerController.h/.cpp`, Create `scripts/measure_difficulty.sh`, Modify `docs/reviews/`(새 기록 파일)

시나리오 검증표가 **새로 추가한 행**이다:

> **난이도** — 새로 필요한 검증이다. 잡기가 실제로 어려운지는 테스트로 알 수 없다. 개발자가 직접 해 보고 "**처음 3분에 몇 마리 잡았는지**"를 기록하고, **아이가 해 본 결과와 비교**한다.

**이 태스크가 M8의 합격 판정이다.** 앞의 모든 단위 테스트가 초록불이어도 잡기가 시시하면 이 마일스톤은 실패다.

- [ ] **Step 1: 잡기 기록을 남긴다(`-AquariumCatchLog=<절대 경로 csv>`)**

클릭 로그(`ParseClickLogPath`)와 **똑같은 모양**으로 만든다 — 새 문법이 없고, 파싱 실패가 곧 "파일 없음"으로 드러난다.

`DiverPlayerController.h`:

```cpp
	// -AquariumCatchLog=<절대 csv 경로>; 없거나 비면 false. 개발 전용.
	// 한 행 = 판정 한 번(잡음/스침). **별명은 절대 들어가지 않는다.**
	static bool ParseCatchLogPath(const TCHAR* CmdLine, FString& OutPath);
```

행 형식(헤더 포함):

```
seconds,outcome,closing,threshold,dash_charge,count
12.43,Catch,0.31,0.225,0.12,1
```

`UCatchSubsystem`이 판정할 때마다 컨트롤러에 넘기고, 컨트롤러가 `EndPlay`에서 파일을 쓴다(클릭 로그와 같은 구조).

- [ ] **Step 2: 측정 스크립트 `scripts/measure_difficulty.sh`**

```bash
#!/usr/bin/env bash
# 난이도 측정. 사람이 3분 동안 직접 논다. 자동화할 수 없는 것이 요점이다.
#
# 사용:  bash scripts/measure_difficulty.sh <이름표>   (예: dev-run1, child-run1)
# 결과:  docs/reviews/difficulty/<이름표>.csv 와 마지막 요약 줄
set -euo pipefail
LABEL="${1:?사용: measure_difficulty.sh <이름표>}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/docs/reviews/difficulty"
mkdir -p "$OUT"
CSV="$OUT/$LABEL.csv"
UE="/Users/Shared/Epic Games/UE_5.8"

echo "3분 동안 논다. 끝나면 나가기를 누른다. 시드는 고정이다."
"$UE/Engine/Binaries/Mac/UnrealEditor" "$ROOT/unreal/Aquarium/Aquarium.uproject" \
  -game -windowed -ResX=1280 -ResY=720 \
  -AquariumAssignmentSeed=20260921 \
  -AquariumCatchLog="$CSV" \
  -AquariumAutoExitAfter=180 \
  -stdout -FullStdOutLogOutput 2>&1 | grep -E "Failed to compile Material|CATCHLOG" || true

# 판정: 처음 3분의 잡은 수 = Catch 행의 수.
CATCHES=$(grep -c ",Catch," "$CSV" || true)
BUMPS=$(grep -c ",Bump," "$CSV" || true)
echo "DIFFICULTY_OK label=$LABEL catches=$CATCHES bumps=$BUMPS csv=$CSV"
```

- [ ] **Step 3: 목표 범위를 정하고 적어 둔다**

**처음 3분, 처음 해 보는 아이 기준 목표: 3~8마리.**

근거:
- **0~2마리면 너무 어렵다.** 3분 동안 성공이 한 번도 없으면 "내가 못 하는 것"이 아니라 "안 되는 것"으로 읽히고, 아이는 끈다.
- **9마리 이상이면 너무 쉽다.** 3분에 9마리면 12분이면 36마리 전부다. 그러면 이 마일스톤의 전제("처음엔 3마리, 나중엔 15마리")가 무너지고 40분이 아니라 12분짜리 게임이 된다.
- **개발자(익숙해진 뒤)는 3분에 10~18마리**가 나와야 한다. 아이의 2~3배 — 그 격차가 "숙련이 존재한다"의 증거다. **격차가 1.5배 미만이면 숙련의 여지가 없다는 뜻이므로 실패다.**

**손잡이는 셋이고, 한 번에 하나만 돌린다:**

| 증상 | 돌릴 것 | 방향 |
|---|---|---|
| 너무 쉽다 | `CatchParams::catchSpeedFraction`(0.55) | 올린다 (0.70까지) |
| 너무 쉽다 | `EvadeParams::noticeRadius`(0.13) | 올린다 — 더 일찍 눈치챈다 |
| 너무 어렵다 | `CatchParams::graceScale`(1.15) | 올린다 (1.35까지). **절대 1.0 아래로 내리지 않는다** |
| 너무 어렵다 | `EvadeParams::dodgeSpeedScale`(1.7) | 내린다 (1.3까지) |
| 숙련의 격차가 없다 | `DashParams::burstScale`(2.4) | 올린다 — 잘하는 쪽이 더 이득을 본다 |

**손잡이를 돌릴 때마다 Task 16 Step 2를 다시 돌린다.** 예측으로 고치지 않는다(M4b 전례).

- [ ] **Step 4: 개발자 측정 두 번 (첫 3분, 그리고 연습 뒤 3분)**

```bash
cd /Users/hans/dev/aquarium && bash scripts/measure_difficulty.sh dev-first && bash scripts/measure_difficulty.sh dev-practiced
```
기대: 두 줄의 `DIFFICULTY_OK`. **`dev-first`가 12마리를 넘으면 이 게임은 아이에게 시시하다는 뜻이다** — 손잡이를 돌리고 다시 잰다. `dev-practiced / dev-first`가 1.5배 미만이면 숙련의 여지가 없다는 뜻이니 돌진의 이득을 키운다.

- [ ] **Step 5: 결과를 기록한다**

`docs/reviews/2026-09-21-m8-difficulty.md`에 표로 적는다: 측정 이름표, 잡은 수, 스친 수, 그때의 손잡이 값 세 개, 그리고 한 줄 소감. **아이 측정은 Task 20에서 사용자에게 인계한다** — 이 문서에 빈 행으로 자리를 만들어 둔다.

- [ ] **Step 6: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add scripts/measure_difficulty.sh docs/reviews unreal/Aquarium/Source/Aquarium && git commit -q -F - << 'EOF'
feat(m8): 난이도를 잰다 — 단위 테스트가 답할 수 없는 유일한 질문

잡기가 실제로 어려운지는 테스트로 알 수 없고, 쉬우면 이 마일스톤의 전제가
통째로 무너진다. 고정 시드로 3분을 직접 놀고 판정 로그를 CSV로 남긴다.

목표: 처음 해 보는 아이가 3분에 3~8마리. 0~2면 "안 되는 것"으로 읽혀 끄고,
9 이상이면 12분이면 다 끝나 40분짜리가 되지 못한다. 개발자가 연습 뒤 아이의
2~3배가 나와야 숙련이 존재한다는 증거가 된다.

손잡이 셋과 돌리는 방향을 적어 두었다. 한 번에 하나만, 예측이 아니라 측정으로.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 17: 성능 — 예측하지 말고 잰다

**Files:** Modify `scripts/measure_package_perf.sh`(또는 새 `scripts/measure_m8_perf.sh`), 기록만

M8이 틱마다 새로 하는 일: 36마리 × 1회 판정(수십 flops), 회피 상태 갱신 36개, 자국 인스턴스 수십 개, 포스트 프로세스 머티리얼 1패스. **포스트 프로세스가 제일 비쌀 것 같지만 그것은 예측이다** — M4b에서 "커튼이 제일 비쌀 것"이라는 예측이 측정으로 반증됐다.

- [ ] **Step 1: 기준선을 다시 잰다(M8 이전 커밋에서)**

```bash
cd /Users/hans/dev/aquarium && git stash list && bash scripts/measure_package_perf.sh 2>&1 | tail -5
```
기대: 61 fps대. **이 값은 수직동기에 눌린 값이고 실행 간 노이즈가 ~1.3 fps다.** 차이가 1.3 fps 안이면 "변화 없음"으로 읽는다.

- [ ] **Step 2: 항목별로 끄고 귀속한다**

개발 전용 불리언 플래그로만 끈다(값을 받는 새 문법을 만들지 않는다 — 조용히 무시되는 토큰이 생긴다):
`-AquariumNoCatch`(`UCatchSubsystem::bCatchEnabled`), `-AquariumNoWake`, `-AquariumNoWaterPush`.

```bash
cd /Users/hans/dev/aquarium && for f in "" "-AquariumNoCatch" "-AquariumNoWake" "-AquariumNoWaterPush"; do echo "--- $f"; bash scripts/measure_package_perf.sh $f 2>&1 | tail -2; done
```

- [ ] **Step 3: 판정**

- 전체가 기준선 대비 **1.3 fps 안**이면 아무것도 하지 않는다.
- 떨어졌다면 **가장 비싼 항목부터** 손본다. 예비 레버(`GridSizeZ` 128→64, +17.1 fps)는 **아직 쓰지 않았다** — 마지막 수단이고, 쓸 때는 시각 비교 클립을 함께 낸다.
- 결과를 `docs/reviews/2026-09-21-m8-perf.md`에 표로 적는다. **예측과 측정이 달랐다면 그 사실을 명시한다** — 이 프로젝트에서 두 번째 사례가 된다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add scripts docs/reviews && git commit -q -F - << 'EOF'
perf(m8): 항목별로 끄고 재서 귀속했다

포스트 프로세스가 제일 비쌀 것 같았지만 그것은 예측이다. 판정·자국·왜곡을
각각 끄고 측정했다. 노이즈 바닥이 1.3 fps이므로 그 안의 차이는 변화 없음으로
읽었다. 예비 레버(GridSizeZ 128→64)는 아직 쓰지 않았다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 18: 실제 RHI에서 머티리얼이 진짜 컴파일되는지 — **M7이 남긴 빚**

**Files:** 기록만(필요하면 `unreal/Aquarium/Scripts/build_fx_m7.py`·`build_fx_m8.py` 수정)

**머티리얼은 조용히 실패하고 회색 기본 머티리얼로 떨어지면서 에디터 뷰포트에서는 멀쩡해 보인다.** M7 장면 2의 실행자는 이 확인을 **하지 못한 채 넘겼고**, 그것이 지금 미해결로 남아 있다. M8이 갚는다.

- [ ] **Step 1: 실제 RHI로 짧게 돌리고 컴파일 실패를 센다**

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && "$UE/Engine/Binaries/Mac/UnrealEditor" "$PWD/unreal/Aquarium/Aquarium.uproject" -game -windowed -ResX=1280 -ResY=720 -AquariumAutoNickname=테스트 -AquariumAutoExitAfter=20 -stdout -FullStdOutLogOutput 2>&1 | tee /tmp/m8-rhi.log | grep -cE "Failed to compile Material|Material.*fell back|DefaultMaterial"
```
기대: `0`. **`-nullrhi`를 붙이면 이 질문에 답하지 못한다.** 0이 아니면 어떤 머티리얼인지 로그에서 이름을 뽑아 고친다.

- [ ] **Step 2: 네 머티리얼이 실제로 로드됐는지 이름으로 확인**

```bash
grep -E "M_Bubble|M_FoodPellet|M_Wake|M_WaterPush" /tmp/m8-rhi.log | head -20
```
기대: 네 이름이 전부 나오고, 그 줄에 오류 표현이 없다.

- [ ] **Step 3: 기포가 "귀여운 방울"로 읽히는지 캡처로 판정한다**

이것은 헤드리스로 판정할 수 없다. **실제 화면을 잡아 눈으로 본다.**

```bash
cd /Users/hans/dev/aquarium && UE="/Users/Shared/Epic Games/UE_5.8" && mkdir -p /tmp/m8-bubble && "$UE/Engine/Binaries/Mac/UnrealEditor" "$PWD/unreal/Aquarium/Aquarium.uproject" -game -windowed -ResX=1280 -ResY=720 -AquariumAutoNickname=테스트 -AquariumAssignmentSeed=20260921 -AquariumAutoClick="2@0.4x0.5,3@0.5x0.5,4@0.6x0.5" -AquariumCaptureUI=/tmp/m8-bubble -AquariumAutoExitAfter=8 -stdout -FullStdOutLogOutput 2>&1 | grep -E "armed|WARN|Warning" | head
```
**경고 0건과 `armed 3`을 확인한다.** M5의 캡처 하네스는 계획한 클릭 7개가 **전부 빈 물을 맞혔는데도** 경고 0건에 그럴듯한 클립을 냈다 — 그래서 `armed N`이 요청한 수와 같은지도 함께 본다.

그다음 실제로 **연다**:

```bash
open /tmp/m8-bubble && ls /tmp/m8-bubble | wc -l
```

판정 기준(시나리오의 연출 문법):
- 동글동글하고 또렷한 **구슬**로 보이면 실패다 → `build_fx_m7.py`의 `build_bubble`에서 프레넬 지수를 올리고(더 가장자리만) 불투명도를 내려 **난류의 흔적**에 가깝게 만든 뒤 다시 임포트·캡처한다.
- 빠르게 지나간 자리의 **하얀 줄**로 보이면 통과다.
- **판정 결과와 근거(어느 프레임을 봤는지)를 기록한다.** "보기 좋았다"는 판정이 아니다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Scripts docs/reviews && git commit -q -F - << 'EOF'
fix(m8): M7이 남긴 빚 — 실제 RHI에서 머티리얼 컴파일 확인

머티리얼은 조용히 실패하고 회색 기본으로 떨어지면서 에디터 뷰포트에서는
멀쩡해 보인다. -nullrhi 로그는 이 질문에 답하지 못한다. 실제 게임 실행 로그에서
Failed to compile Material 0건을 확인하고 네 머티리얼의 로드를 이름으로 봤다.

기포가 '귀여운 방울'로 읽히는지도 실제 캡처를 열어서 판정했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 19: 캡처 하네스 — 전/후 클립과 **무는 로그**

**Files:** Create `scripts/render_m8_catch.sh`

시나리오 검증표: "같은 맵·시드·입력으로 전/후 클립을 만들어 나란히 본다. **클립을 실제로 열어서 본다.**"

- [ ] **Step 1: 하네스를 쓴다**

`scripts/render_m5_click.sh`를 본으로 삼되(같은 구조를 따른다), **다음 넷을 반드시 단언한다**:
1. `-AquariumAutoInput`·`-AquariumAutoClick` 파서의 **경고 0건**
2. `armed N`이 요청한 수와 **같다**
3. 잡기 로그(`-AquariumCatchLog`)에 **`Catch` 행이 1개 이상** — 이것이 "클립에 실제로 잡는 장면이 있다"의 유일한 증거다. **없으면 클립은 그럴듯한 쓰레기다**(M5에서 정확히 그랬다)
4. `Failed to compile Material` **0건**(실제 RHI 실행이다)

```bash
cd /Users/hans/dev/aquarium && bash scripts/render_m8_catch.sh 2>&1 | tail -8
```
기대: 마지막 줄 `M8_CLIP_OK catches=<N>=1 이상 warnings=0 armed=<요청 수> materials_failed=0 out=<경로>`.

- [ ] **Step 2: 클립을 **실제로 연다****

```bash
open docs/reviews/clips/m8-catch-after.mp4
```
보는 것 넷: ① 놈이 **닿기 전에** 옆으로 튀는가 ② 잡히는 순간 화면이 **짧게** 흔들리는가(길면 지진이다) ③ 도장이 몸에 **붙어 다니는가** ④ 숫자가 오르는가. **안 보이면 통과가 아니다.**

- [ ] **Step 3: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add scripts/render_m8_catch.sh docs/reviews && git commit -q -F - << 'EOF'
test(m8): 캡처 하네스 — 클립에 잡는 장면이 실제로 있다는 것을 단언한다

M5의 하네스는 계획한 클릭 7개가 전부 빈 물을 맞혔는데도 경고 0건에 그럴듯한
클립을 냈다. 그래서 이 하네스는 파서 경고 0건과 armed 수뿐 아니라 잡기 로그의
Catch 행이 1개 이상인지도 단언한다. 없으면 클립은 그럴듯한 쓰레기다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 20: 문서 갱신과 **사람만 할 수 있는 확인** 인계

**Files:** Modify `docs/TASK.md`, Modify `docs/ASSETS.md`, Modify `docs/SETUP.md`, Create `docs/reviews/2026-09-21-m8-summary.md`

- [ ] **Step 1: `docs/TASK.md`에 M8을 적는다**

로드맵 표에 M8 행을 더하고, **사양 변경 절**에 아래를 추가한다(SRS 문구 개정은 **사용자 결정 사항이라 하지 않는다** — 기록만 한다).

```markdown
## M8에서 확정된 사양 변경 (2026-09-21)

- **주된 동사가 바뀌었다.** "클릭한다"에서 "내 물고기로 부딪혀 잡는다"로. 클릭은 폐기되지 않고
  **몰이 도구로 격하**됐다(산호 뒤에 숨은 놈을 튀어나오게 만드는 용도).
- **접촉은 화면 좌표에서 판정한다.** 물고기는 월드 X가 고정된 평면 위에 살고 내 물고기(X=220)와
  배경 물고기(X>=330)는 3차원에서 닿을 수 없다. 평면 구조를 깨면 M3의 경계 보장과 M4c의
  평면별 장애물 사전계산이 함께 무너지므로, 카메라에서 본 겹침으로 정의했다. 클릭(F-09)이
  이미 쓰는 것과 같은 기하다.
- **규칙 순서에 회피가 들어갔다**: 입력/Wander → 먹이 → 회피 → 도망 → 무리 → 장애물 → 경계
  → StepMotion → Clamp. 경계는 여전히 마지막이다.
- **새 입력: 스페이스(돌진).** 거절하지 않고 연타하면 약해질 뿐이다.
- **쌓이는 숫자 하나가 생겼다.** 초판의 "점수 금지"는 **잃는 점수**를 말한 것이었다는 시나리오
  개정을 따른다. 분모도 라벨도 없다.
- **F-11 폐기와 F-12 실질 폐기는 M7에서 결정된 그대로 유지된다.** SRS 문구 개정은 사용자
  결정 사항이라 손대지 않았다.
```

- [ ] **Step 2: `docs/ASSETS.md`** — `S_Thud.wav`의 출처(생성 스크립트)와 `S_Startle.wav`의 **재합성 사실**을 적는다. 앰비언스 행은 **여전히 비어 있다**(사용자가 파일을 주지 않았다).

- [ ] **Step 3: `docs/SETUP.md`** — 새 개발 플래그 네 개(`-AquariumCatchLog=`, `-AquariumNoCatch`, `-AquariumNoWake`, `-AquariumNoWaterPush`)와 `scripts/measure_difficulty.sh`, `scripts/render_m8_catch.sh`, `unreal/Aquarium/Scripts/build_fx_m8.py`를 적는다.

- [ ] **Step 4: 사람만 할 수 있는 확인을 인계한다** — `docs/reviews/2026-09-21-m8-summary.md`

```markdown
# M8 요약 — 사용자 확인이 필요한 항목

## 1. 난이도 (가장 중요하다)
`bash scripts/measure_difficulty.sh child-run1`을 **아이에게 시킨다.** 3분.
- 목표: **3~8마리.** 0~2면 너무 어렵고, 9 이상이면 너무 쉽다.
- 개발자 측정(`dev-first`, `dev-practiced`)은 이미 `docs/reviews/2026-09-21-m8-difficulty.md`에 있다.
- 아이 결과가 범위 밖이면 그 문서의 손잡이 표대로 **한 번에 하나씩** 돌린다.

## 2. 소리 (헤드리스로 들을 수 없다)
- 「퍽」과 「쿵」이 **구분되는가**. 놀란 것과 잡힌 것이 소리만으로 갈려야 한다.
- 「꺅」이 사라졌는가. 만화 비명으로 들리면 이 나이대에는 그것이 게임을 끄는 이유다.
- 앰비언스를 스피커로 **20분** 틀어 놓고 일해 본다(아직 파일이 없어 보류 중이다).

## 3. 유치함 판정 (자동화 불가)
- 기포가 **귀여운 방울**로 보이는가, **난류의 흔적**으로 보이는가.
- 화면 흔들림이 **짧은가**. 길면 지진이고 연타할 때 멀미한다.
- 도장 글자가 화면을 **글자밭**으로 만들지는 않는가(36개가 다 붙었을 때).

## 4. 최종 판정
**아이가 직접 한다.** 판단 문장: "이 아이가 이걸 친구에게 보여주고 싶어 할까?"
```

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add docs && git commit -q -F - << 'EOF'
docs(m8): 사양 변경 기록과 사람만 할 수 있는 확인 인계

주된 동사 변경, 화면 좌표 접촉 판정의 근거, 규칙 순서에 들어간 회피, 새 입력,
쌓이는 숫자 하나를 TASK.md에 적었다. SRS 문구 개정은 사용자 결정 사항이라
하지 않았다.

사용자 확인 네 항목을 인계한다. 첫 번째가 난이도이고, 그것이 이 마일스톤의
진짜 합격 판정이다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

### Task 21: 자체 검토와 마감

**Files:** 이 문서(`docs/superpowers/plans/2026-09-21-m8-skill-game.md`)

- [ ] **Step 1: 전체 초록불 확인(양쪽 다)**

```bash
cd /Users/hans/dev/aquarium && cmake --build build -j 2>&1 | tail -2 && ctest --test-dir build 2>&1 | tail -2 && UE="/Users/Shared/Epic Games/UE_5.8" && for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:"; done && "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "Tests Failed|Test Completed.*Fail" | head
```
기대: `100% tests passed out of 197`, 실패 줄 없음. (실제 숫자가 다르면 그 숫자를 쓴다 — 규약 9.)

- [ ] **Step 2: 시나리오 대조 — 한 줄씩 짚는다**

| 시나리오 항목 | 어디서 지켜졌나 | 확인 |
|---|---|---|
| 부딪혀 잡기 | Task 2·7·9 | ☐ |
| 놈이 먼저 눈치챈다 | Task 3·8 | ☐ |
| 관성 → 숙련 | 기존 `Motion.h` + Task 4(돌진) | ☐ |
| 도장, 떼기 없음, 나가기로 리셋 | Task 5·10 | ☐ |
| 도장의 그림은 이름표 | Task 10 | ☐ |
| 숫자 하나, 점수판 없음 | Task 11 | ☐ |
| 완주는 조용히 | Task 13 | ☐ |
| 먹이 = 미끼 | Task 14 | ☐ |
| 갈라짐은 속도 비례 | Task 15 | ☐ |
| 부풀리지 않는다 | 어느 태스크에도 크기 배율 코드가 없다 | ☐ |
| 만화 비명 금지 | Task 1 | ☐ |
| 귀여운 기포 금지 | Task 12(자국은 뜨지 않는다)·18(캡처 판정) | ☐ |
| 반짝이·칭찬 금지 | Task 11·13 | ☐ |
| 속도·짧은 흔들림·물 밀림·낮은 소리 | Task 1·4·12 | ☐ |
| 난이도 측정 | Task 16 | ☐ |

**☐가 하나라도 남으면 마감하지 않는다.**

```bash
cd /Users/hans/dev/aquarium && grep -rn "SetActorScale3D\|scale.*1\.5\|Puff\|Sparkle\|잘했\|멋져" unreal/Aquarium/Source/Aquarium/CatchSubsystem.cpp unreal/Aquarium/Source/Aquarium/HudWidget.cpp rules/include/aquarium/Impact.h rules/include/aquarium/Catch.h
```
기대: **출력 없음.** 부풀기·반짝이·칭찬이 한 줄도 없다는 것을 기계로 확인한다.

- [ ] **Step 3: 앰비언스 게이트가 여전히 정당하게 빨간불인지**

```bash
cd /Users/hans/dev/aquarium && bash scripts/check_ambience.sh; echo "exit=$?"
```
기대: Task 0 Step 6과 **똑같이** 실패한다. 이 계획이 약화하지 않았다는 증거다.

- [ ] **Step 4: 「구현 중 발견한 후속 항목」을 채운다**

아래 절을 **반드시** 채운다. 아무것도 없었다면 "없음"과 그 근거를 적는다. **빈 채로 마감하지 않는다.**

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add docs && git commit -q -F - << 'EOF'
docs(m8): 자체 검토와 마감 — 시나리오 항목을 한 줄씩 대조했다

부풀기·반짝이·칭찬이 코드에 한 줄도 없다는 것을 grep으로 확인했고,
앰비언스 게이트가 여전히 정당하게 빨간불인 것도 확인했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
git log --oneline -1
```

---

## 구현 중 발견한 후속 항목

*(실행 중에 채운다. 비워 둔 채로 마감하지 않는다 — 아무것도 없었다면 "없음"과 그 근거를 적는다.)*

### 사양 변경 (조용히 넘어가지 않는 것)

### 계획의 오류와 수정

### 남은 것 / 별도 태스크로 넘긴 것

---

## 이 계획이 하지 않는 것

시나리오가 **다음 순서 또는 범위 밖**으로 명시한 것들이다. 구현 중에 "겸사겸사" 넣지 않는다.

- **위협(그물·큰 물고기)** — 시나리오 우선순위 5. 잡기가 재미있다는 것이 아이에게 확인된 **다음** 순서다. 지금 넣으면 두 가지 새 재미가 섞여 무엇이 통했는지 판단할 수 없다(M4를 셋으로 쪼갠 것과 같은 이유).
- **2인 겨루기와 방해** — 우선순위 6. 옵션 D 위에서 별도 마일스톤이다. **돌진 키를 두 벌 만들어야 한다는 것**만 여기 적어 둔다(스페이스는 한 명분이다).
- **2인 전용 도발 이모트** — 우선순위 7, 곁가지.
- **사진 이름표** — 시나리오가 "이름표를 먼저 완성하고 사진은 별도 스파이크 뒤에 판단한다"로 명시했다. 위험이 전부 "고르는 방법"에 몰려 있고(HEIC·EXIF·샌드박스·`ImportFileAsTexture2D`의 경로 로깅), 1일짜리 entitlement 스파이크가 선행 조건이다.
- **앰비언스(M7 Task 6)** — CC0 파일을 사용자가 주기 전까지 보류. `scripts/check_ambience.sh`는 **정당하게 빨간불이고 약화하지 않는다.**
- **잃는 점수·타이머 압박·진행 날리기·실패 화면·먹이 자원 개념** — 영구 금지.
- **크기 부풀기·만화 비명·귀여운 기포·반짝이·축하 화면·별 세 개** — 유치함 신호, 영구 금지.
- **Niagara·MetaSound·SoundCue** — 스크립트로 만들 수 없는 에셋은 쓰지 않는다.
