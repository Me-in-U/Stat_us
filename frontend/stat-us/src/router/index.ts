import { createRouter, createWebHistory } from 'vue-router'
import LoginView from '@/views/LoginView.vue'
import SignupView from '@/views/SignupView.vue'
import { useAuthStore } from '@/stores/auth'
import ProfileView from '@/views/ProfileView.vue'
import StatusView from '@/views/StatusView.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    { path: '/', name: 'home', component: { template: '<div>Home</div>' } },
    { path: '/login', name: 'login', component: LoginView },
    { path: '/signup', name: 'signup', component: SignupView },
    {
      path: '/protected',
      name: 'protected',
      component: { template: '<div>Protected</div>' },
      meta: { requiresAuth: true },
    },
    { path: '/profile', name: 'profile', component: ProfileView, meta: { requiresAuth: true } },
    { path: '/status', name: 'status', component: StatusView, meta: { requiresAuth: true } },
  ],
})

router.beforeEach((to) => {
  const store = useAuthStore()
  if (to.meta.requiresAuth && !store.accessToken) {
    return { name: 'login' }
  }
})

export default router
