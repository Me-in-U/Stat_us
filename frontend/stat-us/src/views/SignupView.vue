<script setup lang="ts">
import { ref } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { useRouter } from 'vue-router'

const email = ref('')
const password = ref('')
const nickname = ref('')
const store = useAuthStore()
const router = useRouter()

const onSubmit = async () => {
  await store.signup(email.value, password.value, nickname.value)
  router.push('/')
}
</script>

<template>
  <div>
    <h1>Signup</h1>
    <form @submit.prevent="onSubmit">
      <input v-model="email" placeholder="email" />
      <input v-model="password" type="password" placeholder="password" />
      <input v-model="nickname" placeholder="nickname" />
      <button type="submit">Signup</button>
    </form>
  </div>
  <router-link to="/login">Go to Login</router-link>
</template>
