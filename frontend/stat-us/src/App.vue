<script setup lang="ts"></script>

<template>
  <nav style="display: flex; gap: 8px; margin-bottom: 12px">
    <RouterLink to="/">Home</RouterLink>
    <RouterLink to="/login">Login</RouterLink>
    <RouterLink to="/signup">Signup</RouterLink>
    <RouterLink to="/protected">Protected</RouterLink>
    <RouterLink to="/profile">Profile</RouterLink>
    <RouterLink to="/status">Status</RouterLink>
  </nav>
  <RouterView />
</template>

<style scoped>
nav a {
  text-decoration: none;
}
</style>
