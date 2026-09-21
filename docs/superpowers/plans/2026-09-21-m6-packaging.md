# M6 — macOS 패키징·성능 판정·출하 검증 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 이 게임을 **패키징하고, 그 패키지에 대해 SRS의 모든 미판정 항목을 판정한다.** 산출물은 (1) 실행되는 macOS 패키지 두 개(Development / Shipping), (2) **SRS 규약(패키지 개발 빌드·30초 예열·3분)의 공식 성능 판정**, (3) MCP 서버·개발 플래그·학생 별명이 배포물에 없다는 **단언된 증거**, (4) 배포물 기준 에셋 출처 확인, (5) 사용자 수동 확인 5항목 인계. **코드 기능은 더하지 않는다.** 규칙 103 유지, Automation 60 유지.

**Architecture:** M6은 대부분 **검증**이다. 그래서 이 계획의 단위는 "무슨 코드를 쓰는가"가 아니라 **"무슨 명령이 무슨 증거를 만드는가"**다. 새로 만드는 것은 셸 스크립트 셋(`package_mac.sh`, `verify_package.sh`, `measure_package_perf.sh`)과 문서뿐이고, 소스 수정은 `.uproject`의 플러그인 제한 한 줄뿐이다.

**Tech Stack:** Unreal 5.8.2 `RunUAT BuildCookRun`, xcodebuild 27.0, bash, python3, ffmpeg.

**설계 문서:** [`docs/superpowers/specs/2026-09-21-m6-packaging-design.md`](../specs/2026-09-21-m6-packaging-design.md)

**공통 경로**

```bash
ROOT=/Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
SCRATCH=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad
PKG="$SCRATCH/m6_package"        # 패키지 출력. 저장소 밖이고 커밋하지 않는다
```

**공통 명령**

- 규칙 테스트: `cd $ROOT && cmake -S . -B build && cmake --build build -j && ctest --test-dir build`
- UE 에디터 빌드(**반드시 두 번**): `"$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PROJ" -WaitMutex`
- Automation: `"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "Test Completed|Tests Failed"`

**모든 태스크에 걸리는 규약 아홉 가지 (전부 이 프로젝트가 실제로 대가를 치른 것):**

1. **이 ctest는 `100% tests passed out of N` 형식을 쓴다.** `"0 tests failed out of"`를 grep하면 **아무것도 검증하지 않은 채 통과한다.**
2. **규칙 테스트 mtime 함정** — 같은 초 안에 다시 빌드하면 `make`가 재컴파일을 건너뛰고 ctest가 낡은 결과를 보고한다. RED 확인 시 `touch`로 mtime을 올린다.
3. **UE 모듈은 자동화 전에 두 번 빌드한다.** UBT가 `UnrealEditor.modules`를 한 빌드 늦게 쓴다.
4. **에디터 Python은 `-FullStdOutLogOutput` 없이 돌리면 아무것도 출력하지 않는다**(`AssertionError`조차). 그런데 레벨은 덮어쓴다. **종료 코드는 무관한 `GameFeatureData` 오류 때문에 항상 1이다. 판정은 `*_OK` grep으로만 한다.**
5. **머티리얼은 조용히 실패하고 회색 기본 머티리얼로 떨어지면서 뷰포트에서는 멀쩡해 보인다.** `-nullrhi`가 **아닌** 실제 실행 로그에서 `Failed to compile Material` 0건을 grep한다. **M6에서는 그 실행이 패키지 실행이어야 한다.**
6. **검사는 아무것도 검사하지 않으면서 통과할 수 있다.** `PropMaterialsCompile`은 `checked 0 of 13`을 찍으며 Success였고, `SchoolMatesPullTogether`는 무리를 완전히 끈 채 통과했으며, `UpVectorStaysUpright`는 구조상 빨간불이 될 수 없고, 계획서가 적은 M5의 클릭 좌표 7개는 전부 빈 물이었다. **이 계획의 모든 신규 검사는 빨간불을 먼저 확인한다. 제안한 변이로 빨간불이 안 켜지면 그것은 통과의 증거가 아니다 — 켜지는 변이를 찾거나, 찾지 못했다는 사실을 후속 항목에 적는다.** M6의 검사 대부분은 "없음을 확인"하는 형태라 이 함정에 **가장 취약**하다.
7. **`grep`이 0을 돌려주는 이유는 둘이다** — 정말 없거나, **엉뚱한 파일을 보고 있거나.** M6의 "없음" 검사는 전부 **대조군**을 함께 돌린다.
8. **같은 규칙을 두 군데 두면 검증이 결함을 영원히 못 잡는다.** 개발 플래그 토큰 목록은 손으로 베끼지 않고 **소스에서 뽑는다.**
9. **계획의 전제는 틀릴 수 있다.** 이 프로젝트의 **모든** 마일스톤에서 계획서 오류가 나왔다(기대 HEAD 값, 없는 플래그 토큰, 물리적으로 불가능한 테스트 기대값, 틀린 half-extent). **각 태스크의 첫 스텝은 그 태스크가 기대는 전제를 명령으로 확인하는 것이고, 어긋나면 진행하지 말고 보고한다.**

---

### Task 0: 전제 확인 — 이 계획이 기대는 모든 사실을 명령으로 검증한다

**Files:** 없음 (읽기 전용)

- [ ] **Step 1: 브랜치와 작업 트리**

```bash
cd /Users/hans/dev/aquarium
git status --short; git rev-parse --abbrev-ref HEAD; git rev-parse --short HEAD
```
기대: 첫 명령 출력 없음, `feat/m6-packaging`, `dd22ad5`. **다르면 멈추고 보고한다.**

- [ ] **Step 2: 규칙 103개**

```bash
cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 103`. (규약 1 — `0 tests failed out of`를 찾지 말 것.)

- [ ] **Step 3: Automation 60개**

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `Result: Succeeded` 두 번, 마지막 줄 `60`. (규약 3 — 빌드는 두 번.)

- [ ] **Step 4: MCP 플러그인이 정말 게임 타깃에 들어가는 상태인지 확인** — 이 계획 Task 2의 전제다.

```bash
cd /Users/hans/dev/aquarium
python3 - <<'PY'
import json
d = json.load(open('unreal/Aquarium/Aquarium.uproject'))
for p in d.get('Plugins', []):
    print(p.get('Name'), 'Enabled=', p.get('Enabled'),
          'TargetAllowList=', p.get('TargetAllowList'), 'TargetDenyList=', p.get('TargetDenyList'))
u = json.load(open('/Users/Shared/Epic Games/UE_5.8/Engine/Plugins/Experimental/ModelContextProtocol/ModelContextProtocol.uplugin'))
for m in u.get('Modules', []):
    print('module', m['Name'], m['Type'], m.get('LoadingPhase'))
PY
```
기대(현 상태):
```
ModelContextProtocol Enabled= True TargetAllowList= None TargetDenyList= None
AllToolsets Enabled= True TargetAllowList= None TargetDenyList= None
module ModelContextProtocol Runtime Default
module ModelContextProtocolEngine Runtime Default
module ModelContextProtocolEditor Editor Default
module ModelContextProtocolTests UncookedOnly Default
...
```
**이미 `TargetAllowList`가 붙어 있거나 Runtime 모듈이 없으면 Task 2의 전제가 틀린 것이므로 멈추고 보고한다.**

- [ ] **Step 5: 개발 플래그 토큰 목록을 소스에서 뽑아 보관** (규약 8 — 뒤 태스크가 이 파일을 쓴다)

```bash
SCRATCH=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad
mkdir -p "$SCRATCH"
cd /Users/hans/dev/aquarium
grep -rhoE '\-Aquarium[A-Za-z]+=?' unreal/Aquarium/Source/Aquarium/*.cpp \
  | tr -d '-=' | sort -u | tee "$SCRATCH/devflags.txt"
wc -l < "$SCRATCH/devflags.txt"
```
기대: `AquariumAssignmentSeed`, `AquariumAutoClick`, `AquariumAutoExitAfter`, `AquariumAutoInput`, `AquariumAutoNickname`, `AquariumCaptureUI`, `AquariumClickLog`, `AquariumFrameStats`, `AquariumNoFlee`, `AquariumNoPropAvoid`, `AquariumNoSchooling` 등 **10개 이상**. 개수가 0이면 grep 패턴이 잘못된 것이므로 멈춘다.

- [ ] **Step 6: 성능 스크립트의 기본 측정 길이 확인** — Task 7의 전제다.

```bash
grep -nE 'RUN_SEC=|WARMUP_SEC=' /Users/hans/dev/aquarium/scripts/measure_m2b_perf.sh
```
기대: `RUN_SEC="${RUN_SEC:-95}"`, `WARMUP_SEC="${WARMUP_SEC:-30}"`.
**즉 기본값으로는 실측 구간이 65초뿐이고 SRS의 3분에 미달한다. 공식 실행은 반드시 `RUN_SEC=210`이다.**

- [ ] **Step 7: 디스크 여유 확인** — 쿡+스테이징+패키지 2벌은 수 GB다.

```bash
df -h /private/tmp | tail -1
```
기대: 여유 **20 GB 이상**. 미만이면 멈추고 보고한다.

- [ ] **Step 8: 커밋 없음** — 읽기 전용 태스크다. 다음 태스크로 넘어간다.

---

### Task 1: UBT 앱 마무리 실패의 진단 (M0부터 이월)

**Files:** Create `docs/reviews/2026-09-21-m6-ubt.md`

M0부터 이월된 `exit 65`를 **정면으로** 본다. 이 태스크는 원인을 찾거나, 찾지 못했다는 사실과 그때까지 배제한 가설을 기록한다. **우회가 통한다는 이유로 건너뛰지 않는다.**

- [ ] **Step 1: 실패를 현재 환경에서 재현한다**

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
SCRATCH=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad
mkdir -p "$SCRATCH/m6_ubt"
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" Aquarium Mac Development \
  -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex \
  > "$SCRATCH/m6_ubt/build-dev.log" 2>&1; echo "exit=$?"
grep -nE "App finalization|PostBuildSync|xcodebuild|exit code|Result:" "$SCRATCH/m6_ubt/build-dev.log" | tail -30
```
**두 결과 모두 가능하고, 어느 쪽인지가 이 태스크의 첫 사실이다.**
- 성공(`Result: Succeeded`, 오류 없음)이면 **M0의 증상이 재현되지 않는 것**이다. Step 2~5를 건너뛰고 Step 6에 "현재 환경에서 재현되지 않음"을 기록한 뒤, Task 3의 `BuildCookRun`에서 다시 확인한다.
- 실패하면 계속한다.

- [ ] **Step 2 (D1): UBT 로그에서 실제 `xcodebuild` 호출을 확보한다**

```bash
UE="/Users/Shared/Epic Games/UE_5.8"
ls -l "$UE/Engine/Programs/UnrealBuildTool/Log.txt"
grep -nE "xcodebuild|UE_XCODE_BUILD_MODE|Touch UBT generated tiles" "$UE/Engine/Programs/UnrealBuildTool/Log.txt" | tail -20
```
호출 줄 전체를 `$SCRATCH/m6_ubt/xcodebuild-cmd.txt`에 그대로 복사해 둔다. **기억으로 재구성하지 않는다.**

- [ ] **Step 3 (D2): pre-action 스크립트 본문을 읽는다**

```bash
cd /Users/hans/dev/aquarium/unreal/Aquarium
find . -name "*.xcscheme" -newermt '-1 day' 2>/dev/null | head
# 찾은 스킴에서 pre-action 본문을 뽑는다
for s in $(find . -name "*.xcscheme" 2>/dev/null); do
  python3 - "$s" <<'PY'
import sys, re
t = open(sys.argv[1]).read()
if 'Touch UBT generated tiles' in t or 'PreAction' in t:
    print('=== ', sys.argv[1])
    for m in re.finditer(r'scriptText\s*=\s*"(.*?)"', t, re.S):
        print(m.group(1).replace('&#10;', '\n'))
PY
done
```
**이 스크립트가 무엇을 하는지, 어떤 경로·환경 변수에 의존하는지 읽어서 기록한다.** "출력 없이 실패"의 가장 유력한 설명이 여기 있다 — pre-action 출력은 stdout이 아니라 Xcode 빌드 로그로 간다.

- [ ] **Step 4 (D3): `-resultBundlePath`를 붙여 손으로 돌리고 pre-action 로그를 연다**

```bash
SCRATCH=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad
rm -rf "$SCRATCH/m6_ubt/ubt.xcresult"
# Step 2에서 확보한 명령 뒤에 -resultBundlePath 를 덧붙여 실행한다
eval "$(cat "$SCRATCH/m6_ubt/xcodebuild-cmd.txt") -resultBundlePath $SCRATCH/m6_ubt/ubt.xcresult" \
  > "$SCRATCH/m6_ubt/xcodebuild-manual.log" 2>&1; echo "exit=$?"
tail -20 "$SCRATCH/m6_ubt/xcodebuild-manual.log"
xcrun xcresulttool get --legacy --path "$SCRATCH/m6_ubt/ubt.xcresult" --format json 2>/dev/null | head -60
```
기대: M0과 같다면 `BUILD SUCCEEDED`. **성공하더라도 result bundle에서 pre-action의 경고를 찾는다** — 손 실행이 조용히 다른 길로 가는지가 핵심 질문이다.

- [ ] **Step 5 (D4): 환경 차이 가설을 검정한다**

```bash
SCRATCH=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad
env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME="$HOME" \
  /bin/bash -c "$(cat "$SCRATCH/m6_ubt/xcodebuild-cmd.txt")" \
  > "$SCRATCH/m6_ubt/xcodebuild-envless.log" 2>&1; echo "exit=$?"
tail -20 "$SCRATCH/m6_ubt/xcodebuild-envless.log"
```
- **여기서 exit 65가 재현되면 원인은 환경 변수다.** `TMPDIR`, `DEVELOPER_DIR`, `USER`, `LANG`을 하나씩 되돌리며 범인을 특정하고 기록한다.
- 재현되지 않으면 환경 가설은 **기각**이다. 그렇게 적는다.

- [ ] **Step 6: 진단 기록을 쓴다** — `docs/reviews/2026-09-21-m6-ubt.md`

다음 구조로 쓴다. **추측과 측정을 섞지 않는다.**

```markdown
# M6 — UBT 앱 마무리(exit 65) 진단

작성일: 2026-09-21 · 환경: Xcode 27.0 (Build 27A266a) / UE 5.8.2 / macOS 27.0

## 증상 (M0에서 기록, 2026-09-21 재확인)
...현재 환경에서 재현되는가 / 안 되는가를 Step 1 결과 그대로...

## 확인한 사실
| # | 확인 | 명령 | 결과 |
|---|---|---|---|
| D1 | UBT가 실제로 실행한 xcodebuild 명령 | ... | ... |
| D2 | `Touch UBT generated tiles` pre-action 본문 | ... | ... |
| D3 | 손 실행 + result bundle | ... | ... |
| D4 | 환경 비운 실행 | ... | ... |

## 기각한 가설
- 샌드박스 (M0에서 기각)
- `Build/Mac/Resources` 부재 (M0에서 기각)
- `TMPDIR` 부재 (M0에서 기각)
- ... D4의 결과 ...

## 결론
(원인 특정 / 원인 미특정 중 하나를 명확히. 미특정이면 그렇게 적는다.)

## 패키징에 대한 영향
`RunUAT BuildCookRun`은 내부에서 UBT를 불러 게임 타깃을 빌드하므로 같은 단계를 지난다.
Task 3에서 실제로 부딪히는지 확인하고, 부딪히면 설계 문서의 3단 대응을 순서대로 적용한다.
```

- [ ] **Step 7: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add docs/reviews/2026-09-21-m6-ubt.md
git commit -m "$(cat <<'EOF'
docs: M6 UBT 앱 마무리(exit 65) 진단 기록

M0부터 이월된 xcodebuild PostBuildSync 실패를 D1~D4로 진단했다.
패키징 단계에서 같은 벽에 부딪히는지는 BuildCookRun 실행으로 확인한다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 2: MCP 플러그인을 게임 타깃에서 배제한다

**Files:** Modify `unreal/Aquarium/Aquarium.uproject`

Task 0 Step 4가 확인한 대로 `ModelContextProtocol`의 두 Runtime 모듈은 **지금 상태로 쿡·스테이징된다.** 플러그인을 끄지 않고 **에디터 타깃으로만 제한**한다(끄면 `CaptureViewport` 검증 채널이 사라진다).

- [ ] **Step 1: `.uproject`를 고친다**

`unreal/Aquarium/Aquarium.uproject`의 `Plugins` 배열을 아래로 **완전히 교체**한다.

```json
	"Plugins": [
		{
			"Name": "ModelContextProtocol",
			"Enabled": true,
			"TargetAllowList": [
				"Editor"
			]
		},
		{
			"Name": "AllToolsets",
			"Enabled": true,
			"TargetAllowList": [
				"Editor"
			]
		}
	]
```

- [ ] **Step 2: JSON이 깨지지 않았는지 확인한다**

```bash
cd /Users/hans/dev/aquarium
python3 -c "
import json; d=json.load(open('unreal/Aquarium/Aquarium.uproject'))
print([(p['Name'], p.get('TargetAllowList')) for p in d['Plugins']])"
```
기대: `[('ModelContextProtocol', ['Editor']), ('AllToolsets', ['Editor'])]`

- [ ] **Step 3: 에디터 워크플로가 그대로인지 확인한다** (규약 3 — 두 번 빌드)

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:|error:"; done
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
```
기대: `Result: Succeeded` 두 번, `60`.
**에디터 타깃에서 플러그인이 여전히 켜지는지도 확인한다** — 빌드 로그에 MCP 모듈이 보여야 한다.

```bash
grep -c "ModelContextProtocol" "$UE/Engine/Programs/UnrealBuildTool/Log.txt"
```
기대: **0이 아님.** 0이면 `TargetAllowList`가 에디터까지 막은 것이고, 그러면 `["Editor"]` 철자를 UE 5.8의 `TargetType` 이름(`Game`/`Editor`/`Client`/`Server`/`Program`)과 대조해 고친다.

- [ ] **Step 4: 게임 타깃에서는 빠지는지 UBT 수준에서 먼저 본다** (패키징 전 빠른 신호)

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
SCRATCH=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" Aquarium Mac Development \
  -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex \
  > "$SCRATCH/m6_game_build.log" 2>&1 || true
grep -ci "ModelContextProtocol" "$SCRATCH/m6_game_build.log"
```
기대: **`0`.** 0이 아니면 제한이 먹지 않은 것이므로 진행하지 말고 보고한다.
(이 빌드 자체는 Task 1의 exit 65 때문에 실패할 수 있다. **여기서 보는 것은 종료 코드가 아니라 grep 결과다.**)

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add unreal/Aquarium/Aquarium.uproject
git commit -m "$(cat <<'EOF'
fix(unreal): MCP 플러그인을 에디터 타깃으로 제한

ModelContextProtocol의 두 모듈은 Type=Runtime이라 아무 조치 없이 패키징하면
배포 게임에 MCP 런타임이 들어간다(docs/SETUP.md:97의 확인 항목).
플러그인을 끄면 CaptureViewport 검증 채널이 사라지므로 TargetAllowList=["Editor"]로
에디터에서만 켠다. 게임 타깃 빌드 로그에 ModelContextProtocol 0건 확인.
실제 배제 여부는 패키지에서 단언한다(verify_package.sh).

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 3: 패키징 스크립트 — `scripts/package_mac.sh`

**Files:** Create `scripts/package_mac.sh`

한 줄로 재현되는 패키징을 만든다. **Task 1의 벽에 부딪히면 설계 문서의 3단 대응을 이 스크립트 안에 박는다** — 손으로 치는 단계를 남기지 않는다.

- [ ] **Step 1: 스크립트를 만든다**

`scripts/package_mac.sh` 전체 내용:

```bash
#!/bin/bash
# Packages the macOS game. CONFIG=Development (default) or Shipping.
#
# Output goes OUTSIDE the repository: a packaged build is a multi-GB build
# artefact, and AGENTS.md forbids committing build output. The repo keeps only
# the verification evidence (hashes, logs, the perf report).
#
# KNOWN M0 ISSUE: UBT's final "App finalization" (xcodebuild PostBuildSync) step
# has failed silently with exit 65 on this machine since M0, while the identical
# xcodebuild command succeeds when run by hand. See
# docs/reviews/2026-09-21-m6-ubt.md. If BuildCookRun hits it, this script falls
# back to the supported split: build + manual finalize + -skipbuild cook/stage.
# The fallback is SCRIPTED on purpose -- a build that needs a human to type a
# command is not reproducible.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
CONFIG="${CONFIG:-Development}"
OUT="${OUT:-/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package/$CONFIG}"
LOG="${LOG:-/tmp/m6-package-$CONFIG.log}"

case "$CONFIG" in Development|Shipping) ;; *) echo "CONFIG must be Development or Shipping" >&2; exit 1;; esac

mkdir -p "$OUT"
echo "packaging $CONFIG -> $OUT (log: $LOG)"

set +e
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$PROJ" \
  -platform=Mac -clientconfig="$CONFIG" -configuration="$CONFIG" \
  -build -cook -stage -pak -package -archive -archivedirectory="$OUT" \
  -nop4 -utf8output -unattended -nocompileeditor \
  > "$LOG" 2>&1
RC=$?
set -e

if [[ $RC -ne 0 ]]; then
  echo "BuildCookRun failed (rc=$RC). Checking for the known PostBuildSync wall."
  grep -nE "PostBuildSync|App finalization|Touch UBT generated tiles|exit code 65|ERROR:" "$LOG" | tail -20
  if grep -qE "PostBuildSync|App finalization" "$LOG"; then
    echo "FALLBACK: build + manual finalize + -skipbuild"
    "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" Aquarium Mac "$CONFIG" \
      -Project="$PROJ" -WaitMutex >> "$LOG" 2>&1 || true
    # The exact xcodebuild line is taken from the UBT log, not from memory.
    XC="$(grep -oE 'xcodebuild .*UE_XCODE_BUILD_MODE=PostBuildSync.*' \
          "$UE_ROOT/Engine/Programs/UnrealBuildTool/Log.txt" | tail -1)"
    if [[ -z "$XC" ]]; then
      echo "ERROR: could not recover the PostBuildSync command from the UBT log" >&2
      exit 1
    fi
    echo "running: $XC" >> "$LOG"
    eval "$XC" >> "$LOG" 2>&1
    "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
      -project="$PROJ" \
      -platform=Mac -clientconfig="$CONFIG" -configuration="$CONFIG" \
      -skipbuild -cook -stage -pak -package -archive -archivedirectory="$OUT" \
      -nop4 -utf8output -unattended -nocompileeditor \
      >> "$LOG" 2>&1
  else
    echo "ERROR: BuildCookRun failed for a DIFFERENT reason -- do not assume the M0 wall" >&2
    exit 1
  fi
fi

APP="$(find "$OUT" -maxdepth 3 -name 'Aquarium.app' -print -quit)"
[[ -n "$APP" ]] || { echo "ERROR: no Aquarium.app under $OUT" >&2; exit 1; }
BIN="$APP/Contents/MacOS/Aquarium"
[[ -x "$BIN" ]] || { echo "ERROR: no executable at $BIN" >&2; exit 1; }
codesign -dv "$APP" 2>&1 | head -5

echo "APP=$APP"
echo "PACKAGE_OK config=$CONFIG"
```

```bash
chmod +x /Users/hans/dev/aquarium/scripts/package_mac.sh
```

- [ ] **Step 2: Development 패키지를 만든다** (수십 분 걸릴 수 있다)

```bash
cd /Users/hans/dev/aquarium && CONFIG=Development ./scripts/package_mac.sh 2>&1 | tail -20
```
기대 마지막 줄: `PACKAGE_OK config=Development`, 그 앞에 `APP=/private/tmp/.../Development/Mac/Aquarium.app`.
**실패하면 `/tmp/m6-package-Development.log`의 마지막 200줄을 읽고 설계 문서의 3단 대응 순서로 진행한다. 2단(우회)까지 실패하면 그 사실을 보고하고 멈춘다 — 반쪽 번들로 수치를 내지 않는다.**

- [ ] **Step 3: 패키지가 실제로 실행되는지 확인한다** (헤드리스가 아니라 실제 렌더링)

```bash
APP="$(find /private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package/Development -maxdepth 3 -name 'Aquarium.app' -print -quit)"
rm -f ~/Library/Logs/Aquarium/Aquarium.log
"$APP/Contents/MacOS/Aquarium" ReefM1 -windowed -ResX=1280 -ResY=720 -ForceRes \
  -unattended -nosplash -log -AquariumAssignmentSeed=1 -AquariumAutoNickname="측정" \
  -AquariumAutoExitAfter=20 >/dev/null 2>&1 &
sleep 45; kill %1 2>/dev/null || true; sleep 5
grep -ciE "Failed to compile Material|Default Material|WorldGridMaterial" ~/Library/Logs/Aquarium/Aquarium.log
```
기대: 마지막 줄 **`0`** (규약 5). 0이 아니면 쿡이 머티리얼을 깨뜨린 것이므로 멈추고 보고한다.
**로그 파일 자체가 없으면** `~/Library/Logs/` 아래에서 실제 위치를 찾아 이 계획의 이후 경로를 전부 그에 맞춰 고친다(계획 오류로 보고).

- [ ] **Step 4: 저장소에 패키지가 새어 들어가지 않았는지 확인한다**

```bash
cd /Users/hans/dev/aquarium && git status --short | head -20
du -sh /private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
```
기대: `git status`에 `scripts/package_mac.sh`만(또는 없음). 패키지 디렉터리는 저장소 밖이므로 보이지 않아야 한다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add scripts/package_mac.sh
git commit -m "$(cat <<'EOF'
build: macOS 패키징 스크립트 추가

RunUAT BuildCookRun으로 Development/Shipping을 저장소 밖에 패키징한다.
M0부터 이월된 PostBuildSync(exit 65)에 부딪히면 build + 수동 마무리 + -skipbuild
쿡/스테이징으로 스크립트 안에서 우회한다. 사람이 손으로 치는 단계를 남기지 않는다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 4: 패키지 검증 스크립트 — `scripts/verify_package.sh`

**Files:** Create `scripts/verify_package.sh`

**M6에서 가장 함정이 많은 태스크다.** 여기의 모든 검사는 "없음"을 주장하고, "없음"은 검사가 엉뚱한 곳을 봐도 나온다(규약 7). 그래서 **모든 검사에 대조군이 있다.**

- [ ] **Step 1: 스크립트를 만든다**

`scripts/verify_package.sh` 전체 내용:

```bash
#!/bin/bash
# Asserts what must and must not be inside a packaged build.
#
# Usage:  ./scripts/verify_package.sh <path-to-Aquarium.app> [Development|Shipping]
#
# EVERY check here asserts an ABSENCE, and an absence is also what you get when
# the check is pointed at the wrong file. So every absence check has a control:
#   - MCP:        the same scan run against the EDITOR binaries directory must FAIL
#                 (the editor really does contain ModelContextProtocol).
#   - dev flags:  Shipping must have 0, Development must have >0. A Shipping-only
#                 run proves nothing.
# Run with CONTROL=1 to execute the control runs and report whether they bite.
set -euo pipefail

APP="${1:?usage: verify_package.sh <Aquarium.app> [config]}"
CONFIG="${2:-Development}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$APP/Contents/MacOS/Aquarium"
FAIL=0

say()  { printf '%-34s %s\n' "$1" "$2"; }
bad()  { say "$1" "FAIL  $2"; FAIL=1; }
good() { say "$1" "ok    $2"; }

[[ -x "$BIN" ]] || { echo "no executable at $BIN" >&2; exit 1; }

# ---------- 1. MCP plugin must not be in the package ----------
# Scan every Mach-O and every cooked container inside the bundle.
mcp_hits() {   # $1 = directory to scan
  { grep -ril --include='*' -e 'ModelContextProtocol' "$1" 2>/dev/null
    find "$1" -type f \( -name '*.dylib' -o -name '*.pak' -o -name '*.ucas' -o -name '*.utoc' -o -perm -u+x \) \
      -exec sh -c 'strings -a "$1" 2>/dev/null | grep -qE "ModelContextProtocol|ToolsetRegistry" && echo "$1"' _ {} \; 2>/dev/null
  } | sort -u
}
HITS="$(mcp_hits "$APP" || true)"
if [[ -z "$HITS" ]]; then good "mcp-absent" "no ModelContextProtocol/ToolsetRegistry in bundle"
else bad "mcp-absent" "found in:"; echo "$HITS" | head -10; fi

# ---------- 2. dev-only flags ----------
FLAGS_FILE="${FLAGS_FILE:-$ROOT/.m6-devflags.txt}"
[[ -f "$FLAGS_FILE" ]] || { echo "missing $FLAGS_FILE (Task 0 Step 5 produces it)" >&2; exit 1; }
FOUND=0; FOUND_LIST=""
while read -r f; do
  [[ -n "$f" ]] || continue
  if strings -a "$BIN" 2>/dev/null | grep -qF "$f"; then FOUND=$((FOUND+1)); FOUND_LIST="$FOUND_LIST $f"; fi
done < "$FLAGS_FILE"
TOTAL="$(grep -c . "$FLAGS_FILE")"
if [[ "$CONFIG" == "Shipping" ]]; then
  if [[ $FOUND -eq 0 ]]; then good "devflags-absent" "0 of $TOTAL dev flags in Shipping binary"
  else bad "devflags-absent" "$FOUND still present:$FOUND_LIST"; fi
else
  if [[ $FOUND -gt 0 ]]; then good "devflags-present(control)" "$FOUND of $TOTAL present in Development -- the scan can see flags"
  else bad "devflags-present(control)" "0 of $TOTAL in a DEVELOPMENT binary: the scan is looking at the wrong file, so a Shipping 0 would mean nothing"; fi
fi

# ---------- 3. Korean font must be in the package (F-04) ----------
if find "$APP" -type f \( -name '*.pak' -o -name '*.ucas' -o -name '*.utoc' \) \
     -exec sh -c 'strings -a "$1" 2>/dev/null | grep -qi "NotoSansKR" && echo hit' _ {} \; | grep -q hit; then
  good "font-present" "NotoSansKR found in cooked data"
else
  bad "font-present" "Korean font not found -- F-04 would render tofu"
fi

# ---------- 4. OFL licence copy must ship with the font ----------
if find "$APP" -iname 'OFL.txt' -o -iname '*OFL*' | grep -q .; then
  good "font-licence" "OFL copy present in bundle"
else
  bad "font-licence" "OFL 1.1 requires the licence to travel with the font"
fi

# ---------- 5. no student data ----------
if find "$APP" -type f -newermt '-1 day' -name '*.ini' -o -type f -name '*.sav' 2>/dev/null | grep -q .; then
  say "saved-data" "note: bundle contains ini/sav files -- inspect manually"
fi

echo
if [[ $FAIL -eq 0 ]]; then echo "VERIFY_OK config=$CONFIG"; else echo "VERIFY_FAILED config=$CONFIG"; exit 1; fi
```

```bash
chmod +x /Users/hans/dev/aquarium/scripts/verify_package.sh
cp /private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/devflags.txt /Users/hans/dev/aquarium/.m6-devflags.txt
```

- [ ] **Step 2 (검사가 무는지 증명 1 — MCP): 에디터 디렉터리에 겨눠 빨간불을 본다** (규약 6)

에디터 바이너리에는 MCP 모듈이 **실제로 존재한다.** 같은 스캔이 거기서 빨간불이 나야 그 스캔은 찾을 줄 아는 스캔이다.

```bash
cd /Users/hans/dev/aquarium
grep -ril 'ModelContextProtocol' "/Users/Shared/Epic Games/UE_5.8/Engine/Plugins/Experimental/ModelContextProtocol" 2>/dev/null | head -3
# 스캔 함수만 떼어 에디터 플러그인 디렉터리에 돌린다
bash -c '
D="/Users/Shared/Epic Games/UE_5.8/Engine/Plugins/Experimental/ModelContextProtocol"
H="$(grep -ril "ModelContextProtocol" "$D" 2>/dev/null | head -5)"
[[ -n "$H" ]] && echo "CONTROL_BITES yes" || echo "CONTROL_BITES no"'
```
기대: `CONTROL_BITES yes`.
**`no`가 나오면 이 스캔은 아무것도 못 찾는 스캔이다. 그 상태의 초록불은 증거가 아니므로, 무는 스캔으로 고치거나 "물게 만들 수 없었다"를 후속 항목에 적는다.**

- [ ] **Step 3: Development 패키지에 돌린다**

```bash
cd /Users/hans/dev/aquarium
APP="$(find /private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package/Development -maxdepth 3 -name 'Aquarium.app' -print -quit)"
./scripts/verify_package.sh "$APP" Development
```
기대:
```
mcp-absent                         ok    no ModelContextProtocol/ToolsetRegistry in bundle
devflags-present(control)          ok    N of N present in Development -- the scan can see flags
font-present                       ok    NotoSansKR found in cooked data
font-licence                       ...
VERIFY_OK config=Development
```
**`devflags-present(control)`가 FAIL이면 `strings` 스캔이 엉뚱한 파일을 보고 있는 것이고, 그러면 Task 6의 Shipping 0건은 아무 의미가 없다. 여기서 고친다.**
`font-licence`가 FAIL이면 Task 8에서 닫는다(예상된 실패다).

- [ ] **Step 4 (검사가 무는지 증명 2 — 포트): 실제 실행 중 8000 포트를 본다**

```bash
APP="$(find /private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package/Development -maxdepth 3 -name 'Aquarium.app' -print -quit)"
"$APP/Contents/MacOS/Aquarium" ReefM1 -windowed -ResX=1280 -ResY=720 -ForceRes \
  -unattended -nosplash -AquariumAssignmentSeed=1 -AquariumAutoNickname="측정" >/dev/null 2>&1 &
GP=$!
for i in 1 2 3 4 5 6; do sleep 10; echo -n "t=$((i*10))s: "; lsof -nP -iTCP:8000 -sTCP:LISTEN | wc -l; done
kill $GP 2>/dev/null || true
```
기대: 여섯 줄 모두 `0`.
**대조군:** 같은 명령을 **에디터 GUI**에 대해 돌리면 35~65초 후 1행 이상이 나온다(`docs/SETUP.md`의 MCP 절에 기록됨). 시간이 허락하면 한 번 돌려 `lsof`가 실제로 감지할 줄 아는지 확인한다. **포트 검사는 약한 검사이므로 파일 검사가 초록불인 것이 1차 근거다** — 이 사실을 보고서에 적는다.

- [ ] **Step 5: `.m6-devflags.txt`를 저장소에 남길지 결정한다**

남긴다. 검사의 입력이고 소스에서 파생된 것이므로 **검사와 함께 커밋한다.** 단 재생성 명령을 스크립트 주석에 적어 둔다(이미 적혀 있다).

- [ ] **Step 6: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add scripts/verify_package.sh .m6-devflags.txt
git commit -m "$(cat <<'EOF'
test: 패키지 검증 스크립트 추가 (MCP 배제·개발 플래그·폰트·라이선스)

모든 검사가 "없음"을 주장하므로 전부 대조군을 함께 돈다.
MCP 스캔은 에디터 플러그인 디렉터리에서 빨간불이 나는 것을 확인했고,
개발 플래그 스캔은 Development 패키지에서 비0건이 나오는 것을 확인한다.
대조군 없는 0건은 PropMaterialsCompile의 "checked 0 of 13"과 같은 통과다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 5: 별명이 디스크에 닿지 않는지 검사 — `scripts/verify_privacy.sh`

**Files:** Create `scripts/verify_privacy.sh`

SRS 52행과 `AGENTS.md`의 하드 요구다. **지금까지의 근거는 "그렇게 짰다"뿐이고, M6이 실제 패키지를 돌린 뒤 디스크를 뒤진다.**

- [ ] **Step 1: 스크립트를 만든다**

`scripts/verify_privacy.sh` 전체 내용:

```bash
#!/bin/bash
# Checks that a nickname never reaches disk. SRS line 52 / AGENTS.md.
#
# TWO MODES, and the distinction is the whole point:
#
#   auto  -- runs the packaged Development build with -AquariumAutoNickname=<token>.
#            The engine echoes the WHOLE COMMAND LINE into its log, so the token
#            WILL be found. Asserting "0 hits" here would be a check that can only
#            ever be red, and the usual response to that is to weaken it until it
#            checks nothing. So this mode asserts something else and true:
#            EVERY hit must be on a command-line echo line. A hit anywhere else
#            means the GAME wrote the nickname, which is the defect.
#
#   manual -- the user typed the nickname into the packaged app by hand, so the
#            command line never carried it. Here 0 hits is the correct and
#            meaningful assertion. THIS is the honest answer to SRS line 52.
#
# Run: ./scripts/verify_privacy.sh auto <Aquarium.app>
#      ./scripts/verify_privacy.sh manual <token-the-user-typed>
set -euo pipefail

MODE="${1:?usage: verify_privacy.sh auto <app> | manual <token>}"
TOKEN_DEFAULT="별명검사토큰QX7"
FAIL=0

scan() {  # $1 = token
  local t="$1"
  { grep -rl -a -- "$t" "$HOME/Library/Logs/Aquarium" 2>/dev/null
    grep -rl -a -- "$t" "$HOME/Library/Application Support/Epic" 2>/dev/null
    grep -rl -a -- "$t" "$HOME/Library/Preferences" 2>/dev/null
  } | sort -u
}

case "$MODE" in
auto)
  APP="${2:?usage: verify_privacy.sh auto <Aquarium.app>}"
  TOKEN="${TOKEN:-$TOKEN_DEFAULT}"
  rm -rf "$HOME/Library/Logs/Aquarium"
  "$APP/Contents/MacOS/Aquarium" ReefM1 -windowed -ResX=1280 -ResY=720 -ForceRes \
    -unattended -nosplash -log -AquariumAssignmentSeed=1 \
    -AquariumAutoNickname="$TOKEN" -AquariumAutoExitAfter=10 >/dev/null 2>&1 &
  sleep 40; kill %1 2>/dev/null || true; sleep 5

  FILES="$(scan "$TOKEN")"
  echo "files containing the token:"; echo "${FILES:-  (none)}"
  TOTAL=0; ECHO_LINES=0
  for f in $FILES; do
    n=$(grep -a -c -- "$TOKEN" "$f" || true)
    e=$(grep -a -- "$TOKEN" "$f" | grep -c -- '-AquariumAutoNickname=' || true)
    TOTAL=$((TOTAL+n)); ECHO_LINES=$((ECHO_LINES+e))
    echo "  $f: $n hits, $e on command-line echo lines"
    grep -a -n -- "$TOKEN" "$f" | grep -v -- '-AquariumAutoNickname=' | head -5
  done
  echo "AUTO_TOTAL=$TOTAL AUTO_ECHO=$ECHO_LINES"
  if [[ $TOTAL -eq 0 ]]; then
    echo "FAIL: the token was not found even on the command-line echo line."
    echo "      That means this scan is looking in the wrong place, so a clean"
    echo "      manual run would prove nothing. Fix the search paths."
    FAIL=1
  elif [[ $TOTAL -ne $ECHO_LINES ]]; then
    echo "FAIL: the nickname appears OUTSIDE the command-line echo -- the game wrote it."
    FAIL=1
  else
    echo "ok: every occurrence is the engine echoing its own command line."
  fi
  ;;
manual)
  TOKEN="${2:?usage: verify_privacy.sh manual <token-the-user-typed>}"
  FILES="$(scan "$TOKEN")"
  if [[ -z "$FILES" ]]; then
    echo "PRIVACY_OK: '$TOKEN' appears in no log, config or preference file."
  else
    echo "FAIL: the hand-typed nickname reached disk:"; echo "$FILES"
    for f in $FILES; do grep -a -n -- "$TOKEN" "$f" | head -5; done
    FAIL=1
  fi
  ;;
*) echo "unknown mode $MODE" >&2; exit 1;;
esac

[[ $FAIL -eq 0 ]] || exit 1
```

```bash
chmod +x /Users/hans/dev/aquarium/scripts/verify_privacy.sh
```

- [ ] **Step 2 (RED — 검사가 무는지 증명): 별명을 찍는 로그를 임시로 넣는다** (규약 6)

지금 코드에는 별명을 찍는 `UE_LOG`가 **한 줄도 없다.** 그대로 검사를 돌리면 아무것도 없는 것을 못 찾고 통과한다 — `checked 0 of 13`과 같은 모양이다. 그래서 먼저 빨간불을 본다.

`unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp`에서 세션을 시작하는 지점(별명이 확정되는 곳)을 찾는다.

```bash
cd /Users/hans/dev/aquarium
grep -n "Nickname" unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp | head -20
```

찾은 함수 본문에 **임시로** 아래 한 줄을 넣는다(들여쓰기는 주변에 맞춘다):

```cpp
	UE_LOG(LogTemp, Warning, TEXT("M6 RED PROBE nickname=%s"), *Nickname);   // TEMPORARY -- revert
```

에디터 빌드로 확인한다(패키지를 다시 만들 필요는 없다 — 검사가 로그 파일을 읽을 줄 아는지가 질문이다).

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:"; done
rm -rf ~/Library/Logs/Aquarium
"$UE/Engine/Binaries/Mac/UnrealEditor" "$PWD/unreal/Aquarium/Aquarium.uproject" ReefM1 \
  -game -windowed -ResX=1280 -ResY=720 -ForceRes -unattended -nosplash -log \
  -AquariumAssignmentSeed=1 -AquariumAutoNickname="별명검사토큰QX7" -AquariumAutoExitAfter=8 >/dev/null 2>&1 &
sleep 45; kill %1 2>/dev/null || true; sleep 5
grep -a -n "별명검사토큰QX7" ~/Library/Logs/Aquarium/Aquarium.log | grep -v -- '-AquariumAutoNickname=' | head
```
기대: **`M6 RED PROBE nickname=별명검사토큰QX7` 줄이 나온다.** 즉 "에코 줄이 아닌 곳에서 발견" = 검사가 문다.
**나오지 않으면** 로그 경로나 grep이 틀린 것이므로 그것부터 고친다. 이 확인 없이는 이후의 초록불에 의미가 없다.

- [ ] **Step 3: 임시 로그를 되돌리고 원상 복구를 확인한다**

```bash
cd /Users/hans/dev/aquarium
git checkout -- unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp
git status --short unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp
grep -c "M6 RED PROBE" unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp
```
기대: `git status` 출력 없음, 마지막 줄 `0`.

- [ ] **Step 4 (GREEN): 패키지에 자동 모드를 돌린다**

```bash
cd /Users/hans/dev/aquarium
APP="$(find /private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package/Development -maxdepth 3 -name 'Aquarium.app' -print -quit)"
./scripts/verify_privacy.sh auto "$APP"
```
기대: `AUTO_TOTAL=N AUTO_ECHO=N` (둘이 같고 N ≥ 1), 그리고
`ok: every occurrence is the engine echoing its own command line.`
- `AUTO_TOTAL=0`이면 **검사가 엉뚱한 곳을 보고 있다.** 고친다.
- `AUTO_TOTAL != AUTO_ECHO`이면 **게임이 별명을 썼다.** 결함이므로 보고하고 고친다.

- [ ] **Step 5: 수동 모드는 Task 9의 인계 목록으로 넘긴다** — 여기서는 명령만 확인한다.

```bash
cd /Users/hans/dev/aquarium && ./scripts/verify_privacy.sh manual "존재하지않는토큰ZZZ9"
```
기대: `PRIVACY_OK: '존재하지않는토큰ZZZ9' appears in no log, config or preference file.`
(이 실행은 **검사가 돌아간다**는 것만 보이며, 판정이 아니다. 판정은 사용자가 직접 타이핑한 세션 뒤에 한다.)

- [ ] **Step 6: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add scripts/verify_privacy.sh
git commit -m "$(cat <<'EOF'
test: 별명이 디스크에 닿지 않는지 검사하는 스크립트 추가

엔진이 명령줄 전체를 로그에 찍으므로 개발 플래그로 별명을 넣은 실행에서
0건을 요구하면 영원히 빨간불이 된다. 그래서 자동 모드는 "모든 발견이
명령줄 에코 줄인가"를 단언하고, 판정은 사용자가 손으로 타이핑한 세션에
대한 수동 모드(0건)가 한다.
임시 UE_LOG 한 줄로 검사가 실제로 무는 것을 먼저 확인하고 되돌렸다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 6: Shipping 패키지와 개발 플래그 배제 판정

**Files:** 없음 (스크립트 재사용)

- [ ] **Step 1: Shipping 패키지를 만든다**

```bash
cd /Users/hans/dev/aquarium && CONFIG=Shipping ./scripts/package_mac.sh 2>&1 | tail -20
```
기대: `PACKAGE_OK config=Shipping`.
**Shipping 타깃은 이 프로젝트에서 한 번도 컴파일된 적이 없다.** 컴파일 오류(예: `#if !UE_BUILD_SHIPPING` 밖에서 개발 전용 심볼을 참조)가 나면 **그것 자체가 M6이 잡아낸 결함**이다. 고치고 규칙/Automation을 다시 돌린 뒤 계속한다.

- [ ] **Step 2: 대조 검증 — Shipping 0건 / Development 비0건**

```bash
cd /Users/hans/dev/aquarium
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
DEV="$(find "$B/Development" -maxdepth 3 -name 'Aquarium.app' -print -quit)"
SHIP="$(find "$B/Shipping" -maxdepth 3 -name 'Aquarium.app' -print -quit)"
echo "--- Development (대조군: 플래그가 있어야 한다)"; ./scripts/verify_package.sh "$DEV"  Development
echo "--- Shipping    (판정: 플래그가 없어야 한다)";   ./scripts/verify_package.sh "$SHIP" Shipping
```
기대:
```
--- Development
devflags-present(control)          ok    11 of 11 present in Development -- the scan can see flags
--- Shipping
devflags-absent                    ok    0 of 11 dev flags in Shipping binary
```
**Development 쪽이 0이면 Shipping의 0은 아무것도 뜻하지 않는다.** 그 경우 `strings` 대상 파일을 다시 잡는다(예: `Contents/MacOS/Aquarium`이 얇은 런처이고 실제 코드가 `Contents/UE/...` 아래일 수 있다). 다음으로 확인한다:

```bash
file "$DEV/Contents/MacOS/Aquarium"; du -sh "$DEV/Contents/MacOS/Aquarium"
find "$DEV" -name '*.dylib' | head
```

- [ ] **Step 3: Shipping 패키지에도 MCP가 없는지 확인한다** — Step 2의 출력에 `mcp-absent ok`가 포함되어 있어야 한다. 없으면 멈추고 보고한다.

- [ ] **Step 4: Shipping 빌드가 실행은 되는지 확인한다**

```bash
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
SHIP="$(find "$B/Shipping" -maxdepth 3 -name 'Aquarium.app' -print -quit)"
open "$SHIP"; sleep 20; pgrep -f "Aquarium" | head; pkill -f "$SHIP" || true
```
기대: 프로세스가 뜬다. **Shipping은 개발 플래그가 없으므로 자동 입장이 불가능하고, 화면 판정은 사용자 수동 확인(Task 9)으로 넘긴다.** 그것이 옳다 — 아이가 받는 빌드에 자동화 구멍이 없다는 뜻이다.

- [ ] **Step 5: 커밋** (코드 변경이 없으면 Step 1에서 고친 것이 있을 때만 커밋한다)

```bash
cd /Users/hans/dev/aquarium
git status --short
# 변경이 있으면:
git commit -am "$(cat <<'EOF'
fix(unreal): Shipping 구성 빌드 오류 수정

Shipping 타깃은 M6에서 처음 컴파일했다. 수정 내용과 근거는 커밋 본문에 적는다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 7: SRS 공식 성능 측정 — 패키지 개발 빌드

**Files:** Create `scripts/measure_package_perf.sh`, Create `docs/reviews/2026-09-21-m6-perf.md`

**이 프로젝트의 첫 공식 성능 판정이다.** 지금까지의 모든 수치는 에디터 빌드였다.

- [ ] **Step 1: 스크립트를 만든다** — `measure_m2b_perf.sh`와 같은 구조이되 **패키지 바이너리를 돌리고 SRS 길이를 기본값으로 한다.**

`scripts/measure_package_perf.sh` 전체 내용:

```bash
#!/bin/bash
# THE official SRS performance measurement: a PACKAGED DEVELOPMENT build,
# 30 s warm-up then 3 minutes (SRS line 38). Every earlier figure in this repo
# came from an editor build and is NOT an SRS verdict.
#
# measure_m2b_perf.sh defaults to RUN_SEC=95, which leaves only 65 s of samples
# after the warm-up. That is short of the SRS three minutes, which is why this
# script exists with RUN_SEC=210 baked in.
#
# -benchmark is deliberately NOT passed: it fixes the timestep and would make
# DeltaSeconds a useless constant.
#
# The nickname is TEST DATA ONLY -- the engine echoes the command line to its log.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:?set APP=/path/to/Aquarium.app}"
BIN="$APP/Contents/MacOS/Aquarium"
MAP="${MAP:-ReefM1}"
RUN_SEC="${RUN_SEC:-210}"          # 30 s warm-up + 180 s measured
WARMUP_SEC="${WARMUP_SEC:-30}"
RESX="${RESX:-1920}"; RESY="${RESY:-1080}"
LABEL="${LABEL:-m6}"
DATE="$(date +%F)"
CSV="$ROOT/docs/reviews/$DATE-$LABEL-frametimes.csv"

[[ -x "$BIN" ]] || { echo "no executable at $BIN" >&2; exit 1; }
[[ $RUN_SEC -ge $((WARMUP_SEC + 180)) ]] || { echo "ERROR: RUN_SEC=$RUN_SEC is shorter than SRS warm-up+3min" >&2; exit 1; }

mkdir -p "$ROOT/docs/reviews"; rm -f "$CSV"
"$BIN" "$MAP" -windowed -ResX="$RESX" -ResY="$RESY" -ForceRes \
  -notexturestreaming -unattended -nosplash -log \
  -AquariumAutoNickname="측정" -AquariumAssignmentSeed=1 \
  -AquariumFrameStats="$CSV" ${EXTRA_ARGS:-} >/dev/null 2>&1 &
PID=$!
echo "measuring ${RUN_SEC}s (pid $PID)"
for (( i = 0; i < RUN_SEC; i++ )); do kill -0 "$PID" 2>/dev/null || break; sleep 1; done
kill "$PID" 2>/dev/null || true
for (( i = 0; i < 30; i++ )); do kill -0 "$PID" 2>/dev/null || break; sleep 1; done
kill -9 "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true

[[ -f "$CSV" ]] || { echo "ERROR: no frame-time CSV at $CSV" >&2; exit 1; }

CSV="$CSV" WARMUP_SEC="$WARMUP_SEC" python3 - <<'PY'
import csv, os, statistics
rows = [float(r[-1]) for r in csv.reader(open(os.environ['CSV'])) if r and r[-1].replace('.','',1).replace('e-','',1).isdigit()]
warm = float(os.environ['WARMUP_SEC']); t = 0.0; kept = []
for d in rows:
    t += d
    if t >= warm: kept.append(d)
if not kept: raise SystemExit('ERROR: no samples after warm-up -- the run was too short')
kept.sort()
p95 = kept[int(len(kept) * 0.95)] * 1000.0
avg = len(kept) / sum(kept)
print(f'SAMPLES={len(kept)} MEASURED_SEC={sum(kept):.1f} AVG_FPS={avg:.2f} P95_MS={p95:.2f}')
print('SRS_GATE=' + ('PASS' if avg >= 60.0 and p95 <= 22.0 else 'FAIL'))
PY
```

```bash
chmod +x /Users/hans/dev/aquarium/scripts/measure_package_perf.sh
```

- [ ] **Step 2: 스크립트가 짧은 실행을 거부하는지 확인한다** (이 가드가 무는지)

```bash
cd /Users/hans/dev/aquarium
APP=/nonexistent.app RUN_SEC=95 ./scripts/measure_package_perf.sh; echo "exit=$?"
```
기대: `no executable at ...`로 먼저 죽는다. 실제 앱 경로로 다시:

```bash
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
APP="$(find "$B/Development" -maxdepth 3 -name 'Aquarium.app' -print -quit)" RUN_SEC=95 \
  ./scripts/measure_package_perf.sh; echo "exit=$?"
```
기대: `ERROR: RUN_SEC=95 is shorter than SRS warm-up+3min`, `exit=1`.
**이 가드가 안 물면 M6의 공식 측정이 조용히 65초짜리가 된다.**

- [ ] **Step 3: 공식 측정 1회차**

```bash
cd /Users/hans/dev/aquarium
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
APP="$(find "$B/Development" -maxdepth 3 -name 'Aquarium.app' -print -quit)" LABEL=m6-run1 \
  ./scripts/measure_package_perf.sh
```
기대: `SAMPLES=… MEASURED_SEC=18x.x AVG_FPS=… P95_MS=… SRS_GATE=PASS|FAIL`.
**`MEASURED_SEC`이 180 미만이면 측정이 SRS 규약을 못 채운 것이므로 다시 돌린다.**

- [ ] **Step 4: 공식 측정 2회차** — 노이즈 바닥 1.3 fps 때문에 1회로는 판정하지 않는다.

```bash
cd /Users/hans/dev/aquarium
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
APP="$(find "$B/Development" -maxdepth 3 -name 'Aquarium.app' -print -quit)" LABEL=m6-run2 \
  ./scripts/measure_package_perf.sh
```

- [ ] **Step 5: 판정한다 — 기계적으로**

```
두 실행의 평균 A, p95의 큰 쪽 P
A >= 60.0 AND P <= 22.0  → SRS 통과. 예비 레버를 쓰지 않는다. Step 7로.
그 외                     → Step 6으로.
```
**두 실행 중 하나라도 60 fps 아래면 평균이 넘더라도 그 사실을 보고서에 적는다.** 숨기지 않는다.

- [ ] **Step 6 (조건부): 예비 레버** — **Step 5가 FAIL일 때만 실행한다. PASS면 이 스텝을 건너뛴다.**

`unreal/Aquarium/Config/DefaultEngine.ini`에서 `r.VolumetricFog.GridSizeZ=128`을 `64`로 바꾼다(XY의 `GridPixelSize=4`는 **건드리지 않는다**). 그 다음 **다시 패키징하고 다시 잰다.**

```bash
cd /Users/hans/dev/aquarium
grep -n "VolumetricFog" unreal/Aquarium/Config/DefaultEngine.ini
# GridSizeZ=128 -> 64 로 수정
CONFIG=Development ./scripts/package_mac.sh 2>&1 | tail -3
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
APP="$(find "$B/Development" -maxdepth 3 -name 'Aquarium.app' -print -quit)" LABEL=m6-lever1 ./scripts/measure_package_perf.sh
APP="$(find "$B/Development" -maxdepth 3 -name 'Aquarium.app' -print -quit)" LABEL=m6-lever2 ./scripts/measure_package_perf.sh
```
그래도 미달이면 순서대로: (2) 부유 입자 커튼 3 → 1(`AQ_DROP_CURTAINS=near`로 레벨 재빌드), (3) SSAO 끄기(`AQ_NO_SSAO=1`). **하나 되돌릴 때마다 다시 잰다.** 레벨을 바꾸면 `build_reef_m1.py`를 규약 4의 형태로 돌리고 `REEF_OK`를 grep한다.

- [ ] **Step 7: 보고서를 쓴다** — `docs/reviews/2026-09-21-m6-perf.md`

```markdown
# M6 성능 — SRS 공식 판정 (패키지 개발 빌드)

작성일: 2026-09-21 · 커밋: <short sha>

**이것이 이 프로젝트의 첫 SRS 공식 성능 판정이다.** M1~M5의 모든 수치는 에디터 빌드에서 나왔고,
SRS 38행은 "패키징한 개발 빌드에서 30초 예열 후 3분"을 요구한다.

## 측정 조건
| 항목 | 값 |
|---|---|
| 빌드 | 패키지 **Development** (`RunUAT BuildCookRun`), 쿡·pak 적용 |
| 기기 | <sysctl machdep.cpu.brand_string> |
| 해상도 | 1920×1080 내부 = 출력, 업스케일 없음, `-ForceRes` |
| 그래픽 설정 | 커밋된 `DefaultEngine.ini` 그대로 (`GridPixelSize=4`, `GridSizeZ=<128 또는 64>`) |
| 장면 | `ReefM1` — 제어 1 + 배경 36 + 프롭 22 |
| 예열 / 측정 | 30초 / 180초, 2회 |

**장면이 SRS 37행보다 무겁다.** SRS는 "제어 1 + 배경 19"를 적었고 실제는 배경 36마리다
(M2b에서 PRD의 30~40마리 요구를 따랐다). 게이트를 유리하게 바꾸지 않았다는 뜻이므로 그대로 판정한다.

## 결과
| 실행 | 평균 fps | p95 프레임 시간 | 샘플 |
|---|---|---|---|
| 1회차 | … | … ms | … |
| 2회차 | … | … ms | … |
| **평균** | **…** | **…** | |

## SRS 게이트 판정
| 기준 | 목표 | 실측 | 판정 |
|---|---|---|---|
| 평균 fps | ≥ 60 | … | … |
| p95 프레임 시간 | ≤ 22 ms | … | … |

**판정: 통과 / 미달**

## 에디터 빌드와의 관계
M5의 63.50 fps는 **에디터 빌드**이며 쿡·셰이더·텍스처 스트리밍 설정이 모두 달라 직접 비교할 수
없다. 참고로만 적고 게이트 판정에는 쓰지 않는다.

## 예비 레버
`r.VolumetricFog.GridSizeZ` 128 → 64 (M4b 측정 +17.1 fps)는 M4b·M4c·M5에서 일부러 아껴 두었다.
**사용 여부: 썼다 / 쓰지 않았다** — <이유: 평균이 60 fps를 넘어 쓸 필요가 없었다 / 미달이라 적용했다>

## 노이즈
실행 간 노이즈 바닥은 약 1.3 fps. 두 실행의 차이는 … fps로 <노이즈 바닥 이하 / 이상>이다.
```

- [ ] **Step 8: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add scripts/measure_package_perf.sh docs/reviews/2026-09-21-m6-perf.md
git status --short   # frametimes CSV는 .gitignore로 제외되어 있어야 한다
git commit -m "$(cat <<'EOF'
perf: SRS 공식 성능 판정 — 패키지 개발 빌드 30초 예열 + 3분

measure_m2b_perf.sh의 기본 RUN_SEC=95는 예열을 빼면 65초뿐이라 SRS의 3분에
미달한다. 전용 스크립트를 만들고 warm-up+180초 미만을 거부하는 가드를 넣었다.
이 저장소의 첫 SRS 공식 판정이다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 8: 에셋 출처 — 배포물 기준 확인과 OFL 사본 동봉

**Files:** Modify `docs/ASSETS.md`, 필요 시 Modify `unreal/Aquarium/Scripts/import_fonts.py` 또는 패키징 설정

M4a~M5의 확인은 전부 `git diff`였고 그것은 **"우리가 원본을 건드렸는가"**만 답한다. M6의 질문은 **"배포물에 무엇이 들어갔는가"**다.

- [ ] **Step 1: 저장소 원본 불변 확인** (기존 규약)

```bash
cd /Users/hans/dev/aquarium
git diff --stat main...HEAD -- assets/models assets/textures assets/fonts
```
기대: 출력 없음.

- [ ] **Step 2: 배포물의 에셋 목록을 뽑는다**

```bash
UE="/Users/Shared/Epic Games/UE_5.8"
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
SCRATCH=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad
APP="$(find "$B/Shipping" -maxdepth 3 -name 'Aquarium.app' -print -quit)"
PAK="$(find "$APP" -name '*.pak' -o -name '*.utoc' | head -1)"
echo "container: $PAK"
"$UE/Engine/Binaries/Mac/UnrealPak" "$PAK" -List 2>/dev/null | tee "$SCRATCH/pak-list.txt" | head -20
grep -c . "$SCRATCH/pak-list.txt"
```
`UnrealPak -List`가 IoStore 컨테이너에서 실패하면 대신:

```bash
strings -a "$PAK" | grep -oE '/Game/[A-Za-z0-9_/]+' | sort -u > "$SCRATCH/pak-list.txt"
wc -l < "$SCRATCH/pak-list.txt"
```
기대: **0이 아닌 목록.** 0이면 엉뚱한 파일을 본 것이다(규약 7).

- [ ] **Step 3: 목록을 `docs/ASSETS.md`와 대조한다**

```bash
SCRATCH=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad
grep -oiE 'BlueTang|Clownfish|YellowTang|Butterflyfish|Damselfish|BranchCoral|PlateCoral|BrainCoral|FanCoral|TubeCoral|boulder_01|rock_07|rock_09|coast_sand|NotoSansKR' \
  "$SCRATCH/pak-list.txt" | sort | uniq -c
```
기대: **15개 이름이 모두 1건 이상.** 하나라도 0이면 (a) 쿡에서 빠졌거나 (b) 목록 추출이 부실한 것이다. 어느 쪽인지 확인한 뒤 기록한다.
**반대 방향도 본다** — 목록에 있는데 `ASSETS.md`에 없는 **외부 유래** 에셋이 있는지:

```bash
grep -oE '/Game/[A-Za-z0-9_]+/[A-Za-z0-9_]+' "$SCRATCH/pak-list.txt" | sort -u | head -60
```
프로젝트가 만든 것(`/Game/Fish`, `/Game/Props`, `/Game/Maps`, `/Game/UI` 등)과 엔진 기본 콘텐츠 외에 출처 불명이 있으면 기록하고 조사한다.

- [ ] **Step 4: OFL 사본을 패키지에 동봉한다**

Task 4 Step 3에서 `font-licence`가 FAIL이었다면(예상된 실패) 닫는다. `unreal/Aquarium/Config/DefaultGame.ini`의 스테이징 설정에 라이선스 파일을 추가한다.

```bash
cd /Users/hans/dev/aquarium
mkdir -p unreal/Aquarium/Content/Licenses
cp assets/fonts/OFL.txt unreal/Aquarium/Content/Licenses/OFL-NotoSansKR.txt
grep -n "DirectoriesToAlwaysStageAsNonUFS\|\[/Script/UnrealEd.ProjectPackagingSettings\]" unreal/Aquarium/Config/DefaultGame.ini
```

`[/Script/UnrealEd.ProjectPackagingSettings]` 절(없으면 새로 만든다)에 다음을 추가한다:

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysStageAsNonUFS=(Path="Licenses")
```

그리고 `README.md`에 출처 표기 한 줄을 넣는다:

```markdown
## 에셋 출처

이 게임은 Noto Sans KR(SIL OFL 1.1), Poly Haven CC0 에셋(모래 텍스처·바위 3종)을 사용한다.
라이선스 전문은 패키지의 `Licenses/` 폴더와 `docs/ASSETS.md`에 있다.
```

- [ ] **Step 5: 재패키징하고 라이선스 검사가 초록불이 되는지 확인한다**

```bash
cd /Users/hans/dev/aquarium && CONFIG=Shipping ./scripts/package_mac.sh 2>&1 | tail -3
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
./scripts/verify_package.sh "$(find "$B/Shipping" -maxdepth 3 -name 'Aquarium.app' -print -quit)" Shipping
```
기대: `font-licence  ok`, `VERIFY_OK config=Shipping`.
**Step 4 전에 이 검사가 FAIL이었다는 사실이 곧 이 검사가 문다는 증거다.** 그렇게 기록한다.

- [ ] **Step 6: `docs/ASSETS.md`에 M6 절을 추가한다** — "도입 시 필수 기록" 절 **앞**에 넣는다.

```markdown
M6 확인(2026-09-21) — **배포물 기준**: M4a~M5의 확인은 `git diff`로 "저장소 원본을 건드리지 않았다"까지만
말한다. M6은 다른 질문에 답한다 — **패키지 안에 무엇이 들어갔는가.**

| 검사 | 명령 | 결과 |
|---|---|---|
| 저장소 원본 불변 | `git diff --stat main...HEAD -- assets/models assets/textures assets/fonts` | 출력 없음 |
| 배포물 에셋 목록 | `UnrealPak -List` (실패 시 `strings` + `/Game/` 추출) | <N>개 경로 |
| 위 표 15개 에셋 이름이 배포물에 존재 | 목록 grep | 전부 1건 이상 |
| 출처 미기재 외부 에셋 | 목록에서 `/Game/` 항목 전수 확인 | 0건 |
| **OFL 1.1 사본 동봉** | `verify_package.sh`의 `font-licence` | **M6에서 처음 닫았다** — `Content/Licenses/OFL-NotoSansKR.txt`를 NonUFS로 스테이징한다 |

**OFL 항목은 M6에서 처음 걸린 실질적 의무다.** 이 문서는 "OFL: 임베드·재배포 허용"을 옳게 적었지만
OFL 1.1은 사본 동반도 요구하고, 그 전까지 패키지에는 아무것도 들어가지 않았다. 검사는 수정 전 FAIL,
수정 후 ok였다 — 즉 이 검사는 무는 검사다.

M6은 에셋을 새로 만들거나 바꾸지 않았다. 따라서 위 두 표는 행·열 모두 M5 시점 그대로 유효하다.
```

- [ ] **Step 7: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add docs/ASSETS.md README.md unreal/Aquarium/Config/DefaultGame.ini unreal/Aquarium/Content/Licenses
git commit -m "$(cat <<'EOF'
docs: 배포물 기준 에셋 출처 확인과 OFL 사본 동봉

M4a~M5의 git diff 확인은 "원본을 건드렸는가"까지만 답한다. M6은 패키지 안에
무엇이 들어갔는지를 확인한다. OFL 1.1이 요구하는 라이선스 사본 동반은
그동안 빠져 있었고 M6에서 닫았다(수정 전 검사 FAIL, 수정 후 ok).

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 9: 패키지 화면 검토 산출물

**Files:** Create `scripts/render_m6_package.sh`

쿡은 셰이더·텍스처 압축·LOD를 바꾼다. "패키지에서도 M5와 같아 보이는가"는 헤드리스로 답할 수 없고, 이 프로젝트에서 머티리얼이 조용히 회색으로 떨어진 전례가 두 번 있다.

- [ ] **Step 1: 스크립트를 만든다** — `render_m5_click.sh`와 같은 규약이되 **패키지 바이너리**를 쓴다.

`scripts/render_m6_package.sh` 전체 내용:

```bash
#!/bin/bash
# M6 review artefacts, rendered from the PACKAGED build rather than the editor.
# Cooking changes shaders, texture compression and LODs, so "does it still look
# right" is a question only a real packaged run can answer -- and materials in
# this project have silently fallen back to grey twice.
#
# Same map, seed, camera and auto-input as the M5 clip so the comparison is fair.
# Auto-input token syntax: <direction-letter><seconds>, letters R/L/U/D/0, no
# colons, no diagonals. The parser only WARNS on junk, so this script asserts
# both warning counts are zero.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:?set APP=/path/to/Aquarium.app}"
BIN="$APP/Contents/MacOS/Aquarium"
FFMPEG="${FFMPEG:-/opt/homebrew/bin/ffmpeg}"
REVIEWS="$ROOT/docs/reviews"
LOG="${LOG:-/tmp/m6-capture.log}"
MAP="${MAP:-ReefM1}"
FPS="${FPS:-30}"
SECS="${SECS:-30}"
DATE="$(date +%F)"
BEFORE="${BEFORE:-$REVIEWS/2026-09-21-m5-flee.png}"
AUTO_INPUT="${AUTO_INPUT:-R3,U2,L3,D2,0 2,R2,U2,0 2,0 14}"
AUTO_NICKNAME="${AUTO_NICKNAME:-니모}"   # TEST DATA ONLY -- echoed into the log

case "$BEFORE" in *m6*) echo "ERROR: BEFORE points at an m6 artefact -- that compares M6 with itself" >&2; exit 1;; esac
[[ -f "$BEFORE" ]] || { echo "ERROR: no BEFORE still at $BEFORE" >&2; exit 1; }

FRAMES="$(dirname "$APP")/AquariumFrames"; rm -rf "$FRAMES"; mkdir -p "$FRAMES"
rm -rf "$HOME/Library/Logs/Aquarium"

"$BIN" "$MAP" -windowed -ResX=1920 -ResY=1080 -ForceRes \
  -benchmark -fps="$FPS" -seconds="$SECS" -notexturestreaming \
  -unattended -nosplash -log \
  -AquariumAutoNickname="$AUTO_NICKNAME" -AquariumAssignmentSeed=1 \
  -AquariumAutoInput="$AUTO_INPUT" -AquariumCaptureUI="$FRAMES" \
  > "$LOG" 2>&1 || true

GLOG="$HOME/Library/Logs/Aquarium/Aquarium.log"
MATERIAL=$(grep -ciE "Failed to compile Material|Default Material|WorldGridMaterial" "$GLOG" 2>/dev/null || echo 0)
IN_UNK=$(grep -c "AquariumAutoInput: unknown direction" "$GLOG" 2>/dev/null || echo 0)
IN_BAD=$(grep -c "AquariumAutoInput: bad duration" "$GLOG" 2>/dev/null || echo 0)
NFRAMES=$(ls "$FRAMES" | wc -l | tr -d ' ')
echo "MATERIAL_COMPILE_FAILURES=$MATERIAL AUTOINPUT_UNKNOWN=$IN_UNK AUTOINPUT_BAD_DURATION=$IN_BAD FRAMES=$NFRAMES"
[[ "$MATERIAL" == "0" ]] || { echo "ERROR: materials failed to compile in the PACKAGED build" >&2; exit 1; }
[[ "$IN_UNK" == "0" && "$IN_BAD" == "0" ]] || { echo "ERROR: auto-input script was silently truncated" >&2; exit 1; }
[[ "$NFRAMES" -gt $((FPS * SECS / 2)) ]] || { echo "ERROR: only $NFRAMES frames captured" >&2; exit 1; }

mkdir -p "$REVIEWS"
"$FFMPEG" -y -framerate "$FPS" -pattern_type glob -i "$FRAMES/*.png" \
  -c:v libx264 -pix_fmt yuv420p "$REVIEWS/$DATE-m6-package.mp4" >/dev/null 2>&1
echo "VIDEO_OK"
STILL="$(ls "$FRAMES"/*.png | sed -n "$((FPS * 12))p")"
cp "$STILL" "$REVIEWS/$DATE-m6-scene.png"; echo "STILL_OK"
"$FFMPEG" -y -i "$BEFORE" -i "$REVIEWS/$DATE-m6-scene.png" -filter_complex hstack \
  "$REVIEWS/$DATE-m6-compare.png" >/dev/null 2>&1
echo "COMPARE_OK before=$BEFORE"
```

```bash
chmod +x /Users/hans/dev/aquarium/scripts/render_m6_package.sh
```

- [ ] **Step 2: 돌린다**

```bash
cd /Users/hans/dev/aquarium
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m6_package
ls docs/reviews/ | grep m5     # BEFORE로 쓸 M5 스틸의 실제 이름을 확인한다
APP="$(find "$B/Development" -maxdepth 3 -name 'Aquarium.app' -print -quit)" ./scripts/render_m6_package.sh
```
기대: `MATERIAL_COMPILE_FAILURES=0 AUTOINPUT_UNKNOWN=0 AUTOINPUT_BAD_DURATION=0 FRAMES=…`, 이어서 `VIDEO_OK` / `STILL_OK` / `COMPARE_OK`.
**`BEFORE` 파일 이름이 실제와 다르면 스크립트가 그 자리에서 죽는다** — 그것이 의도다(M4b에서 마일스톤을 자기 자신과 비교한 사고가 있었다). 실제 이름으로 `BEFORE=`를 준다.

- [ ] **Step 3: 눈으로 본다** — `docs/reviews/2026-09-21-m6-compare.png`(왼쪽 M5 에디터 빌드, 오른쪽 M6 패키지)와 `-m6-package.mp4`.
확인할 것: **회색 물체가 하나도 없는가**, 조명·안개·코스틱·산호 색이 왼쪽과 같아 보이는가, 물고기 표면이 뭉개지지 않았는가(쿡의 텍스처 압축). **본 것만 적는다.**

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add scripts/render_m6_package.sh docs/reviews/2026-09-21-m6-package.mp4 docs/reviews/2026-09-21-m6-scene.png docs/reviews/2026-09-21-m6-compare.png
git commit -m "$(cat <<'EOF'
chore: M6 패키지 화면 검토 산출물

에디터가 아니라 패키지 바이너리로 찍는다. 쿡은 셰이더·텍스처 압축·LOD를
바꾸므로 머티리얼 컴파일 0건 grep은 패키지 실행 로그에서 해야 의미가 있다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 10: 전체 회귀 — 규칙 103 / Automation 60

**Files:** 없음 (검증만)

M6은 `.uproject`와 설정 파일을 건드렸다. 회귀가 없는지 확인한다.

- [ ] **Step 1: 규칙 계층**

```bash
cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 103` (규약 1).

- [ ] **Step 2: 경고 없이 컴파일되는지**

```bash
cd /Users/hans/dev/aquarium && rm -rf build && cmake -S . -B build && cmake --build build -j 2>&1 | grep -ciE "warning:"
```
기대: `0`. (`-Wall -Wextra -Wshadow` 청정.)

- [ ] **Step 3: Unreal Automation** (규약 3 — 두 번 빌드)

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
for p in 1 2; do "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "^Result:"; done
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "Test Completed|Tests Failed" | tail -5
```
기대: 60개 통과, `Tests Failed` 0.

- [ ] **Step 4: 커밋 없음** — 검증 결과는 Task 11의 문서에 적는다.

---

### Task 11: 문서 마무리 — SETUP / TASK / 이 계획

**Files:** Modify `docs/SETUP.md`, Modify `docs/TASK.md`, Modify this plan

- [ ] **Step 1: `docs/SETUP.md`의 두 이월 항목을 닫는다**

(a) **UBT 앱 마무리(69~74행 주변의 "미해결" 문단).** 문단을 실제 결과로 교체한다.

```markdown
**M6에서 재확인(2026-09-21).** `RunUAT BuildCookRun` 단계에서 이 문제를 정면으로 다뤘다.
진단 기록은 [`docs/reviews/2026-09-21-m6-ubt.md`](reviews/2026-09-21-m6-ubt.md)에 있다.
결과: <재현되지 않음 / 원인 특정 / 원인 미특정 + 우회 적용> — 실제 결과로 쓴다.
패키징은 `scripts/package_mac.sh` 한 줄로 재현되며, 우회를 쓴 경우 그 우회도 스크립트 안에 있다.
```

(b) **MCP 서버의 배포 포함 여부(97행 부근).** 해당 문장을 교체한다.

```markdown
Claude Code 등록: `.mcp.json`의 `unreal` (HTTP, `http://127.0.0.1:8000/mcp`). 에디터가 꺼져 있으면
연결 실패가 정상이다. **배포 게임 포함 여부 — M6에서 확인하고 조치했다(2026-09-21).** 확인해 보니
`ModelContextProtocol`의 모듈 두 개가 `Type: Runtime`이라 **아무 조치 없이 패키징하면 배포 게임에
MCP 런타임이 들어간다.** `Aquarium.uproject`의 두 플러그인 항목에 `"TargetAllowList": ["Editor"]`를
추가해 에디터 타깃에서만 켜지게 했다(플러그인을 끄지 않은 이유는 `CaptureViewport` 검증 채널을
유지하기 위해서다). 배제는 주장이 아니라 단언으로 확인한다 — `scripts/verify_package.sh`가
패키지 번들 전체에서 `ModelContextProtocol`/`ToolsetRegistry` 문자열 0건을 확인하고, 같은 스캔이
에디터 플러그인 디렉터리에서는 빨간불이 나는 것을 대조군으로 확인한다. 실행 중
`lsof -nP -iTCP:8000 -sTCP:LISTEN` 0행도 함께 보지만, 그쪽은 약한 검사이므로 보조다.
```

(c) **M6 재현 명령 절을 추가한다** — 기존 "M1~M4c 재현 명령" 목록 끝에 이어 붙인다.

```bash
# 12. M6 패키징과 검증
CONFIG=Development ./scripts/package_mac.sh          # PACKAGE_OK config=Development
CONFIG=Shipping    ./scripts/package_mac.sh          # PACKAGE_OK config=Shipping
B=<패키지 출력 디렉터리>
./scripts/verify_package.sh "$B/Development/Mac/Aquarium.app" Development   # 개발 플래그 대조군
./scripts/verify_package.sh "$B/Shipping/Mac/Aquarium.app"    Shipping      # VERIFY_OK
./scripts/verify_privacy.sh auto "$B/Development/Mac/Aquarium.app"
APP="$B/Development/Mac/Aquarium.app" ./scripts/measure_package_perf.sh     # SRS 공식 측정 210초
APP="$B/Development/Mac/Aquarium.app" ./scripts/render_m6_package.sh        # VIDEO_OK/STILL_OK/COMPARE_OK
```

그리고 아래 주의를 함께 적는다:

```markdown
**성능 측정 길이 주의.** `measure_m2b_perf.sh`의 기본값은 `RUN_SEC=95`이고, 예열 30초를 빼면
실측 구간이 **65초**뿐이다. SRS 38행은 3분을 요구하므로 **공식 판정은 반드시
`measure_package_perf.sh`(기본 `RUN_SEC=210`)로 한다.** 이 스크립트는 `RUN_SEC`이
예열+180초 미만이면 그 자리에서 거부한다.

**Shipping 빌드는 헤드리스로 검증할 수 없다.** `#if !UE_BUILD_SHIPPING` 안의 개발 플래그가
없으므로 자동 입장도 자동 캡처도 불가능하다. 그것이 의도한 성질이며, Shipping 빌드의 게임 플레이는
사람만 확인할 수 있다.
```

- [ ] **Step 2: `docs/TASK.md`의 M6 행을 채우고 로드맵 상태를 갱신한다**

로드맵 표의 M6 행을 실제 결과로 바꾸고, 표 위 문단의 "남은 단계는 M6뿐이다"를 완료 상태로 고친다. 형식은 M5 행과 맞춘다 — 테스트 수, 산출물 경로, 성능 수치, 사용자 검토 상태.

- [ ] **Step 3: 이 계획의 "구현 중 발견한 후속 항목" 절을 채운다** — 최소한 다음은 반드시 기록한다.

- UBT `exit 65`의 최종 상태(해결 / 우회 / 미해결)와 우회를 썼다면 그 사실
- `TargetAllowList`가 의존 플러그인까지 실제로 막았는지
- Shipping 타깃 첫 컴파일에서 나온 오류가 있었는지
- 공식 성능 수치와 예비 레버 사용 여부
- 대조군 없이는 공허했을 뻔한 검사 목록과, 각 검사가 실제로 물린 방법
- 각 검사 중 **물게 만들 수 없었던 것**(있다면 그 사실 그대로)
- 사용자 수동 확인 5항목의 인계 상태

- [ ] **Step 4: 자체 검토** — 커밋 전에 셋을 본다.

```bash
cd /Users/hans/dev/aquarium
# (1) SRS 커버리지: F-01~F-14와 성능·데이터 절이 어딘가에서 판정되었는가
grep -n "F-0[1-9]\|F-1[0-4]" docs/TASK.md | tail -20
# (2) 미완성 표기 스캔
grep -rn "TBD\|TODO\|FIXME\|작성 예정\|비슷하게\|Task N" docs/superpowers/specs/2026-09-21-m6-packaging-design.md docs/superpowers/plans/2026-09-21-m6-packaging.md docs/reviews/2026-09-21-m6-*.md
# (3) 로컬 링크가 살아 있는가
grep -rhoE '\]\(([^)]+\.md)\)' docs/*.md docs/superpowers/*/*.md | sed -E 's/.*\((.*)\)/\1/' | sort -u
```
기대: (2)는 출력 없음. (3)의 각 경로가 실제로 존재하는지 확인한다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add docs/SETUP.md docs/TASK.md docs/superpowers/plans/2026-09-21-m6-packaging.md
git commit -m "$(cat <<'EOF'
docs: M6 결과 기록 — 패키징·성능 판정·출하 검증

SETUP.md의 두 이월 항목(UBT 앱 마무리 exit 65, MCP 서버의 배포 포함 여부)을
실제 결과로 닫고, M6 재현 명령과 성능 측정 길이 주의를 추가했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 12: 프로젝트 마감 요약과 사용자 수동 확인 인계

**Files:** Create `docs/reviews/2026-09-21-m6-summary.md`

**이 프로젝트의 마지막 산출물이다.** M6은 마지막 마일스톤이므로 이 문서가 "무엇을 만들었고 무엇이 검증되었고 무엇이 사람 손에 남았는가"의 최종 답이다.

- [ ] **Step 1: 모든 수치를 다시 확인한다** — 기억에서 적지 않는다.

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8"
echo "--- rules";      ctest --test-dir build 2>&1 | tail -2
echo "--- automation"; "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed.*Success"
echo "--- perf";       grep -nE "평균|판정|AVG_FPS|P95" docs/reviews/2026-09-21-m6-perf.md | head
echo "--- commits";    git log --oneline dd22ad5..HEAD
```

- [ ] **Step 2: 요약 문서를 쓴다** — `docs/reviews/2026-09-21-m6-summary.md`

```markdown
# Aquarium — 프로젝트 마감 요약 (M6)

작성일: 2026-09-21 · 브랜치 `feat/m6-packaging` · 커밋 <short sha>

## 무엇을 만들었는가

초등학생이 한국어 별명을 입력해 입장하면 다섯 종 중 한 마리를 무작위로 배정받고, 고정 카메라
앞의 산호초 바다에서 방향키로 자기 물고기를 몰며, 배경 물고기 36마리 사이에서 아무 물고기나
클릭해 놀래킬 수 있는 macOS 네이티브 게임. 별명은 세션 메모리에만 있고 디스크에 남지 않는다.

## 테스트 집계

| 계층 | 개수 | 상태 |
|---|---|---|
| 규칙 계층 (Catch2, C++17, 엔진 비의존) | **103** | 전부 통과, `-Wall -Wextra -Wshadow` 경고 0 |
| Unreal Automation | **60** | 전부 통과 |

## SRS 공식 성능 판정 — 패키지 개발 빌드

| 기준 | SRS 목표 | 실측 | 판정 |
|---|---|---|---|
| 평균 fps | ≥ 60 | … | … |
| p95 프레임 시간 | ≤ 22 ms | … | … |

측정 조건: 패키지 **Development** 빌드, 1920×1080, 30초 예열 후 **180초**, 2회 평균.
장면은 제어 1 + 배경 36 + 프롭 22로 **SRS가 적은 것(배경 19)보다 무겁다.**
예비 레버(`GridSizeZ` 128 → 64, +17.1 fps): **<사용 / 미사용>**.
상세는 [`2026-09-21-m6-perf.md`](2026-09-21-m6-perf.md).

**M1~M5의 모든 수치는 에디터 빌드였고 SRS 판정이 아니었다. 위 표가 이 프로젝트의 첫 공식 판정이다.**

## 출하 검증

| 항목 | 결과 | 근거 |
|---|---|---|
| MCP HTTP 서버 배제 | … | `verify_package.sh`의 `mcp-absent`. 같은 스캔이 에디터 디렉터리에서는 빨간불 |
| 개발 플래그 배제 | … | Shipping 0건 / Development <N>건 대조 |
| 별명 비영속 | … | 자동 실행: 모든 발견이 명령줄 에코. 수동 실행: 0건 |
| 에셋 출처 | … | 저장소 `git diff` + 배포물 목록 대조 + OFL 사본 동봉 |
| 머티리얼 | … | **패키지** 실행 로그 `Failed to compile Material` 0건 |
| 화면 | … | `2026-09-21-m6-compare.png` (왼쪽 M5, 오른쪽 M6 패키지) |

## 아이에게 주는 빌드

**Shipping 구성.** Development는 SRS 성능 측정과 자동 캡처를 위해 존재하며 개발 플래그를 갖고
있다. 패키지는 저장소 밖에 있고(재현 명령은 `docs/SETUP.md` 12단계) 커밋하지 않는다.

## 사용자가 직접 확인해야 하는 5가지 — 인계

헤드리스로는 **원리적으로** 확인할 수 없는 것들이다. 이것이 "빌드됨"과 "검증됨" 사이에 남은 전부다.

1. **창 포커스 (F-06, M3부터 이월)** — 창 모드로 입장해 방향키로 움직이는 중 Cmd-Tab으로 다른
   앱에 5초 이상 갔다 돌아온다. **물고기가 순간이동하지 않고 제자리에서 이어져야 하고, 방향키는
   떼어진 상태로 시작해야 한다.**
2. **나가기 버튼 호버 가드 (M5)** — 나가기 버튼 **바로 뒤로 물고기가 지나갈 때** 버튼을 클릭한다.
   **초기 화면으로 돌아가되, 버튼 뒤 물고기가 놀라 달아나면 안 된다.**
3. **한글 IME 입력 (패키지에서는 처음)** — 패키지를 띄워 한글 별명을 직접 타이핑해 입장한다.
   **조합 중 글자가 깨지지 않고 이름표에 입력한 그대로 나와야 한다.**
4. **별명 비영속 최종 판정** — 3번 세션을 끝낸 뒤 알려 주면
   `./scripts/verify_privacy.sh manual "<입력한 별명>"`을 돌린다. **0건이어야 한다.** 이것이
   SRS 52행에 대한 유일하게 정직한 답이다(자동 실행은 명령줄로 별명을 넣으므로 판정이 될 수 없다).
5. **패키지 화면 검토** — Shipping 패키지에서 30초 이상 놀아 본다. **회색 물체가 없고, 조명·안개·
   산호가 M5 클립과 같아 보여야 한다.** Shipping은 개발 플래그가 없어 자동 캡처가 불가능하므로
   이 확인은 사람만 할 수 있다.

## 남은 제약 — 숨기지 않는다

- **Shipping 빌드의 게임 플레이는 자동으로 검증되지 않는다.** 개발 플래그가 없다는 것이 바로
  그 이유이고, 그것은 결함이 아니라 의도다.
- **역투영 경로(클릭)는 Automation으로 검증되지 않는다** — `-nullrhi`에 게임 뷰포트가 없다.
  1차 방어선은 M5의 클릭 로그 CSV다.
- **포트 검사는 약한 검사다** — MCP 배제의 1차 근거는 파일 스캔이다.
- **4K 품질 모드는 범위 밖이다** — SRS가 별도 검증이라고 적었다.
- M4a~M5에서 이월된 시각 항목(비늘 이방성, TubeCoral 실루엣, 동종 무리 육안 확정 등)은 M6이
  건드리지 않았다. 각 마일스톤 계획의 "구현 중 발견한 후속 항목" 절에 그대로 있다.
```

- [ ] **Step 3: 커밋**

```bash
cd /Users/hans/dev/aquarium
git add docs/reviews/2026-09-21-m6-summary.md
git commit -m "$(cat <<'EOF'
docs: 프로젝트 마감 요약 (M6)

테스트 집계, SRS 공식 성능 판정, 출하 검증 결과, 에셋 출처 확인,
그리고 헤드리스로 확인할 수 없어 사용자에게 넘기는 5항목을 정리했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

- [ ] **Step 4: 사용자에게 보고한다** — 마감 요약의 "사용자가 직접 확인해야 하는 5가지"를 **그대로** 전한다. 각 항목은 한 줄이고, 무엇을 하고 무엇을 보면 통과인지가 들어 있어야 한다. **하지 않은 확인을 했다고 적지 않는다.**

---

## 구현 중 발견한 후속 항목

(Task 11 Step 3에서 채운다. 비워 둔 채 M6을 닫지 않는다.)
