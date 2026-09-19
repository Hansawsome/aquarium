# 에셋 출처와 제작 기록

현재 도입한 외부 에셋은 없다. 무료라는 이유만으로 원본 재배포를 허용한다고 가정하지 않는다.

## 직접 제작

| 에셋 | 제작 도구 | 생성 스크립트 | 산출물 | 외부 텍스처 |
|---|---|---|---|---|
| 블루탱 (BlueTang) | Blender 5.2.2 LTS | `assets/blender/make_bluetang.py` | `assets/blender/BlueTang.blend`, `assets/blender/export/BlueTang.fbx`, `T_BlueTang_BaseColor.png` (2048², 절차적 노드 베이크) | 없음 |

블루탱 비고: `assets/blender/export/preview.png`(EEVEE 미리보기 렌더)도 스크립트 산출물이다. `.blend`/`.fbx`/`preview.png`는 재빌드마다 바이트가 달라진다(타임스탬프·세션 데이터, EEVEE 노이즈). 베이크된 `T_BlueTang_BaseColor.png`는 바이트 동일하다. Subdivision 모디파이어는 의도적으로 생략했다(shade_smooth만 적용).

## 도입 시 필수 기록

각 에셋마다 이름, 저작자, 원본 URL, 다운로드 날짜, 라이선스 URL/사본, 출처 표기 문구, 수정 여부, 게임 배포 가능 여부, 공개 저장소에 원본 재배포 가능 여부, 로컬 경로를 기록한다.

직접 제작한 경우 제작 도구, 생성 스크립트 또는 원본 파일 경로, 사용한 외부 텍스처의 출처를 기록한다. 물고기용 한국어 폰트도 같은 기준으로 관리한다.

## 조사 후보

- 바위·모래·표면 텍스처: Poly Haven의 개별 CC0 에셋. 실제 사용할 파일은 아직 선정하지 않았다.
- 물고기·산호: 적합한 무료 모델을 검토하거나 Blender로 직접 제작한다.
- 유영 애니메이션: 출처가 분명한 무료 리깅/애니메이션 또는 Blender 자체 제작.

참고: [Poly Haven 에셋 라이선스](https://polyhaven.com/license). 에셋과 사이트 콘텐츠/API 이용 조건은 구분한다.
