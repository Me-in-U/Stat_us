<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api } from '@/stores/auth'

const apiKey = ref<string>('')
const loading = ref(false)
const message = ref('')

async function loadProfile() {
  try {
    const res = await api.get('/api/member/me')
    apiKey.value = res.data?.result?.apiKey || ''
  } catch (e) {
    console.error(e)
  }
}

async function issueNewKey() {
  loading.value = true
  message.value = ''
  try {
    const res = await api.post('/api/member/api-key/issue')
    apiKey.value = res.data?.result?.apiKey || ''
    message.value = '새 API 키가 발급되었습니다. 안전한 곳에 보관하세요.'
  } catch (e) {
    console.error(e)
    message.value = '발급 중 오류가 발생했습니다.'
  } finally {
    loading.value = false
  }
}

async function copyKey() {
  if (!apiKey.value) return
  await navigator.clipboard.writeText(apiKey.value)
  message.value = '복사되었습니다.'
}

onMounted(loadProfile)
</script>

<template>
  <div>
    <h2>내 프로필</h2>
    <div style="margin: 12px 0">
      <div>API 키</div>
      <input :value="apiKey" readonly style="width: 100%; max-width: 480px" />
      <div style="margin-top: 8px; display: flex; gap: 8px">
        <button @click="copyKey" :disabled="!apiKey">복사</button>
        <button @click="issueNewKey" :disabled="loading">새 키 발급</button>
      </div>
      <p v-if="message" style="color: #555">{{ message }}</p>
      <p style="color: #a33">경고: 새 키 발급 시 기존 키는 즉시 사용할 수 없게 됩니다.</p>
    </div>
  </div>
</template>

<style scoped>
button {
  padding: 6px 12px;
}
</style>
