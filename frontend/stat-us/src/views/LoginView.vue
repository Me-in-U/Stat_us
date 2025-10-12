<script setup lang="ts">
import { ref } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { useRouter } from 'vue-router'

const email = ref('')
const password = ref('')
const store = useAuthStore()
const router = useRouter()

const onSubmit = async () => {
  await store.login(email.value, password.value)
  router.push('/')
}
</script>

<template>
  <div>
    <h1>Login</h1>
    <form @submit.prevent="onSubmit">
      <input v-model="email" placeholder="email" />
      <input v-model="password" type="password" placeholder="password" />
      <button type="submit">Login</button>
    </form>
  </div>
  <p><router-link to="/signup">Go to Signup</router-link></p>
  <pre>user: {{ store.user }}</pre>
  <pre>token: {{ store.accessToken.slice(0, 12) }}...</pre>
  <button @click="store.logout">Logout</button>
  <button @click="store.refresh">Refresh Token</button>
  <router-link to="/">Home</router-link>
  <router-link to="/login">Login</router-link>
  <router-link to="/signup">Signup</router-link>
  <router-link to="/protected">Protected</router-link>
  <router-view />
</template>
