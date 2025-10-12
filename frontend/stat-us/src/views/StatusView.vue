<script setup lang="ts">
import { ref, onMounted, onBeforeUnmount } from 'vue'
import { api } from '@/stores/auth'

const loading = ref(false)
const error = ref('')
const snapshot = ref<Record<string, unknown> | null>(null)

function getStr(key: string, d = ''): string {
  const v = snapshot.value?.[key]
  return typeof v === 'string' ? v : d
}

function getNum(key: string, d = 0): number {
  const v = snapshot.value?.[key]
  if (typeof v === 'number') return v
  if (typeof v === 'string') {
    const n = Number(v)
    return Number.isFinite(n) ? n : d
  }
  return d
}

function formatMs(ms: number): string {
  const sec = Math.floor(ms / 1000)
  const h = Math.floor(sec / 3600)
  const m = Math.floor((sec % 3600) / 60)
  const s = sec % 60
  const parts: string[] = []
  if (h) parts.push(`${h}시간`)
  if (m) parts.push(`${m}분`)
  parts.push(`${s}초`)
  return parts.join(' ')
}

function toLocal(ts: string): string {
  try {
    return new Date(ts).toLocaleString()
  } catch {
    return ts
  }
}

let es: EventSource | null = null

async function load() {
  loading.value = true
  error.value = ''
  try {
    const res = await api.get('/api/status/latest')
    snapshot.value = res.data?.result || {}
  } catch (e: unknown) {
    error.value = e instanceof Error ? e.message : String(e)
  } finally {
    loading.value = false
  }
}

function connectSSE() {
  try {
    // 동일 오리진 프록시가 아니면, 백엔드에 CORS + credentials 정책이 필요합니다.
    es = new EventSource((api.defaults.baseURL || '') + '/api/status/stream', {
      withCredentials: true,
    })
    es.addEventListener('ping', () => {})
    es.addEventListener('status', (ev: MessageEvent) => {
      try {
        const data = JSON.parse(ev.data)
        snapshot.value = data
      } catch {
        // ignore malformed events
      }
    })
    es.onerror = () => {
      // 자동 재연결(브라우저 기본) 대기
    }
  } catch (e: unknown) {
    console.warn('SSE 연결 실패', e)
    // 폴백: 수동 새로고침만 유지
  }
}

onMounted(() => {
  load()
  connectSSE()
})
onBeforeUnmount(() => {
  if (es) {
    es.close()
    es = null
  }
})
</script>

<template>
  <div>
    <h2>현재 상태</h2>
    <div style="margin: 8px 0">
      <button @click="load" :disabled="loading">새로고침</button>
    </div>
    <p v-if="loading">불러오는 중…</p>
    <p v-if="error" style="color: #c33">{{ error }}</p>
    <pre v-if="snapshot" style="background: #f7f7f7; padding: 12px; overflow: auto">{{
      JSON.stringify(snapshot, null, 2)
    }}</pre>
    <p v-else>데이터가 없습니다.</p>

    <div
      v-if="snapshot"
      style="margin-top: 16px; border: 1px solid #eee; padding: 12px; border-radius: 6px"
    >
      <h3 style="margin-top: 0">가독형 요약</h3>
      <div class="kv">
        <div>
          <span class="k">시간</span
          ><span class="v">{{ toLocal(getStr('timestamp', '')) || '—' }}</span>
        </div>
        <div>
          <span class="k">브랜치</span><span class="v">{{ getStr('branch', '—') }}</span>
        </div>
        <div>
          <span class="k">작업 폴더</span><span class="v">{{ getStr('workspaceRoot', '—') }}</span>
        </div>
        <div>
          <span class="k">파일 경로</span><span class="v">{{ getStr('filePath', '—') }}</span>
        </div>
        <div>
          <span class="k">언어</span><span class="v">{{ getStr('languageId', '—') }}</span>
        </div>
        <div>
          <span class="k">상태</span
          ><span class="v">{{
            snapshot?.isIdle === false ? '활성' : snapshot?.isIdle === true ? '비활성' : '—'
          }}</span>
        </div>
        <div>
          <span class="k">비활성 경과</span
          ><span class="v">{{ formatMs(getNum('idleForMs', 0)) }}</span>
        </div>
        <div>
          <span class="k">세션 전체</span
          ><span class="v">{{ formatMs(getNum('sessionMs', 0)) }}</span>
        </div>
        <div>
          <span class="k">세션 활동</span
          ><span class="v">{{ formatMs(getNum('sessionActiveMs', 0)) }}</span>
        </div>
        <div>
          <span class="k">키스트로크</span
          ><span class="v">{{ getNum('keystrokes', 0).toLocaleString() }}</span>
        </div>
        <div>
          <span class="k">VS Code</span><span class="v">{{ getStr('vscodeVersion', '—') }}</span>
        </div>
        <div>
          <span class="k">확장 버전</span
          ><span class="v">{{ getStr('extensionVersion', '—') }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
button {
  padding: 6px 12px;
}
/* 1열(한 줄) 형태: 라벨: 값 */
.kv {
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.kv > div {
  display: block;
}
.kv .k {
  color: #444;
  font-weight: 600;
}
.kv .k::after {
  content: ' : ';
  margin: 0 6px 0 4px;
  color: #999;
  font-weight: normal;
}
.kv .v {
  font-weight: 600;
}
</style>
