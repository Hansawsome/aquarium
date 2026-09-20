# 에셋 출처와 제작 기록

무료라는 이유만으로 원본 재배포를 허용한다고 가정하지 않는다.

## 도입한 외부 에셋

| 에셋 | 저작자 | 원본 URL | 다운로드일 | 라이선스 | 출처 표기 | 수정 | 게임 배포 | 공개 저장소 재배포 | 로컬 경로 |
|---|---|---|---|---|---|---|---|---|---|
| Poly Haven `coast_sand_01` (모래 텍스처 2K: diffuse, normal GL, roughness) | Rob Tuytel | https://polyhaven.com/a/coast_sand_01 | 2026-09-20 | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Textures from Poly Haven") | 없음 | 가능 | 가능 | `assets/textures/sand/` |
| Noto Sans KR Regular (한국어 UI 폰트) | Google / Adobe (Noto CJK 프로젝트) | https://github.com/notofonts/noto-cjk | 2026-09-20 | SIL OFL 1.1 (`assets/fonts/OFL.txt`) | 불필요 (OFL: 폰트 단독 판매 금지, 임베드·재배포 허용) | 없음 | 가능 | 가능 | `assets/fonts/` |
| Poly Haven `boulder_01` (바위 모델, FBX 1K: diffuse/normal GL/roughness/AO) | Rico Cilliers | https://polyhaven.com/a/boulder_01 | 2026-09-20 | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Models from Poly Haven") | 없음 | 가능 | 가능 | `assets/models/rocks/boulder_01/` |
| Poly Haven `rock_07` (바위 모델, FBX 1K: diffuse/normal GL/roughness/AO) | Jenelle van Heerden | https://polyhaven.com/a/rock_07 | 2026-09-20 | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Models from Poly Haven") | 없음 | 가능 | 가능 | `assets/models/rocks/rock_07/` |
| Poly Haven `rock_09` (바위 모델, FBX 1K: diffuse/normal GL/roughness/AO) | Jenelle van Heerden | https://polyhaven.com/a/rock_09 | 2026-09-20 | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Models from Poly Haven") | 없음 | 가능 | 가능 | `assets/models/rocks/rock_09/` |

## 직접 제작

| 에셋 | 제작 도구 | 생성 스크립트 | 산출물 | 외부 텍스처 |
|---|---|---|---|---|
| 블루탱 (BlueTang) | Blender 5.2.2 LTS | `assets/blender/make_bluetang.py` | `assets/blender/BlueTang.blend`, `assets/blender/export/BlueTang.fbx`, `T_BlueTang_BaseColor.png` (2048², 절차적 노드 베이크) | 없음 |
| 흰동가리돔 (Clownfish) | Blender 5.2.2 LTS | `assets/blender/make_clownfish.py` (+ `fishlib.py`) | `assets/blender/Clownfish.blend`, `assets/blender/export/Clownfish.fbx`, `T_Clownfish_BaseColor.png` (2048², 절차적 베이크), `preview_clownfish.png` | 없음 |
| 옐로탱 (YellowTang) | Blender 5.2.2 LTS | `assets/blender/make_yellowtang.py` (+ `fishlib.py`) | `assets/blender/YellowTang.blend`, `assets/blender/export/YellowTang.fbx`, `T_YellowTang_BaseColor.png` (2048², 절차적 베이크), `preview_yellowtang.png` | 없음 |
| 나비고기 (Butterflyfish) | Blender 5.2.2 LTS | `assets/blender/make_butterflyfish.py` (+ `fishlib.py`) | `assets/blender/Butterflyfish.blend`, `assets/blender/export/Butterflyfish.fbx`, `T_Butterflyfish_BaseColor.png` (2048², 절차적 베이크), `preview_butterflyfish.png` | 없음 |
| 자리돔 (Damselfish) | Blender 5.2.2 LTS | `assets/blender/make_damselfish.py` (+ `fishlib.py`) | `assets/blender/Damselfish.blend`, `assets/blender/export/Damselfish.fbx`, `T_Damselfish_BaseColor.png` (2048², 절차적 베이크), `preview_damselfish.png` | 없음 |
| 가지 산호 (BranchCoral) | Blender 5.2.2 LTS | `assets/blender/make_corals.py` (+ `fishlib.py`) | `assets/blender/BranchCoral.blend`, `assets/blender/export/BranchCoral.fbx`, `T_BranchCoral_BaseColor.png` (1024², 절차적 베이크), 공용 `preview_corals.png` | 없음 |
| 판상 산호 (PlateCoral) | Blender 5.2.2 LTS | `assets/blender/make_corals.py` (+ `fishlib.py`) | `assets/blender/PlateCoral.blend`, `assets/blender/export/PlateCoral.fbx`, `T_PlateCoral_BaseColor.png` (1024², 절차적 베이크), 공용 `preview_corals.png` | 없음 |
| 뇌 산호 (BrainCoral) | Blender 5.2.2 LTS | `assets/blender/make_corals.py` (+ `fishlib.py`) | `assets/blender/BrainCoral.blend`, `assets/blender/export/BrainCoral.fbx`, `T_BrainCoral_BaseColor.png` (1024², 절차적 베이크), 공용 `preview_corals.png` | 없음 |

블루탱 비고: `assets/blender/export/preview_bluetang.png`(EEVEE 미리보기 렌더)도 스크립트 산출물이다. `.blend`/`.fbx`/`preview_bluetang.png`는 재빌드마다 바이트가 달라진다(타임스탬프·세션 데이터, EEVEE 노이즈). 베이크된 `T_BlueTang_BaseColor.png`는 실행마다 UV 솔기 부근 수십 서브픽셀이 최대 1/255 흔들린다(2026-09-20 측정). 재현성 검사는 `scripts/compare_texture.py`(픽셀당 최대 델타 ≤ 2/255, 달라진 픽셀 ≤ 0.01%)로 한다. Subdivision 모디파이어는 의도적으로 생략했다(shade_smooth만 적용).

산호 비고: 산호 3종은 정적 프롭이라 아마추어·애니메이션이 없다(메시 + 베이크 베이스컬러만). `make_corals.py` 한 스크립트가 세 종을 모두 생성하며, 형상은 고정 시드(`random.Random`)와 결정론적 수식으로 재현된다. 미리보기는 세 산호를 x = -120 / 0 / 120에 늘어놓은 공용 `assets/blender/export/preview_corals.png` 한 장이다.

## 도입 시 필수 기록

각 에셋마다 이름, 저작자, 원본 URL, 다운로드 날짜, 라이선스 URL/사본, 출처 표기 문구, 수정 여부, 게임 배포 가능 여부, 공개 저장소에 원본 재배포 가능 여부, 로컬 경로를 기록한다.

직접 제작한 경우 제작 도구, 생성 스크립트 또는 원본 파일 경로, 사용한 외부 텍스처의 출처를 기록한다. 물고기용 한국어 폰트도 같은 기준으로 관리한다.

## 조사 후보

- 바위·표면 텍스처: Poly Haven의 개별 CC0 에셋. 모래 텍스처는 `coast_sand_01`, 바위 모델은 `boulder_01`/`rock_07`/`rock_09`로 선정 완료(위 "도입한 외부 에셋" 참고).
- 물고기·산호: 적합한 무료 모델을 검토하거나 Blender로 직접 제작한다.
- 유영 애니메이션: 출처가 분명한 무료 리깅/애니메이션 또는 Blender 자체 제작.

참고: [Poly Haven 에셋 라이선스](https://polyhaven.com/license). 에셋과 사이트 콘텐츠/API 이용 조건은 구분한다.
