#!/usr/bin/env bash
# gen_compile_commands.sh (macOS 기본 bash 3.2 호환)
# 1) Gradle 빌드(:app:assembleDebug)
# 2) .cxx 아래에서 ABI 포함 경로 우선 탐색 → 최신 mtime 파일 선택
# 3) 루트에 compile_commands.json 심볼릭 링크 생성
#    사용: [DEBUG=1] [ABI=arm64-v8a] [APP_PATH=android/app] ./tools/gen_compile_commands.sh

set -eu  # (bash 3.2 환경 고려)

# 항상 저장소 루트에서 동작하도록 이동
repo_root="$(cd "$(dirname "$0")/.." && pwd -P)"
cd "$repo_root"

ABI="${ABI:-arm64-v8a}"
APP_PATH="${APP_PATH:-android/app}"
DEBUG="${DEBUG:-0}"

log(){ echo "[cc-link] $*"; }
dbg(){ [ "$DEBUG" = "1" ] && echo "[cc-link:dbg] $*"; }

mtime() {
  # mac: stat -f %m, linux: stat -c %Y
  local p="$1"
  if stat -f %m "$p" >/dev/null 2>&1; then
    stat -f %m "$p"
  else
    stat -c %Y "$p"
  fi
}

log "Running Gradle build to ensure NDK outputs exist..."
if (cd "$APP_PATH/.." && ./gradlew :app:assembleDebug); then
  log "Gradle build: OK"
else
  log "Gradle build: FAILED (will try to reuse existing .cxx outputs if any)"
fi

root="$APP_PATH/.cxx"
[ -d "$root" ] || { log "❌ Not found: $root"; exit 1; }

# 모든 compile_commands.json 수집 (공백/특수문자 안전)
all_cc=()
while IFS= read -r -d '' f; do
  all_cc+=("$f")
done < <(find "$root" -type f -name compile_commands.json -print0 2>/dev/null || true)

[ "${#all_cc[@]}" -gt 0 ] || { log "❌ compile_commands.json not found under $root"; exit 1; }

# 우선순위 분류: 1) .../debug/.../<ABI>/..., 2) .../<ABI>/..., 3) 그 외
pri=(); sec=(); tri=()
for p in "${all_cc[@]}"; do
  if [[ "$p" == *"/debug/"* && "$p" == *"/$ABI/"* ]]; then
    pri+=("$p")
  elif [[ "$p" == *"/$ABI/"* ]]; then
    sec+=("$p")
  else
    tri+=("$p")
  fi
done

pick_newest() {
  local best=""; local best_m=0
  for f in "$@"; do
    [ -f "$f" ] || continue
    local m; m=$(mtime "$f" 2>/dev/null || echo 0)
    dbg "cand: $f (mtime=$m)"
    if [ "$m" -gt "$best_m" ]; then
      best_m="$m"; best="$f"
    fi
  done
  echo "$best"
}

CANDIDATE="$(pick_newest "${pri[@]}")"
[ -n "$CANDIDATE" ] || CANDIDATE="$(pick_newest "${sec[@]}")"
[ -n "$CANDIDATE" ] || CANDIDATE="$(pick_newest "${tri[@]}")"

if [ -z "$CANDIDATE" ] || [ ! -f "$CANDIDATE" ]; then
  log "❌ compile_commands.json을 찾지 못했습니다."
  log "   Hint) find $root -name compile_commands.json -print"
  exit 1
fi

ln -sf "$CANDIDATE" ./compile_commands.json
log "✅ Linked ./compile_commands.json -> $CANDIDATE"

if [ ! -f ".clangd" ]; then
  cat <<'CLANGD'
[cc-link] ℹ️ .clangd 템플릿:

CompileFlags:
  CompilationDatabase: .
  Add: [-D__ANDROID__, -DANDROID, -std=c++17]
CLANGD
fi