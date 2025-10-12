import { defineStore } from 'pinia'
import axios from 'axios'
import type { AxiosError, AxiosResponse, InternalAxiosRequestConfig } from 'axios'
declare module 'axios' {
  // augment to allow a private _retry flag
  export interface AxiosRequestConfig {
    _retry?: boolean
  }
}

const api = axios.create({
  baseURL: import.meta.env.VITE_API_BASE_URL || 'http://localhost:8080',
  withCredentials: true,
})

// refresh interceptor
api.interceptors.response.use(
  (res: AxiosResponse) => res,
  async (error: AxiosError) => {
    const original = error.config
    if (error.response?.status === 401 && original && !original._retry) {
      original._retry = true
      try {
        const store = useAuthStore()
        await store.refresh()
        if (store.accessToken) {
          original.headers = original.headers || {}
          original.headers['Authorization'] = `Bearer ${store.accessToken}`
        }
        return api(original)
      } catch {
        const store = useAuthStore()
        store.clear()
      }
    }
    return Promise.reject(error instanceof Error ? error : new Error(String(error)))
  },
)

export const useAuthStore = defineStore('auth', {
  state: () => ({
    accessToken: '' as string,
    user: null as null | { id: number; email: string; nickname: string },
  }),
  actions: {
    setToken(token: string) {
      this.accessToken = token
    },
    clear() {
      this.accessToken = ''
      this.user = null
    },
    async signup(email: string, password: string, nickname: string) {
      const res = await api.post('/api/auth/signup', { email, password, nickname })
      const data = res.data?.result
      this.user = { id: data.memberId, email: data.email, nickname: data.nickname }
      this.accessToken = data.accessToken
    },
    async login(email: string, password: string) {
      const res = await api.post('/api/auth/login', { email, password })
      const data = res.data?.result
      this.user = { id: data.memberId, email: data.email, nickname: data.nickname }
      this.accessToken = data.accessToken
    },
    async refresh() {
      const res = await api.post('/api/auth/refresh')
      const data = res.data?.result
      this.user = { id: data.memberId, email: data.email, nickname: data.nickname }
      this.accessToken = data.accessToken
    },
    async logout() {
      await api.post('/api/auth/logout')
      this.clear()
    },
  },
})

// attach Authorization for requests
api.interceptors.request.use((config: InternalAxiosRequestConfig) => {
  const store = useAuthStore()
  if (store.accessToken) {
    config.headers = config.headers || {}
    config.headers['Authorization'] = `Bearer ${store.accessToken}`
  }
  return config
})

export { api }
